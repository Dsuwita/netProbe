#pragma once

#include "common.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace netprobe {

// Running statistics calculator
class Statistics {
public:
    void add(double value);
    void reset();
    
    size_t count() const { return values_.size(); }
    double min() const;
    double max() const;
    double mean() const;
    double median() const;
    double stddev() const;
    double percentile(double p) const; // p in [0, 100]
    double jitter() const;
    
    const std::vector<double>& values() const { return values_; }

private:
    std::vector<double> values_;
    double sum_ = 0.0;
    double sum_sq_ = 0.0;
};

} // namespace netprobe
