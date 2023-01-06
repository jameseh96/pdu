#include "expression.h"

#include "pdu/block.h"
#include "pdu/filter.h"

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
        std::vector<ExpressionVariant> ops) {
    for (const auto& variant : ops) {
        boost::apply_visitor([this](const auto& value) { add(value); },
                             variant);
    }
    increment();
}

template <class F, class... Args>
void apply_to_each(F&& func, Args&&... args) {
    (func(args), ...);
}

void ExpressionIterator::increment() {
    // update the latest values for every iterator
    int64_t newTimestamp = std::numeric_limits<int64_t>::max();
    apply_to_each(
            [&](auto& iterValues) {
                for (int i = 0; i < iterValues.size(); i++) {
                    auto& [iter, value] = iterValues[i];
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
                    // track the latest value. Once a time series runs out of
                    // samples we want keep using the last seen value
                    value = iter->value;
                }
            },
            subiterators.series,
            subiterators.rate,
            subiterators.resample);

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
    auto& store = subiterators.get<CrossIndexSampleIterator>();
    store.emplace_back(cis.getSamples(), 0.0);
    operations.emplace_back(Ref<CrossIndexSampleIterator>{store.size() - 1});
}

void ExpressionIterator::add(const RateExpression& subexpr) {
    auto& store = subiterators.get<IRateIterator>();
    store.emplace_back(IRateIterator(subexpr.expr.begin(), subexpr.monotonic),
                       0.0);
    operations.emplace_back(Ref<IRateIterator>{store.size() - 1});
}

void ExpressionIterator::add(const ResampleExpression& subexpr) {
    auto& store = subiterators.get<ResamplingIterator>();
    store.emplace_back(
            ResamplingIterator(subexpr.expr.begin(), subexpr.interval), 0.0);
    operations.emplace_back(Ref<ResamplingIterator>{store.size() - 1});
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

void ExpressionIterator::evaluate_single(double op) {
    stack.push(op);
}

IRateIterator::IRateIterator(ExpressionIterator iterator, bool monotonic)
    : itr(iterator), monotonic(monotonic) {
    if (itr != end(itr)) {
        increment();
    }
}

void IRateIterator::increment() {
    prevSample = *itr;
    ++itr;
    if (itr != end(itr)) {
        currentResult.timestamp = itr->timestamp;
        auto vdelta = itr->value - prevSample.value;
        // get the time delta in seconds, for a per-second rate
        auto tdelta = (itr->timestamp - prevSample.timestamp) / 1000;
        if (!tdelta) {
            currentResult.value = std::numeric_limits<double>::infinity();
        } else if (monotonic && (vdelta < 0)) {
            // copy prometheus's handling of counter reset.
            // assumes the counter has reset to zero at some point during
            // the sample interval. As an exact rate is hard to give in that
            // situation, treat the previous sample as "zero", and give
            // the delta from that.
            // This may underestimate the real rate by dividing by a larger
            // tdelta than the actual time that has passed since counter reset.
            currentResult.value = itr->value / tdelta;
        } else {
            currentResult.value = vdelta / tdelta;
        }
    }
}

Expression::Expression(CrossIndexSeries cis) {
    operations.emplace_back(std::move(cis));
}

Expression::Expression(RateExpression rateExpression) {
    operations.emplace_back(std::move(rateExpression));
}

Expression::Expression(ResampleExpression resampleExpression) {
    operations.emplace_back(std::move(resampleExpression));
}

Expression::Expression(double constantValue) {
    operations.emplace_back(constantValue);
}

Expression Expression::resample(std::chrono::milliseconds interval) const {
    return ResampleExpression(*this, interval);
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
    operations.reserve(operations.size() + other.operations.size());
    operations.insert(
            operations.end(), other.operations.begin(), other.operations.end());
}

Expression Expression::sum(std::vector<Expression> expressions) {
    if (expressions.empty()) {
        return Expression(0.0);
    }
    Expression result;

    // to avoid constant reallocation of the operations vector, scan and
    // count operations
    size_t opCount = 0;
    for (const auto& expr : expressions) {
        opCount += expr.operations.size();
    }
    // sum is currently implemented as N sequential Add operations
    // so increase the opCount for those ops
    opCount += expressions.size() - 1;

    result.operations.reserve(opCount);

    for (const auto& expr : expressions) {
        result.copy_operations_from(expr);
    }

    // now add all the Add operations
    for (int i = 0; i < expressions.size() - 1; ++i) {
        result.operations.emplace_back(Operation::Add);
    }

    return result;
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

Expression irate(const Expression& expr, bool monotonic) {
    return RateExpression(expr, monotonic);
}

Expression resample(const Expression& expr,
                    std::chrono::milliseconds interval) {
    return expr.resample(interval);
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