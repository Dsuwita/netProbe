#include "commands.h"
#include "../socket.h"
#include "../ansi.h"
#include "../argparse.h"
#include <iostream>
#include <format>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <iomanip>

namespace netprobe::commands {

namespace {

std::string protocol_name(uint8_t protocol) {
    switch (protocol) {
        case IPPROTO_TCP: return "TCP";
        case IPPROTO_UDP: return "UDP";
        case IPPROTO_ICMP: return "ICMP";
        default: return std::format("{}", protocol);
    }
}

void print_packet(const uint8_t* data, size_t len, bool verbose) {
    if (len < sizeof(iphdr)) return;
    
    const auto* ip = reinterpret_cast<const iphdr*>(data);
    
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip->saddr, src, sizeof(src));
    inet_ntop(AF_INET, &ip->daddr, dst, sizeof(dst));
    
    std::string proto = protocol_name(ip->protocol);
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    
    size_t ip_header_len = ip->ihl * 4;
    const uint8_t* transport = data + ip_header_len;
    
    if (ip->protocol == IPPROTO_TCP && len >= ip_header_len + sizeof(tcphdr)) {
        const auto* tcp = reinterpret_cast<const tcphdr*>(transport);
        src_port = ntohs(tcp->source);
        dst_port = ntohs(tcp->dest);
    } else if (ip->protocol == IPPROTO_UDP && len >= ip_header_len + sizeof(udphdr)) {
        const auto* udp = reinterpret_cast<const udphdr*>(transport);
        src_port = ntohs(udp->source);
        dst_port = ntohs(udp->dest);
    }
    
    std::cout << std::format("{} {}:{} → {}:{} len={}",
        ansi::info(proto), src, src_port, dst, dst_port, ntohs(ip->tot_len));
    
    if (verbose && len > ip_header_len) {
        size_t payload_offset = ip_header_len;
        if (ip->protocol == IPPROTO_TCP) {
            const auto* tcp = reinterpret_cast<const tcphdr*>(transport);
            payload_offset += tcp->doff * 4;
        } else if (ip->protocol == IPPROTO_UDP) {
            payload_offset += sizeof(udphdr);
        }
        
        if (len > payload_offset) {
            std::cout << " [";
            size_t print_len = std::min(len - payload_offset, size_t{16});
            for (size_t i = 0; i < print_len; ++i) {
                std::cout << std::format("{:02x}", data[payload_offset + i]);
                if (i < print_len - 1) std::cout << " ";
            }
            if (len - payload_offset > 16) std::cout << "...";
            std::cout << "]";
        }
    }
    
    std::cout << "\n";
}

} // anonymous namespace

int sniff(std::span<const char*> args) {
    ArgParser parser("Capture and display network packets");
    parser.add_positional("filter", "Protocol filter (tcp/udp/icmp)");
    parser.add_option("port", "p", "Filter by port", "");
    parser.add_option("count", "c", "Number of packets to capture", "0");
    parser.add_flag("verbose", "v", "Verbose output with payload hex");
    
    auto parse_result = parser.parse(args);
    if (!parse_result) {
        std::cerr << ansi::error(parse_result.error) << "\n";
        return 1;
    }
    
    auto positional = parser.get_positional();
    std::string filter = positional.empty() ? "tcp" : positional[0];
    
    uint16_t filter_port = parser.get_as<uint16_t>("port").value_or(0);
    size_t count = parser.get_as<size_t>("count").value_or(0);
    bool verbose = parser.get_flag("verbose");
    
    uint8_t proto_filter = 0;
    if (filter == "tcp") proto_filter = IPPROTO_TCP;
    else if (filter == "udp") proto_filter = IPPROTO_UDP;
    else if (filter == "icmp") proto_filter = IPPROTO_ICMP;
    
    // Create raw socket
    int sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sock_fd < 0) {
        std::cerr << ansi::error("Failed to create raw socket (try running with sudo)") << "\n";
        return 1;
    }
    
    Socket sock(sock_fd);
    
    std::cout << ansi::info(std::format("Capturing {} packets", 
        filter.empty() ? "all" : filter));
    if (filter_port > 0) {
        std::cout << ansi::info(std::format(" on port {}", filter_port));
    }
    std::cout << ansi::info(std::format(" ({})\n\n", 
        count > 0 ? std::format("{} packets", count) : "press Ctrl+C to stop"));
    
    size_t captured = 0;
    uint8_t buffer[MAX_PACKET_SIZE];
    
    while (count == 0 || captured < count) {
        auto recv_result = sock.recv(buffer, sizeof(buffer));
        if (!recv_result) continue;
        
        size_t len = *recv_result;
        
        // Skip Ethernet header
        if (len < sizeof(ethhdr)) continue;
        const uint8_t* ip_packet = buffer + sizeof(ethhdr);
        len -= sizeof(ethhdr);
        
        if (len < sizeof(iphdr)) continue;
        
        const auto* ip = reinterpret_cast<const iphdr*>(ip_packet);
        
        // Filter by protocol
        if (proto_filter != 0 && ip->protocol != proto_filter) {
            continue;
        }
        
        // Filter by port
        if (filter_port > 0) {
            size_t ip_header_len = ip->ihl * 4;
            if (len < ip_header_len + 4) continue;
            
            const uint8_t* transport = ip_packet + ip_header_len;
            uint16_t src_port = ntohs(*reinterpret_cast<const uint16_t*>(transport));
            uint16_t dst_port = ntohs(*reinterpret_cast<const uint16_t*>(transport + 2));
            
            if (src_port != filter_port && dst_port != filter_port) {
                continue;
            }
        }
        
        print_packet(ip_packet, len, verbose);
        captured++;
    }
    
    std::cout << "\n" << ansi::success(std::format("Captured {} packets\n", captured));
    
    return 0;
}

} // namespace netprobe::commands
