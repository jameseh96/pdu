#include "compute.h"

#include "pdu/io.h"
#include "pdu/query.h"

#include <boost/variant.hpp>
#include <gsl/gsl-lite.hpp>

#include <stack>

void execute(Operation op, std::stack<double>& stack) {
    // every operation takes at least one argument
    auto arg1 = stack.top();
    stack.pop();

    if (op == Operation::Unary_Minus) {
        arg1 = -arg1;
        stack.push(arg1);
        return;
    }

    // args appear in opposite order on stack
    auto arg0 = stack.top();
    stack.pop();

    switch (op) {
    case Operation::Add:
        stack.push(arg0 + arg1);
        return;
    case Operation::Subtract:
        stack.push(arg0 - arg1);
        return;
    case Operation::Divide:
        if (arg1 == 0.0) {
            throw std::domain_error("Division by zero");
        }
        stack.push(arg0 / arg1);
        return;
    case Operation::Multiply:
        stack.push(arg0 * arg1);
        return;
    case Operation::Unary_Minus:;
        // handled above
    }

    throw std::runtime_error("Unknown expression operation: " +
                             std::to_string(int(op)));
}

ExpressionIterator::ExpressionIterator(
        std::vector<boost::variant<Operation, CrossIndexSeries, double>> ops) {
    for (const auto& variant : ops) {
        boost::apply_visitor([this](const auto& value) { add(value); },
                             variant);
    }
    previousValues.resize(iterators.size());
    increment();
}

void ExpressionIterator::increment() {
    // update the latest values for every iterator
    int64_t newTimestamp = std::numeric_limits<int64_t>::max();
    for (int i = 0; i < iterators.size(); i++) {
        auto& iter = iterators[i];
        if (iter == end(iter)) {
            continue;
        }

        const auto& sample = *iter;
        if (sample.timestamp == lastTimestamp) {
            // we've evaluated this time series at this time before
            // advance to the next sample
            ++iter;
        }

        // the iterator may _now_ be at it's end
        if (iter == end(iter)) {
            continue;
        }

        newTimestamp = std::min(newTimestamp, iter->timestamp);
        // track the latest value. Once a time series runs out of samples
        // we want keep using the last seen value
        previousValues[i] = iter->value;
    }

    if (newTimestamp == std::numeric_limits<int64_t>::max()) {
        finished = true;
        return;
    }
    lastTimestamp = newTimestamp;
    evaluate();
}

void ExpressionIterator::add(Operation op) {
    operations.emplace_back(op);
}
void ExpressionIterator::add(const CrossIndexSeries& cis) {
    iterators.push_back(cis.sampleIterator);
    operations.emplace_back(SeriesRef(iterators.size() - 1));
}
void ExpressionIterator::add(double constant) {
    operations.emplace_back(constant);
}

void ExpressionIterator::evaluate() {
    // evaluate the expression on the current iterator values
    for (const auto& variant : operations) {
        boost::apply_visitor([this](auto value) { evaluate_single(value); },
                             variant);
    }
    Expects(stack.size() == 1);
    currentResult = {lastTimestamp, stack.top()};
    stack.pop();
}

void ExpressionIterator::evaluate_single(Operation op) {
    execute(op, stack);
}
void ExpressionIterator::evaluate_single(SeriesRef op) {
    stack.push(previousValues[op]);
}
void ExpressionIterator::evaluate_single(double op) {
    stack.push(op);
}

Expression::Expression(CrossIndexSeries cis) {
    operations.emplace_back(std::move(cis));
}

Expression::Expression(double constantValue) {
    operations.emplace_back(constantValue);
}

ResamplingIterator Expression::resample(
        std::chrono::milliseconds interval) const {
    return {begin(), interval};
}

Expression Expression::unary_minus() const {
    auto copy = *this;
    copy.operations.emplace_back(Operation::Unary_Minus);
    return copy;
}

Expression Expression::operator-=(const Expression& other) {
    copy_operations_from(other);
    operations.emplace_back(Operation::Subtract);
    return *this;
}
Expression Expression::operator+=(const Expression& other) {
    copy_operations_from(other);
    operations.emplace_back(Operation::Add);
    return *this;
}
Expression Expression::operator/=(const Expression& other) {
    copy_operations_from(other);
    operations.emplace_back(Operation::Divide);
    return *this;
}
Expression Expression::operator*=(const Expression& other) {
    copy_operations_from(other);
    operations.emplace_back(Operation::Multiply);
    return *this;
}

void Expression::copy_operations_from(const Expression& other) {
    operations.insert(
            operations.end(), other.operations.begin(), other.operations.end());
}

Expression operator-(const Expression& expr) {
    return expr.unary_minus();
}
Expression operator+(const Expression& expr) {
    return expr;
}

Expression operator-(Expression a, const Expression& b) {
    a -= b;
    return a;
}
Expression operator+(Expression a, const Expression& b) {
    a += b;
    return a;
}
Expression operator/(Expression a, const Expression& b) {
    a /= b;
    return a;
}
Expression operator*(Expression a, const Expression& b) {
    a *= b;
    return a;
}

double lerp(double start, double end, double ratio) {
    return end*ratio + start * (1 - ratio);
}

Sample lerpSamples(const Sample& start, const Sample& end, int64_t timestamp) {
    double fraction = (double(timestamp) - start.timestamp) /
                      (double(end.timestamp) - start.timestamp);
    Sample res;
    res.timestamp = timestamp;
    res.value = lerp(start.value, end.value, fraction);
    return res;
}

ResamplingIterator::ResamplingIterator(ExpressionIterator iterator,
                                       std::chrono::milliseconds interval)
    : itr(std::move(iterator)), interval(interval.count()) {
    if (itr != end(itr)) {
        nextTimestamp = itr->timestamp + this->interval;
        prevSample = *itr;
        nextSample = *itr;
        computedSample = *itr;
    }
}

void ResamplingIterator::increment() {
    while (nextTimestamp > nextSample.timestamp) {
        ++itr;
        if (itr == end(itr)) {
            return;
        }
        prevSample = nextSample;
        nextSample = *itr;
    }

    computedSample = lerpSamples(prevSample, nextSample, nextTimestamp);
    nextTimestamp = itr->timestamp + this->interval;
}