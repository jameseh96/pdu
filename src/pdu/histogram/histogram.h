#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include <memory>

class Histogram {
public:
    Histogram();
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

private:
    std::vector<double> bucketValues;
    std::shared_ptr<std::vector<double>> bucketBounds;
    double sum;
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

private:
    int64_t timestamp;
};

using TimestampedHistogram = Timestamp<Histogram>;