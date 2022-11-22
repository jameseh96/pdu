#include "sample.h"

bool operator==(const Sample& a, const Sample& b) {
    return a.timestamp == b.timestamp && a.value == b.value;
}

bool operator!=(const Sample& a, const Sample& b) {
    return !(a == b);
}
