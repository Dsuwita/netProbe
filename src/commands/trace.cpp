#include "commands.h"
#include "../socket.h"
#include "../stats.h"
#include "../ansi.h"
#include "../argparse.h"
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <iostream>
#include <format>
#include <thread>

namespace netprobe::commands {

namespace {

Result<std::pair<std::string, double>> probe_hop(const sockaddr_in& dest, int ttl) {
    Socket sock(Socket::Type::UDP);
    if (!sock.is_valid()) {
        return Result<std::pair<std::string, double>>("Failed to create UDP socket");
    }
    
    sock.set_ttl(ttl);
    sock.set_timeout(2000ms);
    
    // Create ICMP socket to receive TTL exceeded messages
    Socket icmp_sock(Socket::Type::ICMP);
    if (!icmp_sock.is_valid()) {
        return Result<std::pair<std::string, double>>("Failed to create ICMP socket");
    }
    
    icmp_sock.set_timeout(2000ms);
    
    auto start = steady_clock::now();
    
    // Send UDP packet to high port
    char data = 0;
    auto send_result = sock.sendto(&data, sizeof(data),
        reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
    
    if (!send_result) {
        return Result<std::pair<std::string, double>>(send_result.error);
    }
    
    // Wait for ICMP response
    char buffer[512];
    sockaddr_in from{};
    socklen_t fromlen = sizeof(from);
    
    auto recv_result = icmp_sock.recvfrom(buffer, sizeof(buffer),
        reinterpret_cast<sockaddr*>(&from), &fromlen);
    
    auto end = steady_clock::now();
    auto rtt = std::chrono::duration<double, std::milli>(end - start).count();
    
    if (!recv_result) {
        return Result<std::pair<std::string, double>>("Timeout");
    }
    
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, addr_str, sizeof(addr_str));
    
    return std::make_pair(std::string(addr_str), rtt);
}

} // anonymous namespace

int trace(std::span<const char*> args) {
    ArgParser parser("Trace the route to a host");
    parser.add_positional("host", "Target host");
    parser.add_option("max-hops", "m", "Maximum number of hops", "30");
    parser.add_option("queries", "q", "Number of queries per hop", "3");
    parser.add_flag("json", "j", "Output in JSON format");
    
    auto parse_result = parser.parse(args);
    if (!parse_result) {
        std::cerr << ansi::error(parse_result.error) << "\n";
        return 1;
    }
    
    auto positional = parser.get_positional();
    if (positional.empty()) {
        std::cerr << ansi::error("Missing host argument") << "\n";
        return 1;
    }
    
    std::string host = positional[0];
    size_t max_hops = parser.get_as<size_t>("max-hops").value_or(30);
    size_t queries = parser.get_as<size_t>("queries").value_or(3);
    bool json = parser.get_flag("json");
    
    // Resolve destination
    auto addr_result = Socket::resolve(host, 33434); // Standard traceroute port
    if (!addr_result) {
        std::cerr << ansi::error(std::format("Failed to resolve {}: {}",
            host, addr_result.error)) << "\n";
        return 1;
    }
    
    auto& dest = *addr_result;
    
    if (!json) {
        std::cout << ansi::info(std::format("traceroute to {} ({}), {} hops max",
            host, Socket::addr_to_string(dest).substr(0, Socket::addr_to_string(dest).find(':')),
            max_hops)) << "\n\n";
    }
    
    ansi::Table table({"Hop", "Address", "RTT 1", "RTT 2", "RTT 3", "Avg"});
    
    for (size_t ttl = 1; ttl <= max_hops; ++ttl) {
        std::vector<double> rtts;
        std::string hop_addr = "*";
        
        for (size_t q = 0; q < queries; ++q) {
            auto probe_result = probe_hop(dest, ttl);
            
            if (probe_result) {
                auto [addr, rtt] = *probe_result;
                hop_addr = addr;
                rtts.push_back(rtt);
            }
            
            std::this_thread::sleep_for(100ms);
        }
        
        if (!json) {
            std::vector<std::string> row;
            row.push_back(std::format("{}", ttl));
            row.push_back(hop_addr);
            
            for (size_t i = 0; i < queries; ++i) {
                if (i < rtts.size()) {
                    row.push_back(std::format("{:.2f} ms", rtts[i]));
                } else {
                    row.push_back("*");
                }
            }
            
            if (!rtts.empty()) {
                double avg = std::accumulate(rtts.begin(), rtts.end(), 0.0) / rtts.size();
                row.push_back(std::format("{:.2f} ms", avg));
            } else {
                row.push_back("*");
            }
            
            table.add_row(row);
        }
        
        // Check if we reached destination
        char dest_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &dest.sin_addr, dest_str, sizeof(dest_str));
        
        if (hop_addr == dest_str) {
            break;
        }
        
        if (hop_addr == "*" && ttl > 10) {
            // Too many failed hops, likely unreachable
            break;
        }
    }
    
    if (!json) {
        std::cout << table.render();
    }
    
    return 0;
}

} // namespace netprobe::commands
