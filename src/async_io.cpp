#include "async_io.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <format>

namespace netprobe {

AsyncIO::AsyncIO() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        // Handle error silently - check in methods
    }
}

AsyncIO::~AsyncIO() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

Result<void> AsyncIO::add(int fd, Event events, Callback callback) {
    if (epoll_fd_ < 0) {
        return Result<void>("AsyncIO not initialized");
    }
    
    struct epoll_event ev{};
    ev.events = 0;
    
    if (static_cast<int>(events) & static_cast<int>(Event::READ)) {
        ev.events |= EPOLLIN;
    }
    if (static_cast<int>(events) & static_cast<int>(Event::WRITE)) {
        ev.events |= EPOLLOUT;
    }
    
    size_t index = events_.size();
    ev.data.u64 = index;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return Result<void>(std::format("epoll_ctl ADD failed: {}", 
            std::strerror(errno)));
    }
    
    events_.push_back({fd, std::move(callback)});
    return Result<void>();
}

Result<void> AsyncIO::modify(int fd, Event events) {
    if (epoll_fd_ < 0) {
        return Result<void>("AsyncIO not initialized");
    }
    
    struct epoll_event ev{};
    ev.events = 0;
    
    if (static_cast<int>(events) & static_cast<int>(Event::READ)) {
        ev.events |= EPOLLIN;
    }
    if (static_cast<int>(events) & static_cast<int>(Event::WRITE)) {
        ev.events |= EPOLLOUT;
    }
    
    // Find event data
    auto it = std::ranges::find_if(events_, [fd](const auto& e) { return e.fd == fd; });
    if (it == events_.end()) {
        return Result<void>("Socket not registered");
    }
    
    ev.data.u64 = std::distance(events_.begin(), it);
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return Result<void>(std::format("epoll_ctl MOD failed: {}", 
            std::strerror(errno)));
    }
    
    return Result<void>();
}

Result<void> AsyncIO::remove(int fd) {
    if (epoll_fd_ < 0) {
        return Result<void>("AsyncIO not initialized");
    }
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        return Result<void>(std::format("epoll_ctl DEL failed: {}", 
            std::strerror(errno)));
    }
    
    // Remove from events vector
    auto it = std::ranges::find_if(events_, [fd](const auto& e) { return e.fd == fd; });
    if (it != events_.end()) {
        events_.erase(it);
    }
    
    return Result<void>();
}

void AsyncIO::run_once(std::chrono::milliseconds timeout) {
    if (epoll_fd_ < 0) return;
    
    constexpr size_t MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    
    int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 
        static_cast<int>(timeout.count()));
    
    if (nfds < 0) {
        if (errno != EINTR) {
            // Error occurred
        }
        return;
    }
    
    for (int i = 0; i < nfds; ++i) {
        size_t index = events[i].data.u64;
        if (index >= events_.size()) continue;
        
        Event event_type = Event::READ;
        if (events[i].events & EPOLLIN) {
            event_type = Event::READ;
        } else if (events[i].events & EPOLLOUT) {
            event_type = Event::WRITE;
        } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
            event_type = Event::ERROR;
        }
        
        events_[index].callback(events_[index].fd, event_type);
    }
}

void AsyncIO::run() {
    running_ = true;
    while (running_) {
        run_once();
    }
}

void AsyncIO::stop() {
    running_ = false;
}

} // namespace netprobe
