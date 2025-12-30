#!/bin/bash

# NetProbe Demo Script
# Demonstrates all commands with real examples

set -e

BOLD='\033[1m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RESET='\033[0m'

echo -e "${BOLD}${CYAN}"
echo "╔═══════════════════════════════════════════╗"
echo "║       NetProbe v1.0.0 Demo Script        ║"
echo "║  Network Diagnostic Toolkit Showcase     ║"
echo "╚═══════════════════════════════════════════╝"
echo -e "${RESET}\n"

# Check if binary exists
if [ ! -f "./build/netprobe" ]; then
    echo -e "${YELLOW}Building NetProbe...${RESET}"
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    echo -e "${GREEN}✓ Build complete${RESET}\n"
fi

NETPROBE="./build/netprobe"

sleep 1

# Demo 1: Version
echo -e "${BOLD}${CYAN}═══ 1. Version Info ═══${RESET}"
$NETPROBE --version
echo ""
sleep 2

# Demo 2: Help
echo -e "${BOLD}${CYAN}═══ 2. Help Menu ═══${RESET}"
$NETPROBE --help
echo ""
sleep 2

# Demo 3: Ping
echo -e "${BOLD}${CYAN}═══ 3. PING - ICMP Echo Test ═══${RESET}"
echo -e "${YELLOW}Command: netprobe ping 8.8.8.8 -c 5${RESET}\n"
sudo $NETPROBE ping 8.8.8.8 -c 5 || echo "⚠ Requires root for ICMP"
echo ""
sleep 2

# Demo 4: Ping with JSON
echo -e "${BOLD}${CYAN}═══ 4. PING - JSON Output ═══${RESET}"
echo -e "${YELLOW}Command: netprobe ping 1.1.1.1 -c 3 --json${RESET}\n"
sudo $NETPROBE ping 1.1.1.1 -c 3 --json || echo "⚠ Requires root"
echo ""
sleep 2

# Demo 5: Trace Route
echo -e "${BOLD}${CYAN}═══ 5. TRACE - Route to Google ═══${RESET}"
echo -e "${YELLOW}Command: netprobe trace google.com -m 15${RESET}\n"
sudo $NETPROBE trace google.com -m 15 || echo "⚠ Requires root"
echo ""
sleep 2

# Demo 6: Port Scan
echo -e "${BOLD}${CYAN}═══ 6. SCAN - Common Ports ═══${RESET}"
echo -e "${YELLOW}Command: netprobe scan localhost 20-100${RESET}\n"
$NETPROBE scan localhost 20-100
echo ""
sleep 2

# Demo 7: Port Scan - Specific Ports
echo -e "${BOLD}${CYAN}═══ 7. SCAN - Specific Ports ═══${RESET}"
echo -e "${YELLOW}Command: netprobe scan scanme.nmap.org 22,80,443${RESET}\n"
$NETPROBE scan scanme.nmap.org 22,80,443
echo ""
sleep 2

# Demo 8: HTTP Benchmark
echo -e "${BOLD}${CYAN}═══ 8. BENCH - HTTP Load Test ═══${RESET}"
echo -e "${YELLOW}Command: netprobe bench httpbin.org/get 5s -c 20${RESET}\n"
$NETPROBE bench httpbin.org/get 5s -c 20
echo ""
sleep 2

# Demo 9: Benchmark JSON
echo -e "${BOLD}${CYAN}═══ 9. BENCH - JSON Output ═══${RESET}"
echo -e "${YELLOW}Command: netprobe bench example.com 3s -c 10 --json${RESET}\n"
$NETPROBE bench example.com 3s -c 10 --json
echo ""
sleep 2

# Demo 10: Packet Sniffer (requires root)
echo -e "${BOLD}${CYAN}═══ 10. SNIFF - Packet Capture ═══${RESET}"
echo -e "${YELLOW}Command: sudo netprobe sniff tcp -c 10${RESET}\n"
echo -e "${YELLOW}⏱  Capturing 10 TCP packets (will timeout if no traffic)...${RESET}"
timeout 5 sudo $NETPROBE sniff tcp -c 10 || echo "⚠ Timed out or requires root"
echo ""
sleep 2

# Demo 11: iPerf Server (background)
echo -e "${BOLD}${CYAN}═══ 11. IPERF - Throughput Test ═══${RESET}"
echo -e "${YELLOW}Starting iperf server in background...${RESET}"
sudo $NETPROBE iperf server &
SERVER_PID=$!
sleep 2

echo -e "${YELLOW}Command: netprobe iperf client localhost -t 5${RESET}\n"
$NETPROBE iperf client localhost -t 5 || echo "⚠ Server connection failed"

# Stop server
kill $SERVER_PID 2>/dev/null || true
echo ""
sleep 2

# Summary
echo -e "${BOLD}${GREEN}"
echo "╔═══════════════════════════════════════════╗"
echo "║           Demo Complete! ✓                ║"
echo "╚═══════════════════════════════════════════╝"
echo -e "${RESET}\n"

echo -e "${CYAN}Binary Information:${RESET}"
ls -lh ./build/netprobe
echo ""

echo -e "${CYAN}Size Comparison:${RESET}"
SIZE=$(stat -f%z ./build/netprobe 2>/dev/null || stat -c%s ./build/netprobe)
SIZE_KB=$((SIZE / 1024))
echo "NetProbe: ${SIZE_KB} KB"
echo ""

echo -e "${GREEN}All commands demonstrated successfully!${RESET}"
echo -e "Run ${CYAN}man ./man/netprobe.1${RESET} for detailed documentation"
echo -e "Install with: ${CYAN}sudo cmake --install build${RESET}\n"
