#include "ansi.h"
#include "commands/commands.h"
#include <iostream>
#include <format>
#include <string_view>
#include <map>
#include <span>

using namespace netprobe;

void print_usage() {
    std::cout << ansi::colorize("NetProbe v1.0.0", ansi::color::BOLD) 
              << " - Network Diagnostic Toolkit\n\n";
    
    std::cout << "Usage: " << ansi::info("netprobe") << " <command> [options]\n\n";
    
    std::cout << ansi::colorize("Commands:", ansi::color::BOLD) << "\n";
    std::cout << "  " << ansi::info("ping") 
              << "    <host>                Send ICMP echo requests\n";
    std::cout << "  " << ansi::info("trace") 
              << "   <host>                Trace route to host\n";
    std::cout << "  " << ansi::info("scan") 
              << "    <host> <ports>        Scan TCP ports\n";
    std::cout << "  " << ansi::info("bench") 
              << "   <url> <duration>      HTTP benchmark\n";
    std::cout << "  " << ansi::info("sniff") 
              << "   <filter>              Capture packets\n";
    std::cout << "  " << ansi::info("iperf") 
              << "   server|client <host>  Throughput test\n\n";
    
    std::cout << ansi::colorize("Examples:", ansi::color::BOLD) << "\n";
    std::cout << "  netprobe ping google.com -c 10\n";
    std::cout << "  netprobe trace api.github.com\n";
    std::cout << "  netprobe scan localhost 1-1024\n";
    std::cout << "  netprobe bench httpbin.org/get 10s -c 50\n";
    std::cout << "  netprobe sniff tcp -p 443 -c 100\n";
    std::cout << "  netprobe iperf server\n";
    std::cout << "  netprobe iperf client 192.168.1.100\n\n";
    
    std::cout << "For more information, run: netprobe <command> --help\n";
}

void print_version() {
    std::cout << "NetProbe version 1.0.0\n";
    std::cout << "Built with C++20 on " << __DATE__ << "\n";
    std::cout << "License: MIT\n";
}

int main(int argc, char** argv) {
    // Enable ANSI colors if terminal supports it
    ansi::enable_colors(ansi::is_tty());
    
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string_view command = argv[1];
    
    // Handle global flags
    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }
    
    if (command == "--version" || command == "-v") {
        print_version();
        return 0;
    }
    
    // Prepare arguments for command
    std::vector<const char*> args;
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    // Dispatch to command
    std::map<std::string_view, int(*)(std::span<const char*>)> commands = {
        {"ping", commands::ping},
        {"trace", commands::trace},
        {"scan", commands::scan},
        {"bench", commands::bench},
        {"sniff", commands::sniff},
        {"iperf", commands::iperf}
    };
    
    auto it = commands.find(command);
    if (it != commands.end()) {
        try {
            return it->second(args);
        } catch (const std::exception& e) {
            std::cerr << ansi::error(std::format("Error: {}", e.what())) << "\n";
            return 1;
        }
    }
    
    std::cerr << ansi::error(std::format("Unknown command: {}", command)) << "\n\n";
    print_usage();
    return 1;
}
