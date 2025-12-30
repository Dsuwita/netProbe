#include "commands.h"
#include "../socket.h"
#include "../stats.h"
#include "../ansi.h"
#include "../argparse.h"
#include <iostream>
#include <format>
#include <thread>
#include <vector>
#include <atomic>
#include <sstream>

namespace netprobe::commands {

namespace {

Result<std::pair<size_t, double>> http_request(std::string_view host, uint16_t port, 
                                                std::string_view path) {
    Socket sock(Socket::Type::TCP);
    if (!sock.is_valid()) {
        return Result<std::pair<size_t, double>>("Failed to create socket");
    }
    
    auto connect_result = sock.connect(host, port, 2000ms);
    if (!connect_result) {
        return Result<std::pair<size_t, double>>(connect_result.error);
    }
    
    // Build HTTP request
    std::string request = std::format(
        "GET {} HTTP/1.1\r\n"
        "Host: {}\r\n"
        "Connection: close\r\n"
        "User-Agent: NetProbe/1.0\r\n"
        "\r\n",
        path, host);
    
    auto start = steady_clock::now();
    
    auto send_result = sock.send(request.data(), request.size());
    if (!send_result) {
        return Result<std::pair<size_t, double>>(send_result.error);
    }
    
    // Receive response
    char buffer[4096];
    size_t total_bytes = 0;
    
    while (true) {
        auto recv_result = sock.recv(buffer, sizeof(buffer));
        if (!recv_result || *recv_result == 0) {
            break;
        }
        total_bytes += *recv_result;
    }
    
    auto end = steady_clock::now();
    auto latency = std::chrono::duration<double, std::milli>(end - start).count();
    
    return std::make_pair(total_bytes, latency);
}

} // anonymous namespace

int bench(std::span<const char*> args) {
    ArgParser parser("HTTP benchmark tool");
    parser.add_positional("url", "Target URL (e.g., example.com or example.com/path)");
    parser.add_positional("duration", "Duration (e.g., 10s)");
    parser.add_option("connections", "c", "Number of concurrent connections", "10");
    parser.add_option("port", "p", "Port number", "80");
    parser.add_flag("json", "j", "Output in JSON format");
    
    auto parse_result = parser.parse(args);
    if (!parse_result) {
        std::cerr << ansi::error(parse_result.error) << "\n";
        return 1;
    }
    
    auto positional = parser.get_positional();
    if (positional.size() < 2) {
        std::cerr << ansi::error("Missing required arguments") << "\n";
        return 1;
    }
    
    std::string url = positional[0];
    std::string duration_str = positional[1];
    size_t connections = parser.get_as<size_t>("connections").value_or(10);
    uint16_t port = parser.get_as<uint16_t>("port").value_or(80);
    bool json = parser.get_flag("json");
    
    // Parse URL
    std::string host = url;
    std::string path = "/";
    
    auto slash_pos = url.find('/');
    if (slash_pos != std::string::npos) {
        host = url.substr(0, slash_pos);
        path = url.substr(slash_pos);
    }
    
    // Parse duration
    size_t duration_sec = std::stoi(duration_str.substr(0, duration_str.length() - 1));
    
    if (!json) {
        std::cout << ansi::info(std::format(
            "Benchmarking http://{}:{}{} for {}s with {} connections...\n",
            host, port, path, duration_sec, connections));
    }
    
    Statistics latency_stats;
    std::atomic<size_t> total_requests{0};
    std::atomic<size_t> total_bytes{0};
    std::atomic<size_t> errors{0};
    std::atomic<bool> running{true};
    
    std::vector<std::thread> threads;
    
    auto worker = [&]() {
        while (running) {
            auto result = http_request(host, port, path);
            
            if (result) {
                auto [bytes, latency] = *result;
                latency_stats.add(latency);
                total_requests.fetch_add(1);
                total_bytes.fetch_add(bytes);
            } else {
                errors.fetch_add(1);
            }
        }
    };
    
    auto start_time = steady_clock::now();
    
    for (size_t i = 0; i < connections; ++i) {
        threads.emplace_back(worker);
    }
    
    // Wait for duration
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
    running = false;
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = steady_clock::now();
    auto actual_duration = std::chrono::duration<double>(end_time - start_time).count();
    
    double req_per_sec = total_requests / actual_duration;
    double bytes_per_sec = total_bytes / actual_duration;
    double error_rate = total_requests > 0 
        ? (100.0 * errors) / (total_requests + errors)
        : 0.0;
    
    if (json) {
        std::cout << std::format(R"({{
  "url": "http://{}:{}{}",
  "duration": {:.2f},
  "total_requests": {},
  "requests_per_sec": {:.2f},
  "total_bytes": {},
  "bytes_per_sec": {:.2f},
  "errors": {},
  "error_rate": {:.2f},
  "latency": {{
    "min": {:.2f},
    "avg": {:.2f},
    "p50": {:.2f},
    "p95": {:.2f},
    "p99": {:.2f},
    "max": {:.2f}
  }}
}})",
            host, port, path,
            actual_duration,
            total_requests.load(),
            req_per_sec,
            total_bytes.load(),
            bytes_per_sec,
            errors.load(),
            error_rate,
            latency_stats.min(),
            latency_stats.mean(),
            latency_stats.percentile(50),
            latency_stats.percentile(95),
            latency_stats.percentile(99),
            latency_stats.max());
    } else {
        std::cout << "\n" << ansi::colorize("Benchmark Results", ansi::color::BOLD) << "\n\n";
        
        ansi::Table table(std::vector<std::string>{"Metric", "Value"});
        table.add_row({"Duration", std::format("{:.2f}s", actual_duration)});
        table.add_row({"Total Requests", std::format("{}", total_requests.load())});
        table.add_row({"Requests/sec", ansi::success(std::format("{:.2f}", req_per_sec))});
        table.add_row({"Total Bytes", std::format("{}", total_bytes.load())});
        table.add_row({"Throughput", std::format("{:.2f} KB/s", bytes_per_sec / 1024)});
        table.add_row({"Errors", errors > 0 ? ansi::error(std::format("{}", errors.load())) 
                                            : std::format("{}", errors.load())});
        table.add_row({"Error Rate", std::format("{:.2f}%", error_rate)});
        
        std::cout << table.render() << "\n";
        
        std::cout << ansi::colorize("Latency Distribution", ansi::color::BOLD) << "\n\n";
        
        ansi::Table latency_table({"Percentile", "Latency (ms)"});
        latency_table.add_row({"Min", std::format("{:.2f}", latency_stats.min())});
        latency_table.add_row({"P50", std::format("{:.2f}", latency_stats.percentile(50))});
        latency_table.add_row({"P95", std::format("{:.2f}", latency_stats.percentile(95))});
        latency_table.add_row({"P99", std::format("{:.2f}", latency_stats.percentile(99))});
        latency_table.add_row({"Max", std::format("{:.2f}", latency_stats.max())});
        latency_table.add_row({"Avg", std::format("{:.2f}", latency_stats.mean())});
        
        std::cout << latency_table.render();
    }
    
    return 0;
}

} // namespace netprobe::commands
