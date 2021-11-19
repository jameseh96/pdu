#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include <memory>

class Histogram {
public:
    Histogram();
    Histogram(const Histogram&) = default;
    Histogram(Histogram&&) = default;
    Histogram& operator=(const Histogram&) = default;
    Histogram& operator=(Histogram&&) = default;

    Histogram(std::vector<double> bucketValues,
              std::shared_ptr<std::vector<double>> bucketBounds,
              double sum);

    const std::vector<double>& getValues() const {
        return bucketValues;
    }
    const std::vector<double>& getBounds() const {
        return *bucketBounds;
    }
    double getSum() const {
        return sum;
    }

    Histogram operator-(const Histogram& other) const;
    Histogram operator+(const Histogram& other) const;

private:
    std::vector<double> bucketValues;
    std::shared_ptr<std::vector<double>> bucketBounds;
    double sum;
};

template <class Base>
class TimeDelta : public Base {
public:
    template <class... Params>
    TimeDelta(int64_t timeDelta, Params&&... params)
        : Base(std::forward<Params>(params)...), timeDelta(timeDelta) {
    }
    int64_t getTimeDelta() const {
        return timeDelta;
    }

private:
    int64_t timeDelta;
};

template <class Base>
class Timestamp : public Base {
public:
    template <class... Params>
    Timestamp(int64_t timestamp, Params&&... params)
        : Base(std::forward<Params>(params)...), timestamp(timestamp) {
    }
    int64_t getTimestamp() const {
        return timestamp;
    }

    TimeDelta<Base> operator-(const Timestamp<Base>& other) const {
        auto tsDelta = timestamp - other.getTimestamp();
        return {tsDelta,
                static_cast<const Base&>(*this) -
                        static_cast<const Base&>(other)};
    }

private:
    int64_t timestamp;
};

using TimestampedHistogram = Timestamp<Histogram>;
using DeltaHistogram = TimeDelta<Histogram>;
