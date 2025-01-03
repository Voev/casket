#pragma once
#include <map>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#include <unistd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#include <unistd.h>
#endif

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

#ifdef _WIN32
    HANDLE iocp;
#elif defined(__linux__)
    int epoll_fd;
    static const int MAX_EVENTS = 10;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    int kqueue_fd;
    static const int MAX_EVENTS = 10;
#endif

public:
    Reactor()
    {
#ifdef _WIN32
        iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (!iocp)
        {
            std::cerr << "Failed to create IOCP\n";
            exit(EXIT_FAILURE);
        }
#elif defined(__linux__)
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1)
        {
            std::cerr << "Failed to create epoll file descriptor\n";
            exit(EXIT_FAILURE);
        }
#elif defined(__APPLE__) || defined(__FreeBSD__)
        kqueue_fd = kqueue();
        if (kqueue_fd == -1)
        {
            std::cerr << "Failed to create kqueue\n";
            exit(EXIT_FAILURE);
        }
#endif
    }

    ~Reactor()
    {
#ifdef _WIN32
        // Windows специфическое освобождение ресурс
#elif defined(__linux__)
        close(epoll_fd);
#elif defined(__APPLE__) || defined(__FreeBSD__)
        close(kqueue_fd);
#endif
    }

    void register_handler(int fd, EventHandler* handler)
    {
        handlers[fd] = handler;

#ifdef _WIN32
        if (!CreateIoCompletionPort((HANDLE)fd, iocp, (ULONG_PTR)fd, 0))
        {
            std::cerr << "Failed to associate socket with IOCP\n";
            exit(EXIT_FAILURE);
        }
#elif defined(__linux__)
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1)
        {
            std::cerr << "Failed to add fd to epoll\n";
            exit(EXIT_FAILURE);
        }
#elif defined(__APPLE__) || defined(__FreeBSD__)
        struct kevent change;
        EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (kevent(kqueue_fd, &change, 1, nullptr, 0, nullptr) == -1)
        {
            std::cerr << "Failed to add fd to kqueue\n";
            exit(EXIT_FAILURE);
        }
#endif
    }

    void event_loop()
    {
#ifdef _WIN32
        while (true)
        {
            DWORD bytesTransferred;
            ULONG_PTR completionKey;
            LPOVERLAPPED overlapped;

            BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);
            int sock = (int)completionKey;
            if (result && handlers.count(sock) > 0)
            {
                handlers[sock]->handle_event(sock);
            }
        }
#elif defined(__linux__)
        struct epoll_event events[MAX_EVENTS];
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
                if (handlers.count(fd) > 0)
                {
                    handlers[fd]->handle_event(fd);
                }
            }
        }
#elif defined(__APPLE__) || defined(__FreeBSD__)
        struct kevent events[MAX_EVENTS];
        while (true)
        {
            int ready_fds = kevent(kqueue_fd, NULL, 0, events, MAX_EVENTS, NULL);
            if (ready_fds == -1)
            {
                std::cerr << "kevent() error\n";
                exit(EXIT_FAILURE);
            }
            for (int i = 0; i < ready_fds; ++i)
            {
                int fd = events[i].ident;
                if (handlers.count(fd) > 0)
                {
                    handlers[fd]->handle_event(fd);
                }
            }
        }
#endif
    }
};

} // namespace casket::event