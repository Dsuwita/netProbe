#include "commands.h"
#include "../socket.h"
#include "../ansi.h"
#include "../argparse.h"
#include <iostream>
#include <format>
#include <thread>
#include <atomic>
#include <cstring>

namespace netprobe::commands {

namespace {

constexpr size_t BUFFER_SIZE = 128 * 1024; // 128KB buffer
constexpr uint16_t DEFAULT_PORT = 5201;

void run_server(uint16_t port, std::chrono::seconds duration) {
    Socket listen_sock(Socket::Type::TCP);
    if (!listen_sock.is_valid()) {
        std::cerr << ansi::error("Failed to create socket") << "\n";
        return;
    }
    
    listen_sock.set_reuse_addr(true);
    
    auto bind_result = listen_sock.bind(port);
    if (!bind_result) {
        std::cerr << ansi::error(std::format("Failed to bind: {}", bind_result.error)) << "\n";
        return;
    }
    
    auto listen_result = listen_sock.listen();
    if (!listen_result) {
        std::cerr << ansi::error(std::format("Failed to listen: {}", listen_result.error)) << "\n";
        return;
    }
    
    std::cout << ansi::success(std::format("iperf server listening on port {}\n", port));
    std::cout << ansi::info("Waiting for client connection...\n\n");
    
    auto client_result = listen_sock.accept();
    if (!client_result) {
        std::cerr << ansi::error(std::format("Failed to accept: {}", client_result.error)) << "\n";
        return;
    }
    
    Socket client = std::move(*client_result);
    std::cout << ansi::success("Client connected!\n");
    
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    size_t total_bytes = 0;
    auto start = steady_clock::now();
    
    while (true) {
        auto recv_result = client.recv(buffer.data(), buffer.size());
        if (!recv_result || *recv_result == 0) {
            break; // Connection closed
        }
        total_bytes += *recv_result;
    }
    
    auto end = steady_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();
    
    double throughput_mbps = (total_bytes * 8.0) / (elapsed * 1024 * 1024);
    
    std::cout << "\n" << ansi::colorize("Server Results", ansi::color::BOLD) << "\n";
    std::cout << std::format("Duration:    {:.2f} seconds\n", elapsed);
    std::cout << std::format("Received:    {} bytes ({:.2f} MB)\n", 
        total_bytes, total_bytes / (1024.0 * 1024.0));
    std::cout << ansi::success(std::format("Throughput:  {:.2f} Mbps\n", throughput_mbps));
}

void run_client(std::string_view host, uint16_t port, std::chrono::seconds duration) {
    Socket sock(Socket::Type::TCP);
    if (!sock.is_valid()) {
        std::cerr << ansi::error("Failed to create socket") << "\n";
        return;
    }
    
    std::cout << ansi::info(std::format("Connecting to {}:{}...\n", host, port));
    
    auto connect_result = sock.connect(host, port, 5000ms);
    if (!connect_result) {
        std::cerr << ansi::error(std::format("Failed to connect: {}", 
            connect_result.error)) << "\n";
        return;
    }
    
    std::cout << ansi::success("Connected!\n");
    std::cout << ansi::info(std::format("Running throughput test for {}s...\n\n", 
        duration.count()));
    
    std::vector<uint8_t> buffer(BUFFER_SIZE, 0xAA);
    size_t total_bytes = 0;
    auto start = steady_clock::now();
    auto end_time = start + duration;
    
    std::atomic<bool> running{true};
    std::thread progress_thread([&]() {
        while (running) {
            auto now = steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed > 0) {
                double mbps = (total_bytes * 8.0) / (elapsed * 1024 * 1024);
                std::cout << std::format("\r{:.2f}s - {:.2f} Mbps          ", 
                    elapsed, mbps);
                std::cout.flush();
            }
            std::this_thread::sleep_for(100ms);
        }
    });
    
    while (steady_clock::now() < end_time) {
        auto send_result = sock.send(buffer.data(), buffer.size());
        if (!send_result) {
            break;
        }
        total_bytes += *send_result;
    }
    
    running = false;
    progress_thread.join();
    
    auto end = steady_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();
    
    double throughput_mbps = (total_bytes * 8.0) / (elapsed * 1024 * 1024);
    
    std::cout << "\n\n" << ansi::colorize("Client Results", ansi::color::BOLD) << "\n";
    std::cout << std::format("Duration:    {:.2f} seconds\n", elapsed);
    std::cout << std::format("Sent:        {} bytes ({:.2f} MB)\n",
        total_bytes, total_bytes / (1024.0 * 1024.0));
    std::cout << ansi::success(std::format("Throughput:  {:.2f} Mbps\n", throughput_mbps));
}

} // anonymous namespace

int iperf(std::span<const char*> args) {
    ArgParser parser("Network throughput testing tool");
    parser.add_positional("mode", "Mode: 'server' or 'client'");
    parser.add_option("port", "p", "Port number", "5201");
    parser.add_option("duration", "t", "Test duration (seconds)", "10");
    
    auto parse_result = parser.parse(args);
    if (!parse_result) {
        std::cerr << ansi::error(parse_result.error) << "\n";
        return 1;
    }
    
    auto positional = parser.get_positional();
    if (positional.empty()) {
        std::cerr << ansi::error("Missing mode argument (server/client)") << "\n";
        return 1;
    }
    
    std::string mode = positional[0];
    uint16_t port = parser.get_as<uint16_t>("port").value_or(DEFAULT_PORT);
    size_t duration_sec = parser.get_as<size_t>("duration").value_or(10);
    auto duration = std::chrono::seconds(duration_sec);
    
    if (mode == "server") {
        run_server(port, duration);
    } else if (mode == "client") {
        if (positional.size() < 2) {
            std::cerr << ansi::error("Client mode requires host argument") << "\n";
            std::cerr << "Usage: netprobe iperf client <host> [options]\n";
            return 1;
        }
        
        std::string host = positional[1];
        
        // Parse host:port if specified
        auto colon_pos = host.find(':');
        if (colon_pos != std::string::npos) {
            port = std::stoi(host.substr(colon_pos + 1));
            host = host.substr(0, colon_pos);
        }
        
        run_client(host, port, duration);
    } else {
        std::cerr << ansi::error(std::format("Unknown mode: {}", mode)) << "\n";
        std::cerr << "Use 'server' or 'client'\n";
        return 1;
    }
    
    return 0;
}

} // namespace netprobe::commands
