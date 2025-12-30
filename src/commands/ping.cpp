#include "commands.h"
#include "../socket.h"
#include "../stats.h"
#include "../ansi.h"
#include "../argparse.h"
#include <netinet/ip_icmp.h>
#include <iostream>
#include <format>
#include <thread>

namespace netprobe::commands {

namespace {

struct ICMPPacket {
    icmphdr header;
    char data[56];
};

uint16_t checksum(const void* data, size_t len) {
    const uint16_t* buf = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *reinterpret_cast<const uint8_t*>(buf);
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return ~sum;
}

Result<double> send_ping(Socket& sock, const sockaddr_in& addr, uint16_t seq) {
    ICMPPacket packet{};
    packet.header.type = ICMP_ECHO;
    packet.header.code = 0;
    packet.header.un.echo.id = getpid();
    packet.header.un.echo.sequence = seq;
    
    // Fill data with pattern
    for (size_t i = 0; i < sizeof(packet.data); ++i) {
        packet.data[i] = i;
    }
    
    packet.header.checksum = 0;
    packet.header.checksum = checksum(&packet, sizeof(packet));
    
    auto start = steady_clock::now();
    
    auto result = sock.sendto(&packet, sizeof(packet),
        reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    
    if (!result) {
        return Result<double>(result.error);
    }
    
    // Wait for reply
    ICMPPacket reply{};
    sockaddr_in from{};
    socklen_t fromlen = sizeof(from);
    
    auto recv_result = sock.recvfrom(&reply, sizeof(reply),
        reinterpret_cast<sockaddr*>(&from), &fromlen);
    
    if (!recv_result) {
        return Result<double>(recv_result.error);
    }
    
    auto end = steady_clock::now();
    auto rtt = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Verify it's our packet
    if (reply.header.type != ICMP_ECHOREPLY || 
        reply.header.un.echo.id != getpid() ||
        reply.header.un.echo.sequence != seq) {
        return Result<double>("Invalid ICMP reply");
    }
    
    return rtt;
}

} // anonymous namespace

int ping(std::span<const char*> args) {
    ArgParser parser("Send ICMP echo requests to a host");
    parser.add_positional("host", "Target host");
    parser.add_option("count", "c", "Number of pings", "10");
    parser.add_option("interval", "i", "Interval between pings (ms)", "1000");
    parser.add_option("timeout", "t", "Timeout for each ping (ms)", "1000");
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
    size_t count = parser.get_as<size_t>("count").value_or(10);
    size_t interval = parser.get_as<size_t>("interval").value_or(1000);
    size_t timeout = parser.get_as<size_t>("timeout").value_or(1000);
    bool json = parser.get_flag("json");
    
    // Resolve host
    auto addr_result = Socket::resolve(host, 0);
    if (!addr_result) {
        std::cerr << ansi::error(std::format("Failed to resolve {}: {}", 
            host, addr_result.error)) << "\n";
        return 1;
    }
    
    auto& addr = *addr_result;
    
    // Create ICMP socket
    Socket sock(Socket::Type::ICMP);
    if (!sock.is_valid()) {
        std::cerr << ansi::error("Failed to create ICMP socket (try running with sudo)") << "\n";
        return 1;
    }
    
    sock.set_timeout(std::chrono::milliseconds(timeout));
    
    if (!json) {
        std::cout << ansi::info(std::format("PING {} ({}) {} bytes of data",
            host, Socket::addr_to_string(addr).substr(0, Socket::addr_to_string(addr).find(':')),
            sizeof(ICMPPacket))) << "\n\n";
    }
    
    Statistics stats;
    size_t transmitted = 0;
    size_t received = 0;
    
    for (size_t i = 0; i < count; ++i) {
        transmitted++;
        
        auto rtt_result = send_ping(sock, addr, i + 1);
        
        if (rtt_result) {
            received++;
            double rtt = *rtt_result;
            stats.add(rtt);
            
            if (!json) {
                std::cout << ansi::success(std::format(
                    "64 bytes from {}: icmp_seq={} ttl=64 time={:.2f} ms",
                    Socket::addr_to_string(addr).substr(0, Socket::addr_to_string(addr).find(':')),
                    i + 1, rtt)) << "\n";
            }
        } else {
            if (!json) {
                std::cout << ansi::error(std::format(
                    "Request timeout for icmp_seq {}", i + 1)) << "\n";
            }
        }
        
        if (i < count - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }
    
    double loss = transmitted > 0 
        ? 100.0 * (transmitted - received) / transmitted 
        : 0.0;
    
    if (json) {
        std::cout << std::format(R"({{
  "host": "{}",
  "transmitted": {},
  "received": {},
  "loss_percent": {:.2f},
  "rtt_min": {:.2f},
  "rtt_avg": {:.2f},
  "rtt_max": {:.2f},
  "rtt_stddev": {:.2f},
  "jitter": {:.2f}
}})",
            host, transmitted, received, loss,
            stats.min(), stats.mean(), stats.max(),
            stats.stddev(), stats.jitter());
    } else {
        std::cout << "\n" << ansi::colorize("--- ping statistics ---", ansi::color::BOLD) << "\n";
        std::cout << std::format("{} packets transmitted, {} received, {:.1f}% packet loss\n",
            transmitted, received, loss);
        
        if (received > 0) {
            std::cout << std::format("rtt min/avg/max/stddev = {:.2f}/{:.2f}/{:.2f}/{:.2f} ms\n",
                stats.min(), stats.mean(), stats.max(), stats.stddev());
            std::cout << std::format("jitter = {:.2f} ms\n", stats.jitter());
        }
    }
    
    return 0;
}

} // namespace netprobe::commands
