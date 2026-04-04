#pragma once

#include <casket/transport/transport_base.hpp>
#include <casket/multiplexing/epoll_poller.hpp>

#include <functional>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace casket
{

template <typename Transport>
class GenericServer
{
public:
    using ConnectionHandler = std::function<void(Transport&, const std::vector<uint8_t>&)>;
    using ErrorHandler = std::function<void(const std::error_code&)>;

    GenericServer() = default;

    ~GenericServer()
    {
        stop();
    }

    bool listen(const std::string& address)
    {
        if constexpr (std::is_same_v<Transport, UnixSocket>)
        {
            return listenTransport_.listen(address);
        }
    }

    void setConnectionHandler(ConnectionHandler handler)
    {
        connectionHandler_ = std::move(handler);
    }

    void setErrorHandler(ErrorHandler handler)
    {
        errorHandler_ = std::move(handler);
    }

    void start()
    {
        if (running_)
        {
            return;
        }

        running_ = true;
        serverThread_ = std::thread(&GenericServer::runLoop, this);
    }

    void stop()
    {
        if (!running_)
        {
            return;
        }

        running_ = false;

        if (poller_)
        {
            poller_->destroy();
            poller_.reset();
        }

        if (serverThread_.joinable())
        {
            serverThread_.join();
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (auto& [fd, client] : clients_)
            {
                client->transport->close();
            }
            clients_.clear();
        }

        listenTransport_.close();
    }

    bool isRunning() const
    {
        return running_;
    }

    size_t getClientCount() const
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        return clients_.size();
    }

private:
    struct ClientContext
    {
        std::unique_ptr<Transport> transport;
        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;
        bool isWriting;

        ClientContext(std::unique_ptr<Transport> t)
            : transport(std::move(t))
            , isWriting(false)
        {
            readBuffer.reserve(8192);
            writeBuffer.reserve(8192);
        }

        bool hasDataToWrite() const
        {
            return !writeBuffer.empty();
        }

        int getFd() const
        {
            return transport ? transport->getFd() : -1;
        }
    };

    void runLoop()
    {
        poller_ = std::make_unique<EpollPoller>();

        if (!poller_->isValid())
        {
            if (errorHandler_)
            {
                errorHandler_(std::make_error_code(std::errc::io_error));
            }
            return;
        }

        std::error_code ec;
        poller_->add(listenTransport_.getFd(), EventType::Readable, ec);

        if (ec)
        {
            if (errorHandler_)
            {
                errorHandler_(ec);
            }
            return;
        }

        constexpr int MAX_EVENTS = 64;
        std::vector<PollEvent> events(MAX_EVENTS);

        while (running_)
        {
            std::error_code waitEc;
            int eventCount = poller_->wait(events.data(), MAX_EVENTS, 100, waitEc);

            if (waitEc)
            {
                if (errorHandler_)
                {
                    errorHandler_(waitEc);
                }
                continue;
            }

            for (int i = 0; i < eventCount; ++i)
            {
                const auto& event = events[i];

                if (event.fd == listenTransport_.getFd())
                {
                    acceptNewClient();
                }
                else if (event.userData)
                {
                    auto* ctx = static_cast<ClientContext*>(event.userData);
                    handleClientEvents(ctx, event.revents);
                }
                else
                {
                    handleClientData(event.fd, event.revents);
                }
            }
        }
    }

    void acceptNewClient()
    {
        auto clientTransport = listenTransport_.accept();

        if (!clientTransport.isConnected())
        {
            if (errorHandler_)
            {
                errorHandler_(clientTransport.lastError());
            }
            return;
        }

        int clientFd = clientTransport.getFd();

        auto ctx = std::make_unique<ClientContext>(std::make_unique<Transport>(std::move(clientTransport)));

        std::error_code ec;
        EventType events = EventType::Readable | EventType::HangUp;
        poller_->add(static_cast<void*>(ctx.get()), clientFd, events, ec);

        if (ec)
        {
            if (errorHandler_)
            {
                errorHandler_(ec);
            }
            return;
        }

        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_[clientFd] = std::move(ctx);
    }

    void handleClientEvents(ClientContext* ctx, EventType revents)
    {
        if (!ctx || !ctx->transport)
        {
            return;
        }

        if ((revents & EventType::Error) != EventType::None || (revents & EventType::HangUp) != EventType::None)
        {
            removeClient(ctx->getFd());
            return;
        }

        if ((revents & EventType::Readable) != EventType::None)
        {
            handleClientRead(ctx);
        }

        if ((revents & EventType::Writable) != EventType::None)
        {
            handleClientWrite(ctx);
        }

        updateClientEvents(ctx);
    }

    void handleClientRead(ClientContext* ctx)
    {
        uint8_t buffer[8192];
        std::vector<uint8_t> allData;

        while (true)
        {
            ssize_t received = ctx->transport->recv(buffer, sizeof(buffer));

            if (received > 0)
            {
                allData.insert(allData.end(), buffer, buffer + received);
            }
            else if (received < 0)
            {
                if (ctx->transport->lastError() != std::errc::resource_unavailable_try_again)
                {
                    if (errorHandler_)
                    {
                        errorHandler_(ctx->transport->lastError());
                    }
                    removeClient(ctx->getFd());
                }
                break;
            }
            else if (received == 0)
            {
                removeClient(ctx->getFd());
                return;
            }
        }

        if (!allData.empty() && connectionHandler_)
        {
            connectionHandler_(*ctx->transport, allData);
        }
    }

    void handleClientWrite(ClientContext* ctx)
    {
        if (ctx->writeBuffer.empty())
        {
            return;
        }

        ssize_t sent = ctx->transport->send(ctx->writeBuffer.data(), ctx->writeBuffer.size());

        if (sent > 0)
        {
            ctx->writeBuffer.erase(ctx->writeBuffer.begin(), ctx->writeBuffer.begin() + sent);

            if (ctx->writeBuffer.empty())
            {
                ctx->isWriting = false;
            }
        }
        else if (sent < 0)
        {
            if (ctx->transport->lastError() != std::errc::resource_unavailable_try_again)
            {
                if (errorHandler_)
                {
                    errorHandler_(ctx->transport->lastError());
                }
                removeClient(ctx->getFd());
            }
        }
    }

    void updateClientEvents(ClientContext* ctx)
    {
        if (!ctx || !ctx->transport || !poller_)
        {
            return;
        }

        std::error_code ec;
        EventType events = EventType::Readable | EventType::HangUp;

        if (ctx->hasDataToWrite())
        {
            events |= EventType::Writable;
        }

        poller_->modify(static_cast<void*>(ctx), ctx->getFd(), events, ec);

        if (ec && errorHandler_)
        {
            errorHandler_(ec);
        }
    }

    void handleClientData(int fd, EventType revents)
    {
        std::unique_ptr<ClientContext> ctx;

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = clients_.find(fd);
            if (it == clients_.end())
            {
                return;
            }
            ctx = std::move(it->second);
            clients_.erase(it);
        }

        if (!ctx)
        {
            return;
        }

        handleClientEvents(ctx.get(), revents);

        if (ctx->transport && ctx->transport->isConnected())
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_[fd] = std::move(ctx);
        }
    }

    void removeClient(int fd)
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(fd);
        if (it != clients_.end())
        {
            if (poller_)
            {
                std::error_code ec;
                poller_->remove(fd, ec);
            }
            clients_.erase(it);
        }
    }

    void sendToClient(int fd, const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(fd);
        if (it != clients_.end())
        {
            it->second->writeBuffer.insert(it->second->writeBuffer.end(), data.begin(), data.end());
            it->second->isWriting = true;

            if (poller_ && it->second->transport)
            {
                std::error_code ec;
                EventType events = EventType::Readable | EventType::HangUp | EventType::Writable;
                poller_->modify(static_cast<void*>(it->second.get()), fd, events, ec);
            }
        }
    }

    Transport listenTransport_;
    ConnectionHandler connectionHandler_;
    ErrorHandler errorHandler_;

    std::unique_ptr<EpollPoller> poller_;
    std::atomic<bool> running_{false};
    std::thread serverThread_;

    std::unordered_map<int, std::unique_ptr<ClientContext>> clients_;
    mutable std::mutex clientsMutex_;
};

} // namespace casket