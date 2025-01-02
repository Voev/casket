#pragma once
#include <map>

#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <casket/thread/pool.hpp>

namespace casket::event
{

class EventHandler
{
public:
    virtual void handle_event(int fd) = 0;
};

class Reactor
{
private:
    std::map<int, EventHandler*> handlers;
    int epoll_fd;
    thread::ThreadPool thread_pool;
    static const int MAX_EVENTS = 10;

public:
    Reactor(size_t threads)
        : thread_pool(threads)
    {
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1)
        {
            std::cerr << "Failed to create epoll file descriptor\n";
            exit(EXIT_FAILURE);
        }
    }

    ~Reactor()
    {
        close(epoll_fd);
    }

    void register_handler(int fd, EventHandler* handler)
    {
        handlers[fd] = handler;
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN; // Monitor for input events
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1)
        {
            std::cerr << "Failed to add file descriptor to epoll\n";
            exit(EXIT_FAILURE);
        }
    }

    void remove_handler(int fd)
    {
        handlers.erase(fd);
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1)
        {
            std::cerr << "Failed to remove file descriptor from epoll\n";
            exit(EXIT_FAILURE);
        }
    }

    void event_loop()
    {
        epoll_event events[MAX_EVENTS];
        while (true)
        {
            int ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
            if (ready_fds == -1)
            {
                std::cerr << "epoll_wait() error\n";
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < ready_fds; ++i)
            {
                int fd = events[i].data.fd;
                auto it = handlers.find(fd);
                if (it != handlers.end())
                {
                    thread_pool.enqueue([handler = it->second, fd] { handler->handle_event(fd); });
                }
            }
        }
    }
};

} // namespace casket::event