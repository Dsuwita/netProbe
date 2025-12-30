#pragma once

#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace netprobe {

// RAII socket wrapper
class Socket {
public:
    enum class Type {
        TCP,
        UDP,
        ICMP,
        RAW
    };
    
    Socket() = default;
    explicit Socket(Type type);
    Socket(int fd) : fd_(fd) {}
    ~Socket();
    
    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // Movable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    // Create socket
    Result<void> create(Type type);
    
    // Connect
    Result<void> connect(std::string_view host, uint16_t port, 
                        std::chrono::milliseconds timeout = 1000ms);
    
    // Bind
    Result<void> bind(uint16_t port);
    
    // Listen
    Result<void> listen(int backlog = 128);
    
    // Accept
    Result<Socket> accept();
    
    // Send/receive
    Result<size_t> send(const void* data, size_t len);
    Result<size_t> recv(void* buffer, size_t len);
    Result<size_t> sendto(const void* data, size_t len, 
                         const sockaddr* addr, socklen_t addrlen);
    Result<size_t> recvfrom(void* buffer, size_t len,
                           sockaddr* addr, socklen_t* addrlen);
    
    // Options
    Result<void> set_nonblocking(bool enabled);
    Result<void> set_reuse_addr(bool enabled);
    Result<void> set_reuse_port(bool enabled);
    Result<void> set_timeout(std::chrono::milliseconds timeout);
    Result<void> set_ttl(int ttl);
    
    // Get info
    int fd() const { return fd_; }
    bool is_valid() const { return fd_ >= 0; }
    
    // Close
    void close();
    
    // Static helpers
    static Result<sockaddr_in> resolve(std::string_view host, uint16_t port);
    static std::string addr_to_string(const sockaddr_in& addr);

private:
    int fd_ = -1;
};

} // namespace netprobe
