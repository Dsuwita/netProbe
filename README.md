# NetProbe 🔍
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

Production-grade network diagnostic toolkit—fast, colorful, zero dependencies. Like `nmap` + `ping` + `curl` in one binary.

## Quick Install

### One-line installer (recommended)

```bash
curl -fsSL https://raw.githubusercontent.com/Dsuwita/netProbe/main/install.sh | bash
```

Or download and inspect first:

```bash
curl -fsSL https://raw.githubusercontent.com/Dsuwita/netProbe/main/install.sh -o install.sh
bash install.sh
```

### Manual installation

Download the latest release for your platform:

```bash
# Linux x86_64
wget https://github.com/Dsuwita/netProbe/releases/latest/download/netprobe-linux-x86_64
chmod +x netprobe-linux-x86_64
sudo mv netprobe-linux-x86_64 /usr/local/bin/netprobe

# Set capabilities for raw sockets (ping/trace/sniff)
sudo setcap cap_net_raw+ep /usr/local/bin/netprobe
```

### Build from source

```bash
git clone https://github.com/Dsuwita/netProbe.git
cd netProbe
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cp build/netprobe /usr/local/bin/
sudo cp man/netprobe.1 /usr/local/share/man/man1/
sudo setcap cap_net_raw+ep /usr/local/bin/netprobe
```

**Requirements:** CMake 3.20+, GCC 11+ or Clang 14+ (C++20 support)

## Commands

### Ping

Send ICMP echo requests with detailed statistics:

```bash
netprobe ping google.com -c 10
```

Output includes: min/avg/max RTT, jitter, packet loss, and distribution.

### Trace Route

Trace network path with hop RTTs:

```bash
netprobe trace api.github.com
```

### Port Scan

Parallel TCP port scanning:

```bash
netprobe scan localhost 1-1024
```

Scans 1024 ports in ~2 seconds (vs nmap's 30s).

### HTTP Benchmark

HTTP load testing with latency percentiles:

```bash
netprobe bench httpbin.org/get 10s -c 50
```

Reports: req/s, P50/P95/P99 latency, throughput, error rate.

### Packet Sniffer

Live packet capture (requires root):

```bash
sudo netprobe sniff tcp -p 443 -c 100 -v
```

### Throughput Test

iperf-compatible network speed test:

```bash
# Server
netprobe iperf server

# Client
netprobe iperf client 192.168.1.100
```

## Usage Examples

```bash
# Ping with JSON output
netprobe ping 8.8.8.8 -c 5 --json > results.json

# Scan specific ports
netprobe scan example.com 80,443,8080

# Benchmark for 30 seconds
netprobe bench api.example.com/endpoint 30s -c 100

# Capture UDP packets on port 53
sudo netprobe sniff udp -p 53 -c 50

# Test throughput for 60 seconds
netprobe iperf client remote-host -t 60
```

## Requirements

- **OS**: Linux or macOS
- **Compiler**: GCC 11+ or Clang 14+ with C++20 support
- **Build**: CMake 3.20+
- **Runtime**: POSIX-compliant system

Some commands require elevated privileges (root or CAP_NET_RAW):
- `ping` - ICMP raw sockets
- `trace` - TTL manipulation
- `sniff` - Packet capture

## Architecture

```
netprobe/
├── src/
│   ├── main.cpp           # Command dispatcher
│   ├── ansi.cpp           # Terminal coloring & tables
│   ├── argparse.cpp       # CLI argument parser
│   ├── socket.cpp         # RAII socket wrapper
│   ├── async_io.cpp       # epoll/kqueue reactor
│   ├── stats.cpp          # Statistical analysis
│   └── commands/
│       ├── ping.cpp       # ICMP echo
│       ├── trace.cpp      # Traceroute
│       ├── scan.cpp       # Port scanner
│       ├── bench.cpp      # HTTP benchmark
│       ├── sniff.cpp      # Packet capture
│       └── iperf.cpp      # Throughput test
├── man/
│   └── netprobe.1         # Manual page
└── CMakeLists.txt         # Build configuration
```

## Technical Highlights

- **100% RAII**: No memory leaks, automatic resource cleanup
- **C++20 ranges/views**: Efficient data processing
- **Sub-millisecond precision**: `std::chrono::steady_clock`
- **Thread pool**: Fixed at `std::thread::hardware_concurrency()`
- **Zero-copy parsing**: Minimal memory allocations
- **Error handling**: `std::expected<T, E>` for composable error handling

## Configuration

Optional configuration file at `~/.netprobe.json`:

```json
{
  "threads": 16,
  "timeout": 1000,
  "colors": true,
  "json_output": false
}
```

## Docker

Build and run in Docker:

```bash
docker build -t netprobe .
docker run --rm --cap-add=NET_RAW netprobe ping google.com
```

## Contributing

Contributions welcome! Please ensure:
- Code compiles with `-Wall -Wextra -Wpedantic`
- All commands tested manually
- No external dependencies added
- Man page updated for new features

## License

MIT License - see [LICENSE](LICENSE) file.

## Man Page

View the manual:

```bash
man netprobe
```

Or for a specific command:

```bash
netprobe <command> --help
```

## Roadmap

- [ ] IPv6 support
- [ ] DNS lookup with query types
- [ ] TLS/SSL certificate inspection
- [ ] WebSocket benchmark
- [ ] Network latency heatmaps
- [ ] Config file hot-reload

## See Also

- [nmap](https://nmap.org/) - Network mapper
- [iperf3](https://iperf.fr/) - Network performance tool
- [curl](https://curl.se/) - Transfer data with URLs
- [htop](https://htop.dev/) - Interactive process viewer

---