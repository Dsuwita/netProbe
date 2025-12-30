FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-11 \
    libstdc++-11-dev \
    && rm -rf /var/lib/apt/lists/*

# Set C++20 compiler
ENV CXX=g++-11
ENV CC=gcc-11

WORKDIR /build

# Copy source
COPY . .

# Build
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc) \
    && strip build/netprobe

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies (minimal)
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Copy binary
COPY --from=builder /build/build/netprobe /usr/local/bin/netprobe
COPY --from=builder /build/man/netprobe.1 /usr/local/share/man/man1/

# Grant network capabilities
RUN setcap cap_net_raw+ep /usr/local/bin/netprobe || true

# Create non-root user (some commands need root)
RUN useradd -m -s /bin/bash netprobe

WORKDIR /home/netprobe

# Default command
ENTRYPOINT ["/usr/local/bin/netprobe"]
CMD ["--help"]
