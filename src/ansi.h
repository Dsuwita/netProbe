#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdio>

namespace netprobe::ansi {

// Color codes
namespace color {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM = "\033[2m";
    
    constexpr const char* BLACK = "\033[30m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* WHITE = "\033[37m";
    
    constexpr const char* BRIGHT_RED = "\033[91m";
    constexpr const char* BRIGHT_GREEN = "\033[92m";
    constexpr const char* BRIGHT_YELLOW = "\033[93m";
    constexpr const char* BRIGHT_CYAN = "\033[96m";
}

// Box drawing characters (ASCII for compatibility)
namespace box {
    constexpr const char* HORIZONTAL = "-";
    constexpr const char* VERTICAL = "|";
    constexpr const char* TOP_LEFT = "+";
    constexpr const char* TOP_RIGHT = "+";
    constexpr const char* BOTTOM_LEFT = "+";
    constexpr const char* BOTTOM_RIGHT = "+";
    constexpr const char* T_DOWN = "+";
    constexpr const char* T_UP = "+";
    constexpr const char* T_RIGHT = "+";
    constexpr const char* T_LEFT = "+";
    constexpr const char* CROSS = "+";
}

// Progress bar characters (ASCII for compatibility)
namespace progress {
    constexpr const char* FULL = "#";
    constexpr const char* SEVEN_EIGHTHS = "#";
    constexpr const char* THREE_QUARTERS = "#";
    constexpr const char* FIVE_EIGHTHS = "=";
    constexpr const char* HALF = "=";
    constexpr const char* THREE_EIGHTHS = "-";
    constexpr const char* QUARTER = "-";
    constexpr const char* ONE_EIGHTH = "-";
}

// Check if terminal supports colors
bool is_tty();
void enable_colors(bool enable);

// Colored output helpers
std::string colorize(std::string_view text, const char* color);
std::string success(std::string_view text);
std::string error(std::string_view text);
std::string warning(std::string_view text);
std::string info(std::string_view text);

// Table rendering
class Table {
public:
    Table(std::vector<std::string> headers);
    void add_row(std::vector<std::string> row);
    std::string render() const;

private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
};

// Progress bar
class ProgressBar {
public:
    ProgressBar(size_t total, size_t width = 50);
    void update(size_t current);
    void finish();

private:
    size_t total_;
    size_t width_;
    size_t last_printed_;
};

// Histogram rendering
std::string render_histogram(const std::vector<double>& values, size_t bins = 20, size_t width = 60);

} // namespace netprobe::ansi
