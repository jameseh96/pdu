#include "histogram.h"

Histogram::Histogram(std::vector<double> bucketValues, double sum)
    : bucketValues(bucketValues), sum(sum) {
}