#include "stats.h"
#include <stdexcept>

namespace netprobe {

void Statistics::add(double value) {
    values_.push_back(value);
    sum_ += value;
    sum_sq_ += value * value;
}

void Statistics::reset() {
    values_.clear();
    sum_ = 0.0;
    sum_sq_ = 0.0;
}

double Statistics::min() const {
    if (values_.empty()) return 0.0;
    return *std::ranges::min_element(values_);
}

double Statistics::max() const {
    if (values_.empty()) return 0.0;
    return *std::ranges::max_element(values_);
}

double Statistics::mean() const {
    if (values_.empty()) return 0.0;
    return sum_ / values_.size();
}

double Statistics::median() const {
    return percentile(50.0);
}

double Statistics::stddev() const {
    if (values_.size() < 2) return 0.0;
    double variance = (sum_sq_ - sum_ * sum_ / values_.size()) / (values_.size() - 1);
    return std::sqrt(std::max(0.0, variance));
}

double Statistics::percentile(double p) const {
    if (values_.empty()) return 0.0;
    if (p <= 0.0) return min();
    if (p >= 100.0) return max();
    
    auto sorted = values_;
    std::ranges::sort(sorted);
    
    double index = (p / 100.0) * (sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));
    
    if (lower == upper) return sorted[lower];
    
    double weight = index - lower;
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

double Statistics::jitter() const {
    if (values_.size() < 2) return 0.0;
    
    double sum_diff = 0.0;
    for (size_t i = 1; i < values_.size(); ++i) {
        sum_diff += std::abs(values_[i] - values_[i-1]);
    }
    return sum_diff / (values_.size() - 1);
}

} // namespace netprobe
