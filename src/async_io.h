#pragma once

#include "common.h"
#include "socket.h"
#include <functional>
#include <vector>

namespace netprobe {

// Async I/O event loop
class AsyncIO {
public:
    enum class Event {
        READ = 1,
        WRITE = 2,
        ERROR = 4
    };
    
    using Callback = std::function<void(int fd, Event event)>;
    
    AsyncIO();
    ~AsyncIO();
    
    // Register socket for events
    Result<void> add(int fd, Event events, Callback callback);
    Result<void> modify(int fd, Event events);
    Result<void> remove(int fd);
    
    // Run event loop
    void run_once(std::chrono::milliseconds timeout = 100ms);
    void run();
    void stop();

private:
    int epoll_fd_ = -1;
    bool running_ = false;
    
    struct EventData {
        int fd;
        Callback callback;
    };
    
    std::vector<EventData> events_;
};

} // namespace netprobe
