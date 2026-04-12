#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

#include <casket/transport/transport_base.hpp>
#include <casket/multiplexing/epoll_poller.hpp>
#include <casket/types/byte_buffer.hpp>

namespace casket
{

template <typename Transport>
class GenericServer
{
public:
    using ConnectionHandler = std::function<void(ByteBuffer&, ByteBuffer&)>;
    using ErrorHandler = std::function<void(const std::error_code&)>;

    struct Config
    {
        size_t bufferSize{8192};
        std::chrono::seconds idleTimeout{60};
        int maxEvents{64};
        int waitTimeoutMs{100};
    };

    explicit GenericServer(const Config& config = Config{})
        : config_(config)
    {
    }

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
        /*else if constexpr (std::is_same_v<Transport, TcpSocket>)
        {
            return listenTransport_.listen(address);
        }*/
        return false;
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
        if (!init())
        {
            return;
        }
        running_ = true;
    }

    bool step()
    {
        if (!initialized_ || !running_)
        {
            return false;
        }

        std::error_code ec;
        int eventCount = poller_->wait(events_.data(), events_.size(), config_.waitTimeoutMs, ec);

        if (ec)
        {
            if (errorHandler_)
            {
                errorHandler_(ec);
            }
            return running_;
        }

        for (int i = 0; i < eventCount; ++i)
        {
            const auto& event = events_[i];

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
        cleanupIdleConnections();

        return running_;
    }

    void run()
    {
        start();

        while (running_)
        {
            if (!step())
            {
                break;
            }
        }
    }

    void stop()
    {
        running_ = false;

        if (poller_)
        {
            poller_->destroy();
            poller_.reset();
        }

        for (auto& [fd, client] : clients_)
        {
            client->transport->close();
        }
        clients_.clear();

        listenTransport_.close();
        initialized_ = false;
    }

    bool isRunning() const
    {
        return running_;
    }

    size_t getClientCount() const
    {
        return clients_.size();
    }

private:
    struct ClientContext
    {
        std::unique_ptr<Transport> transport;
        std::unique_ptr<ByteBuffer> readBuffer;
        std::unique_ptr<ByteBuffer> writeBuffer;
        std::chrono::steady_clock::time_point lastActivity;
        bool isWriting;

        ClientContext(std::unique_ptr<Transport> t, std::unique_ptr<ByteBuffer> readBuf,
                      std::unique_ptr<ByteBuffer> writeBuf)
            : transport(std::move(t))
            , readBuffer(std::move(readBuf))
            , writeBuffer(std::move(writeBuf))
            , lastActivity(std::chrono::steady_clock::now())
            , isWriting(false)
        {
        }

        bool hasDataToWrite() const
        {
            return (writeBuffer && writeBuffer->readable() > 0);
        }

        int getFd() const
        {
            return transport ? transport->getFd() : -1;
        }

        void updateActivity()
        {
            lastActivity = std::chrono::steady_clock::now();
        }
    };

    bool init()
    {
        if (initialized_)
        {
            return true;
        }

        poller_ = std::make_unique<EpollPoller>();

        if (!poller_->isValid())
        {
            if (errorHandler_)
            {
                errorHandler_(std::make_error_code(std::errc::io_error));
            }
            return false;
        }

        std::error_code ec;
        poller_->add(listenTransport_.getFd(), EventType::Readable, ec);

        if (ec)
        {
            if (errorHandler_)
            {
                errorHandler_(ec);
            }
            return false;
        }

        events_.resize(config_.maxEvents);

        initialized_ = true;
        return true;
    }

    void acceptNewClient()
    {
        auto clientTransport = std::make_unique<Transport>(listenTransport_.accept());

        if (!clientTransport->isConnected())
        {
            if (errorHandler_)
            {
                errorHandler_(clientTransport->lastError());
            }
            return;
        }

        std::unique_ptr<ByteBuffer> readBuffer;
        std::unique_ptr<ByteBuffer> writeBuffer;

        /*if (bufferPool_) {
            readBuffer = bufferPool_->acquire();
            writeBuffer = bufferPool_->acquire();
        }*/

        if (!readBuffer)
            readBuffer = std::make_unique<ByteBuffer>(config_.bufferSize);
        if (!writeBuffer)
            writeBuffer = std::make_unique<ByteBuffer>(config_.bufferSize);

        int clientFd = clientTransport->getFd();
        auto ctx =
            std::make_unique<ClientContext>(std::move(clientTransport), std::move(readBuffer), std::move(writeBuffer));

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
    }

    void processClientData(ClientContext* ctx)
    {
        if (!connectionHandler_)
        {
            return;
        }

        while (ctx->readBuffer->readable() > 0)
        {
            size_t oldReadPos = ctx->readBuffer->readPos();

            ByteBuffer& request = *ctx->readBuffer;
            ByteBuffer& response = *ctx->writeBuffer;

            connectionHandler_(request, response);

            /// If the handler hasn't read anything, we exit.
            if (ctx->readBuffer->readPos() == oldReadPos)
            {
                break;
            }

            /// Senging a reply.
            if (ctx->writeBuffer->readable() > 0)
            {
                flushWriteBuffer(ctx);
            }
        }
    }

    void handleClientRead(ClientContext* ctx)
    {
        if (!ctx->readBuffer)
        {
            return;
        }

        while (true)
        {
            size_t available;
            uint8_t* buffer = ctx->readBuffer->prepareWrite(available);

            if (available == 0)
            {
                /// Buffer is full, we must process data.
                processClientData(ctx);
                buffer = ctx->readBuffer->prepareWrite(available);
                if (available == 0)
                    break;
            }

            ssize_t received = ctx->transport->recv(buffer, available);

            if (received > 0)
            {
                ctx->readBuffer->commit(received);
                ctx->updateActivity();
                processClientData(ctx);
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
            else // received == 0
            {
                removeClient(ctx->getFd());
                return;
            }
        }
    }

    void flushWriteBuffer(ClientContext* ctx)
    {
        if (ctx->isWriting)
        {
            return;
        }

        ctx->isWriting = true;

        while (ctx->writeBuffer->readable() > 0)
        {
            size_t available;
            uint8_t* buffer = ctx->writeBuffer->prepareRead(available);
            if (available == 0)
                break;

            ssize_t sent = ctx->transport->send(buffer, available);

            if (sent > 0)
            {
                ctx->writeBuffer->consume(sent);
                ctx->updateActivity();
            }
            else if (sent < 0)
            {
                if (ctx->transport->lastError() == std::errc::resource_unavailable_try_again)
                {
                    ctx->isWriting = false;
                    updateClientEvents(ctx);
                    return;
                }
                else
                {
                    removeClient(ctx->getFd());
                    return;
                }
            }
        }

        ctx->writeBuffer->reset();
        ctx->isWriting = false;
    }

    void handleClientWrite(ClientContext* ctx)
    {
        if (ctx->writeBuffer->readable() > 0)
        {
            flushWriteBuffer(ctx);
        }
        else
        {
            ctx->isWriting = false;
            updateClientEvents(ctx);
        }
    }

    void handleClientData(int fd, EventType revents)
    {
        std::unique_ptr<ClientContext> ctx;

        auto it = clients_.find(fd);
        if (it == clients_.end())
        {
            return;
        }
        ctx = std::move(it->second);
        clients_.erase(it);

        if (!ctx)
        {
            return;
        }

        handleClientEvents(ctx.get(), revents);

        if (ctx->transport && ctx->transport->isConnected())
        {
            clients_[fd] = std::move(ctx);
        }
    }

    void removeClient(int fd)
    {
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

    void cleanupIdleConnections()
    {
        auto now = std::chrono::steady_clock::now();

        for (auto it = clients_.begin(); it != clients_.end();)
        {
            if (now - it->second->lastActivity > config_.idleTimeout)
            {
                if (poller_)
                {
                    std::error_code ec;
                    poller_->remove(it->first, ec);
                }
                it = clients_.erase(it);
            }
            else
            {
                ++it;
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
        EventType events = EventType::Readable | EventType::HangUp | EventType::EdgeTriggered;

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

private:
    Config config_;
    Transport listenTransport_;
    ConnectionHandler connectionHandler_;
    ErrorHandler errorHandler_;

    std::unique_ptr<EpollPoller> poller_;
    std::vector<PollEvent> events_;
    std::atomic<bool> running_{false};
    bool initialized_{false};
    std::thread serverThread_;

    std::unordered_map<int, std::unique_ptr<ClientContext>> clients_;
};

} // namespace casket