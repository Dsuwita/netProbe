#include "ansi.h"
#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <format>

namespace netprobe::ansi {

static bool colors_enabled = true;

bool is_tty() {
    return isatty(STDOUT_FILENO) != 0;
}

void enable_colors(bool enable) {
    colors_enabled = enable && is_tty();
}

std::string colorize(std::string_view text, const char* color) {
    if (!colors_enabled) return std::string(text);
    return std::format("{}{}{}", color, text, ansi::color::RESET);
}

std::string success(std::string_view text) {
    return colorize(text, color::BRIGHT_GREEN);
}

std::string error(std::string_view text) {
    return colorize(text, color::BRIGHT_RED);
}

std::string warning(std::string_view text) {
    return colorize(text, color::BRIGHT_YELLOW);
}

std::string info(std::string_view text) {
    return colorize(text, color::BRIGHT_CYAN);
}

// Table implementation
Table::Table(std::vector<std::string> headers) : headers_(std::move(headers)) {}

void Table::add_row(std::vector<std::string> row) {
    rows_.push_back(std::move(row));
}

std::string Table::render() const {
    if (headers_.empty()) return "";
    
    // Calculate column widths
    std::vector<size_t> widths(headers_.size());
    for (size_t i = 0; i < headers_.size(); ++i) {
        widths[i] = headers_[i].length();
        for (const auto& row : rows_) {
            if (i < row.size()) {
                widths[i] = std::max(widths[i], row[i].length());
            }
        }
    }
    
    std::string result;
    
    // Top border
    result += box::TOP_LEFT;
    for (size_t i = 0; i < widths.size(); ++i) {
        result += std::string(widths[i] + 2, *box::HORIZONTAL);
        if (i < widths.size() - 1) result += box::T_DOWN;
    }
    result += box::TOP_RIGHT;
    result += "\n";
    
    // Header
    result += box::VERTICAL;
    for (size_t i = 0; i < headers_.size(); ++i) {
        result += " " + colorize(headers_[i], color::BOLD);
        result += std::string(widths[i] - headers_[i].length(), ' ') + " ";
        result += box::VERTICAL;
    }
    result += "\n";
    
    // Header separator
    result += box::T_RIGHT;
    for (size_t i = 0; i < widths.size(); ++i) {
        result += std::string(widths[i] + 2, *box::HORIZONTAL);
        if (i < widths.size() - 1) result += box::CROSS;
    }
    result += box::T_LEFT;
    result += "\n";
    
    // Rows
    for (const auto& row : rows_) {
        result += box::VERTICAL;
        for (size_t i = 0; i < headers_.size(); ++i) {
            std::string cell = i < row.size() ? row[i] : "";
            result += " " + cell;
            result += std::string(widths[i] - cell.length(), ' ') + " ";
            result += box::VERTICAL;
        }
        result += "\n";
    }
    
    // Bottom border
    result += box::BOTTOM_LEFT;
    for (size_t i = 0; i < widths.size(); ++i) {
        result += std::string(widths[i] + 2, *box::HORIZONTAL);
        if (i < widths.size() - 1) result += box::T_UP;
    }
    result += box::BOTTOM_RIGHT;
    result += "\n";
    
    return result;
}

// Progress bar implementation
ProgressBar::ProgressBar(size_t total, size_t width)
    : total_(total), width_(width), last_printed_(0) {}

void ProgressBar::update(size_t current) {
    if (!is_tty()) return;
    
    double percentage = total_ > 0 ? (double)current / total_ : 0.0;
    size_t filled = static_cast<size_t>(percentage * width_);
    
    std::printf("\r[");
    for (size_t i = 0; i < width_; ++i) {
        if (i < filled) {
            std::printf("%s", progress::FULL);
        } else {
            std::printf(" ");
        }
    }
    std::printf("] %3.0f%% (%zu/%zu)", percentage * 100, current, total_);
    std::fflush(stdout);
    
    last_printed_ = current;
}

void ProgressBar::finish() {
    update(total_);
    std::printf("\n");
}

// Histogram rendering
std::string render_histogram(const std::vector<double>& values, size_t bins, size_t width) {
    if (values.empty()) return "";
    
    auto [min_it, max_it] = std::ranges::minmax_element(values);
    double min_val = *min_it;
    double max_val = *max_it;
    
    if (min_val == max_val) return "All values identical\n";
    
    std::vector<size_t> histogram(bins, 0);
    double bin_width = (max_val - min_val) / bins;
    
    for (double val : values) {
        size_t bin = std::min(bins - 1, 
            static_cast<size_t>((val - min_val) / bin_width));
        histogram[bin]++;
    }
    
    size_t max_count = *std::ranges::max_element(histogram);
    
    std::string result;
    for (size_t i = 0; i < bins; ++i) {
        double range_start = min_val + i * bin_width;
        double range_end = min_val + (i + 1) * bin_width;
        
        size_t bar_len = max_count > 0 
            ? (histogram[i] * width) / max_count 
            : 0;
        
        result += std::format("{:8.2f}-{:8.2f} │{}{}\n",
            range_start, range_end,
            std::string(bar_len, *progress::FULL),
            std::string(width - bar_len, ' '));
    }
    
    return result;
}

} // namespace netprobe::ansi
