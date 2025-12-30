#include "socket.h"
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <cstring>
#include <format>

namespace netprobe {

Socket::Socket(Type type) {
    auto res = create(type);
    // Ignore errors in constructor
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

Result<void> Socket::create(Type type) {
    int domain = AF_INET;
    int sock_type = SOCK_STREAM;
    int protocol = 0;
    
    switch (type) {
        case Type::TCP:
            sock_type = SOCK_STREAM;
            protocol = IPPROTO_TCP;
            break;
        case Type::UDP:
            sock_type = SOCK_DGRAM;
            protocol = IPPROTO_UDP;
            break;
        case Type::ICMP:
            sock_type = SOCK_RAW;
            protocol = IPPROTO_ICMP;
            break;
        case Type::RAW:
            sock_type = SOCK_RAW;
            protocol = IPPROTO_RAW;
            break;
    }
    
    fd_ = ::socket(domain, sock_type, protocol);
    if (fd_ < 0) {
        return Result<void>(std::format("Failed to create socket: {}", 
            std::strerror(errno)));
    }
    
    return Result<void>();
}

Result<void> Socket::connect(std::string_view host, uint16_t port, 
                             std::chrono::milliseconds timeout) {
    auto addr_result = resolve(host, port);
    if (!addr_result) {
        return Result<void>(addr_result.error);
    }
    
    auto& addr = *addr_result;
    
    // Set non-blocking for timeout support
    if (timeout.count() > 0) {
        if (auto res = set_nonblocking(true); !res) {
            return res;
        }
    }
    
    int result = ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    if (result < 0 && errno != EINPROGRESS) {
        return Result<void>(std::format("Connect failed: {}", 
            std::strerror(errno)));
    }
    
    if (timeout.count() > 0 && errno == EINPROGRESS) {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(fd_, &write_fds);
        
        struct timeval tv;
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;
        
        result = ::select(fd_ + 1, nullptr, &write_fds, nullptr, &tv);
        if (result <= 0) {
            return Result<void>("Connection timeout");
        }
        
        int error;
        socklen_t len = sizeof(error);
        if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            return Result<void>(std::format("Connection failed: {}", 
                std::strerror(error)));
        }
        
        set_nonblocking(false);
    }
    
    return Result<void>();
}

Result<void> Socket::bind(uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return Result<void>(std::format("Bind failed: {}", 
            std::strerror(errno)));
    }
    
    return Result<void>();
}

Result<void> Socket::listen(int backlog) {
    if (::listen(fd_, backlog) < 0) {
        return Result<void>(std::format("Listen failed: {}", 
            std::strerror(errno)));
    }
    return Result<void>();
}

Result<Socket> Socket::accept() {
    int client_fd = ::accept(fd_, nullptr, nullptr);
    if (client_fd < 0) {
        return Result<Socket>(std::format("Accept failed: {}", 
            std::strerror(errno)));
    }
    return Socket(client_fd);
}

Result<size_t> Socket::send(const void* data, size_t len) {
    ssize_t sent = ::send(fd_, data, len, 0);
    if (sent < 0) {
        return Result<size_t>(std::format("Send failed: {}", 
            std::strerror(errno)));
    }
    return static_cast<size_t>(sent);
}

Result<size_t> Socket::recv(void* buffer, size_t len) {
    ssize_t received = ::recv(fd_, buffer, len, 0);
    if (received < 0) {
        return Result<size_t>(std::format("Recv failed: {}", 
            std::strerror(errno)));
    }
    return static_cast<size_t>(received);
}

Result<size_t> Socket::sendto(const void* data, size_t len,
                              const sockaddr* addr, socklen_t addrlen) {
    ssize_t sent = ::sendto(fd_, data, len, 0, addr, addrlen);
    if (sent < 0) {
        return Result<size_t>(std::format("Sendto failed: {}", 
            std::strerror(errno)));
    }
    return static_cast<size_t>(sent);
}

Result<size_t> Socket::recvfrom(void* buffer, size_t len,
                                sockaddr* addr, socklen_t* addrlen) {
    ssize_t received = ::recvfrom(fd_, buffer, len, 0, addr, addrlen);
    if (received < 0) {
        return Result<size_t>(std::format("Recvfrom failed: {}", 
            std::strerror(errno)));
    }
    return static_cast<size_t>(received);
}

Result<void> Socket::set_nonblocking(bool enabled) {
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        return Result<void>("Failed to get socket flags");
    }
    
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (::fcntl(fd_, F_SETFL, flags) < 0) {
        return Result<void>("Failed to set non-blocking mode");
    }
    
    return Result<void>();
}

Result<void> Socket::set_reuse_addr(bool enabled) {
    int opt = enabled ? 1 : 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return Result<void>("Failed to set SO_REUSEADDR");
    }
    return Result<void>();
}

Result<void> Socket::set_reuse_port(bool enabled) {
    int opt = enabled ? 1 : 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        return Result<void>("Failed to set SO_REUSEPORT");
    }
    return Result<void>();
}

Result<void> Socket::set_timeout(std::chrono::milliseconds timeout) {
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    
    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return Result<void>("Failed to set receive timeout");
    }
    
    if (::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return Result<void>("Failed to set send timeout");
    }
    
    return Result<void>();
}

Result<void> Socket::set_ttl(int ttl) {
    if (::setsockopt(fd_, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        return Result<void>("Failed to set TTL");
    }
    return Result<void>();
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Result<sockaddr_in> Socket::resolve(std::string_view host, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    // Try direct IP address first
    if (inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr) == 1) {
        return addr;
    }
    
    // DNS lookup
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    int ret = getaddrinfo(std::string(host).c_str(), nullptr, &hints, &result);
    if (ret != 0) {
        return Result<sockaddr_in>(std::format("DNS lookup failed: {}", 
            gai_strerror(ret)));
    }
    
    if (result == nullptr) {
        return Result<sockaddr_in>("No addresses found");
    }
    
    addr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
    addr.sin_port = htons(port);
    
    freeaddrinfo(result);
    return addr;
}

std::string Socket::addr_to_string(const sockaddr_in& addr) {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::format("{}:{}", buf, ntohs(addr.sin_port));
}

} // namespace netprobe
