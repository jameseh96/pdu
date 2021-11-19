#include "histogram.h"

#include <stdexcept>

Histogram::Histogram(std::vector<double> bucketValues,
                     std::shared_ptr<std::vector<double>> bucketBounds,
                     double sum)
    : bucketValues(bucketValues),
      bucketBounds(std::move(bucketBounds)),
      sum(sum) {
}

Histogram Histogram::operator-(const Histogram& other) const {
    if (*bucketBounds != *other.bucketBounds) {
        throw std::runtime_error(
                "Cannot subtract histograms with different bucket bounds");
    }
    if (bucketValues.size() != other.bucketValues.size()) {
        throw std::logic_error(
                "Histogram::operator- histograms have matching bounds but "
                "different bucketValues.size()");
    }
    std::vector<double> diffValues;
    diffValues.reserve(bucketValues.size());
    for (int i = 0; i < bucketValues.size(); ++i) {
        diffValues.push_back(bucketValues[i] - other.bucketValues[i]);
    }
    auto diffSum = sum - other.sum;
    return {diffValues, bucketBounds, diffSum};
}

Histogram Histogram::operator+(const Histogram& other) const {
    if (*bucketBounds != *other.bucketBounds) {
        throw std::runtime_error(
                "Cannot sum histograms with different bucket bounds");
    }
    if (bucketValues.size() != other.bucketValues.size()) {
        throw std::logic_error(
                "Histogram::operator+ histograms have matching bounds but "
                "different bucketValues.size()");
    }
    std::vector<double> sumValues;
    sumValues.reserve(bucketValues.size());
    for (int i = 0; i < bucketValues.size(); ++i) {
        sumValues.push_back(bucketValues[i] + other.bucketValues[i]);
    }
    auto totalSum = sum + other.sum;
    return {sumValues, bucketBounds, totalSum};
}