#include "histogram.h"

Histogram::Histogram(std::vector<double> bucketValues,
                     std::shared_ptr<std::vector<double>> bucketBounds,
                     double sum)
    : bucketValues(bucketValues),
      bucketBounds(std::move(bucketBounds)),
      sum(sum) {
}