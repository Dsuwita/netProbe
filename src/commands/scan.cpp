#include "commands.h"
#include "../socket.h"
#include "../ansi.h"
#include "../argparse.h"
#include "../async_io.h"
#include <iostream>
#include <format>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace netprobe::commands {

namespace {

struct ScanResult {
    uint16_t port;
    bool open;
    std::string service;
};

std::string get_service_name(uint16_t port) {
    static const std::map<uint16_t, std::string> services = {
        {20, "ftp-data"}, {21, "ftp"}, {22, "ssh"}, {23, "telnet"},
        {25, "smtp"}, {53, "dns"}, {80, "http"}, {110, "pop3"},
        {143, "imap"}, {443, "https"}, {465, "smtps"}, {587, "smtp"},
        {993, "imaps"}, {995, "pop3s"}, {3306, "mysql"}, {5432, "postgresql"},
        {6379, "redis"}, {27017, "mongodb"}, {8080, "http-alt"}, {8443, "https-alt"}
    };
    
    auto it = services.find(port);
    return it != services.end() ? it->second : "unknown";
}

bool scan_port(std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
    Socket sock(Socket::Type::TCP);
    if (!sock.is_valid()) {
        return false;
    }
    
    auto result = sock.connect(host, port, timeout);
    return result.has_value();
}

} // anonymous namespace

int scan(std::span<const char*> args) {
    ArgParser parser("Scan TCP ports on a host");
    parser.add_positional("host", "Target host");
    parser.add_positional("ports", "Port range (e.g., 1-1024 or 80,443,8080)");
    parser.add_option("timeout", "t", "Timeout per port (ms)", "500");
    parser.add_option("threads", "T", "Number of threads", "100");
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
    
    std::string host = positional[0];
    std::string port_spec = positional[1];
    size_t timeout = parser.get_as<size_t>("timeout").value_or(500);
    size_t num_threads = parser.get_as<size_t>("threads").value_or(100);
    bool json = parser.get_flag("json");
    
    // Parse port specification
    std::vector<uint16_t> ports;
    
    if (port_spec.find('-') != std::string::npos) {
        // Range: 1-1024
        auto dash_pos = port_spec.find('-');
        int start = std::stoi(port_spec.substr(0, dash_pos));
        int end = std::stoi(port_spec.substr(dash_pos + 1));
        
        for (int p = start; p <= end && p <= 65535; ++p) {
            ports.push_back(p);
        }
    } else if (port_spec.find(',') != std::string::npos) {
        // List: 80,443,8080
        size_t pos = 0;
        while (pos < port_spec.length()) {
            size_t comma = port_spec.find(',', pos);
            if (comma == std::string::npos) comma = port_spec.length();
            
            int port = std::stoi(port_spec.substr(pos, comma - pos));
            ports.push_back(port);
            pos = comma + 1;
        }
    } else {
        // Single port
        ports.push_back(std::stoi(port_spec));
    }
    
    if (!json) {
        std::cout << ansi::info(std::format("Scanning {} ports on {}...\n", 
            ports.size(), host));
    }
    
    std::vector<ScanResult> results;
    std::mutex results_mutex;
    std::atomic<size_t> completed{0};
    
    ansi::ProgressBar progress(ports.size());
    
    // Thread pool for scanning
    std::vector<std::thread> threads;
    std::atomic<size_t> next_port{0};
    
    auto scan_worker = [&]() {
        while (true) {
            size_t idx = next_port.fetch_add(1);
            if (idx >= ports.size()) break;
            
            uint16_t port = ports[idx];
            bool is_open = scan_port(host, port, std::chrono::milliseconds(timeout));
            
            if (is_open) {
                std::lock_guard lock(results_mutex);
                results.push_back({port, true, get_service_name(port)});
            }
            
            size_t done = completed.fetch_add(1) + 1;
            if (!json) {
                progress.update(done);
            }
        }
    };
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(scan_worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    if (!json) {
        progress.finish();
    }
    
    // Sort results by port
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.port < b.port;
    });
    
    if (json) {
        std::cout << "{\n  \"host\": \"" << host << "\",\n";
        std::cout << "  \"open_ports\": [\n";
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << std::format("    {{\"port\": {}, \"service\": \"{}\"}}",
                results[i].port, results[i].service);
            if (i < results.size() - 1) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        std::cout << "\n" << ansi::success(std::format("Found {} open ports:\n\n", 
            results.size()));
        
        if (!results.empty()) {
            ansi::Table table({"Port", "State", "Service"});
            for (const auto& result : results) {
                table.add_row({
                    std::format("{}", result.port),
                    ansi::success("open"),
                    result.service
                });
            }
            std::cout << table.render();
        }
    }
    
    return 0;
}

} // namespace netprobe::commands
