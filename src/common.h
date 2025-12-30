#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <variant>
#include <chrono>
#include <cstdint>

namespace netprobe {

// Type aliases
using namespace std::chrono_literals;
using steady_clock = std::chrono::steady_clock;
using time_point = steady_clock::time_point;
using duration = steady_clock::duration;

// Error handling - C++20 compatible optional-based result
template<typename T>
struct Result {
    std::optional<T> value;
    std::string error;
    
    Result(T&& val) : value(std::move(val)) {}
    Result(const T& val) : value(val) {}
    Result(std::string err) : error(std::move(err)), value(std::nullopt) {}
    
    bool has_value() const { return value.has_value(); }
    explicit operator bool() const { return has_value(); }
    T& operator*() { return *value; }
    const T& operator*() const { return *value; }
    T* operator->() { return &(*value); }
    const T* operator->() const { return &(*value); }
};

template<>
struct Result<void> {
    std::string error;
    bool success;
    
    Result() : success(true) {}
    Result(std::string err) : error(std::move(err)), success(false) {}
    
    bool has_value() const { return success; }
    explicit operator bool() const { return success; }
};

// Common constants
constexpr size_t DEFAULT_THREADS = 8;
constexpr size_t DEFAULT_TIMEOUT_MS = 1000;
constexpr size_t MAX_PACKET_SIZE = 65536;

} // namespace netprobe
