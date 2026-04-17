#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <casket/transport/transport_base.hpp>
#include <casket/transport/unix_socket.hpp>
#include <casket/transport/tcp_socket.hpp>

#include <casket/multiplexing/epoll_poller.hpp>
#include <casket/types/byte_buffer.hpp>
#include <casket/types/pooled_queue.hpp>

namespace casket
{

struct ServerStatistics
{
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> activeConnections{0};
    std::atomic<uint64_t> totalDisconnections{0};
    std::atomic<uint64_t> totalBytesReceived{0};
    std::atomic<uint64_t> totalBytesSent{0};
    std::atomic<uint64_t> totalMessagesProcessed{0};
    std::atomic<uint64_t> totalErrors{0};
    std::atomic<uint64_t> peakConnections{0};
    std::chrono::steady_clock::time_point startTime;

    void reset()
    {
        totalConnections = 0;
        activeConnections = 0;
        totalDisconnections = 0;
        totalBytesReceived = 0;
        totalBytesSent = 0;
        totalMessagesProcessed = 0;
        totalErrors = 0;
        peakConnections = 0;
        startTime = std::chrono::steady_clock::now();
    }

    void recordConnection()
    {
        uint64_t current = ++activeConnections;
        ++totalConnections;

        uint64_t peak = peakConnections.load();
        while (current > peak && !peakConnections.compare_exchange_weak(peak, current))
            ;
    }

    void recordDisconnection()
    {
        --activeConnections;
        ++totalDisconnections;
    }

    void recordBytesReceived(size_t bytes)
    {
        totalBytesReceived += bytes;
    }

    void recordBytesSent(size_t bytes)
    {
        totalBytesSent += bytes;
    }

    void recordMessage()
    {
        ++totalMessagesProcessed;
    }

    void recordError()
    {
        ++totalErrors;
    }

    double getUptimeSeconds() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();
    }

    double getAverageMessageSize() const
    {
        uint64_t messages = totalMessagesProcessed.load();
        if (messages == 0)
            return 0.0;
        return static_cast<double>(totalBytesReceived.load() + totalBytesSent.load()) / messages;
    }

    void print(std::ostream& os = std::cout) const
    {
        double uptime = getUptimeSeconds();
        uint64_t received = totalBytesReceived.load();
        uint64_t sent = totalBytesSent.load();
        uint64_t messages = totalMessagesProcessed.load();

        os << "\n=== Server Statistics ===\n"
           << "Uptime: " << std::fixed << std::setprecision(2) << uptime << " seconds\n"
           << "Total connections: " << totalConnections.load() << "\n"
           << "Active connections: " << activeConnections.load() << "\n"
           << "Peak connections: " << peakConnections.load() << "\n"
           << "Total disconnections: " << totalDisconnections.load() << "\n"
           << "Total messages processed: " << messages << "\n"
           << "Total bytes received: " << received << " (" << std::fixed << std::setprecision(2)
           << received / (1024.0 * 1024.0) << " MB)\n"
           << "Total bytes sent: " << sent << " (" << sent / (1024.0 * 1024.0) << " MB)\n"
           << "Total errors: " << totalErrors.load() << "\n"
           << "\nPerformance metrics:\n"
           << "  Messages/sec: " << std::setprecision(0) << (uptime > 0 ? messages / uptime : 0) << " msg/s\n"
           << "  Throughput (recv): " << std::setprecision(2) << (uptime > 0 ? received / (uptime * 1024 * 1024) : 0)
           << " MB/s\n"
           << "  Throughput (send): " << std::setprecision(2) << (uptime > 0 ? sent / (uptime * 1024 * 1024) : 0)
           << " MB/s\n"
           << "  Average message size: " << std::setprecision(0) << getAverageMessageSize() << " bytes\n";
    }
};

template <typename Transport>
class GenericServer
{
public:
    using ConnectionHandler = std::function<void(ByteBuffer&, ByteBuffer&)>;
    using ErrorHandler = std::function<void(const std::error_code&)>;
    using StatisticsHandler = std::function<void(const ServerStatistics&)>;

    struct Config
    {
        size_t bufferSize{8192};
        size_t bufferPoolSize{1024};
        std::chrono::seconds idleTimeout{10};
        int maxEvents{64};
        int waitTimeoutMs{100};
        bool enableStatistics{true};
    };

    explicit GenericServer(const Config& config = Config{})
        : config_(config)
        , freeReadBuffers_(config_.bufferPoolSize)
        , freeWriteBuffers_(config_.bufferPoolSize)
    {
        if (config_.enableStatistics)
        {
            statistics_.reset();
        }
    }

    ~GenericServer()
    {
        stop();
    }

    bool listen(const std::string& address, int port = 0)
    {
        if constexpr (std::is_same_v<Transport, UnixSocket>)
        {
            return listenTransport_.listen(address);
        }
        else if constexpr (std::is_same_v<Transport, TcpSocket>)
        {
            return listenTransport_.listen(port, address);
        }
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

    void setStatisticsHandler(StatisticsHandler handler)
    {
        statisticsHandler_ = std::move(handler);
    }

    const ServerStatistics& getStatistics() const
    {
        return statistics_;
    }

    void enableStatistics(bool enable)
    {
        config_.enableStatistics = enable;
        if (enable && statistics_.startTime.time_since_epoch().count() == 0)
        {
            statistics_.reset();
        }
    }

    void printStatistics(std::ostream& os = std::cout) const
    {
        statistics_.print(os);
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
            if (config_.enableStatistics)
            {
                statistics_.recordError();
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
            if (client->readBuffer)
            {
                releaseReadBuffer(std::move(client->readBuffer));
            }
            if (client->writeBuffer)
            {
                releaseWriteBuffer(std::move(client->writeBuffer));
            }
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

    void printPeriodicStats(std::chrono::seconds interval, std::ostream& os = std::cout)
    {
        static auto lastPrint = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (now - lastPrint >= interval)
        {
            statistics_.print(os);
            lastPrint = now;
        }
    }

private:
    struct ClientContext
    {
        std::unique_ptr<Transport> transport;
        std::unique_ptr<ByteBuffer> readBuffer;
        std::unique_ptr<ByteBuffer> writeBuffer;
        std::chrono::steady_clock::time_point lastActivity;
        bool isWriting;

        uint64_t bytesReceived{0};
        uint64_t bytesSent{0};
        uint64_t messagesProcessed{0};

        ClientContext(std::unique_ptr<Transport> t, std::unique_ptr<ByteBuffer> readBuf,
                      std::unique_ptr<ByteBuffer> writeBuf)
            : transport(std::move(t))
            , readBuffer(std::move(readBuf))
            , writeBuffer(std::move(writeBuf))
            , lastActivity(std::chrono::steady_clock::now())
            , isWriting(false)
        {
        }

        ~ClientContext() noexcept
        {
        }

        bool hasDataToWrite() const
        {
            return writeBuffer && writeBuffer->availableRead() > 0;
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

        if (!clientTransport->isValid())
        {
            if (errorHandler_)
            {
                errorHandler_(clientTransport->lastError());
            }
            if (config_.enableStatistics)
            {
                statistics_.recordError();
            }
            return;
        }

        auto readBuffer = acquireReadBuffer();
        auto writeBuffer = acquireWriteBuffer();

        if (!readBuffer || !writeBuffer)
        {
            if (errorHandler_)
            {
                errorHandler_(std::make_error_code(std::errc::not_enough_memory));
            }
            if (config_.enableStatistics)
            {
                statistics_.recordError();
            }
            return;
        }

        int clientFd = clientTransport->getFd();
        auto ctx =
            std::make_unique<ClientContext>(std::move(clientTransport), std::move(readBuffer), std::move(writeBuffer));

        std::error_code ec;
        EventType events = EventType::Readable | EventType::HangUp | EventType::EdgeTriggered;
        poller_->add(static_cast<void*>(ctx.get()), clientFd, events, ec);

        if (ec)
        {
            if (errorHandler_)
            {
                errorHandler_(ec);
            }
            if (config_.enableStatistics)
            {
                statistics_.recordError();
            }
            releaseReadBuffer(std::move(ctx->readBuffer));
            releaseWriteBuffer(std::move(ctx->writeBuffer));
            return;
        }

        clients_[clientFd] = std::move(ctx);

        if (config_.enableStatistics)
        {
            statistics_.recordConnection();
        }
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

        while (ctx->readBuffer->availableRead() > 0)
        {
            size_t oldReadPos = ctx->readBuffer->readPos();
            size_t oldWritePos = ctx->writeBuffer->writePos();

            connectionHandler_(*ctx->readBuffer, *ctx->writeBuffer);

            // If the handler hasn't read anything, we exit
            if (ctx->readBuffer->readPos() == oldReadPos)
            {
                break;
            }

            if (config_.enableStatistics)
            {
                size_t bytesRead = ctx->readBuffer->readPos() - oldReadPos;
                size_t bytesWritten = ctx->writeBuffer->writePos() - oldWritePos;

                ctx->bytesReceived += bytesRead;
                ctx->bytesSent += bytesWritten;
                ctx->messagesProcessed++;

                statistics_.recordBytesReceived(bytesRead);
                statistics_.recordBytesSent(bytesWritten);
                statistics_.recordMessage();
            }

            // Sending a reply
            if (ctx->writeBuffer->availableRead() > 0)
            {
                flushWriteBuffer(ctx);
            }
        }
    }

    void handleClientRead(ClientContext* ctx)
    {
        if (!ctx->readBuffer || !ctx->transport)
            return;

        if (ctx->writeBuffer->availableRead() > 0)
        {
            flushWriteBuffer(ctx);
        }

        if (ctx->readBuffer->availableWrite() == 0)
        {
            processClientData(ctx);

            if (ctx->readBuffer->availableWrite() == 0)
            {
                removeClient(ctx->getFd());
                return;
            }
        }

        if (!ctx->transport)
        {
            /// Already closed
            return;
        }

        struct iovec iov[2];
        size_t iovcnt = ctx->readBuffer->getWriteIovec(iov, 2);

        if (iovcnt == 0)
        {
            removeClient(ctx->getFd());
            return;
        }

        struct msghdr msg{};
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;

        ssize_t received = ctx->transport->recvmsg(&msg, 0);

        if (received > 0)
        {
            ctx->readBuffer->commitWrite(received);
            ctx->updateActivity();
            processClientData(ctx);
        }
        else if (received < 0)
        {
            if (ctx->transport->lastError() != std::errc::resource_unavailable_try_again)
            {
                removeClient(ctx->getFd());
            }
        }
        else // received == 0
        {
            removeClient(ctx->getFd());
        }
    }

    void flushWriteBuffer(ClientContext* ctx)
    {
        if (ctx->isWriting)
        {
            return;
        }

        ctx->isWriting = true;

        while (ctx->writeBuffer->availableRead() > 0)
        {
            struct iovec iov[2];
            size_t iovcnt = ctx->writeBuffer->getReadIovec(iov, 2);

            if (iovcnt == 0)
                break;

            struct msghdr msg{};
            msg.msg_iov = iov;
            msg.msg_iovlen = iovcnt;

            ssize_t sent = ctx->transport->sendmsg(&msg, 0);

            if (sent > 0)
            {
                ctx->writeBuffer->commitRead(sent);
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
                    if (config_.enableStatistics)
                    {
                        statistics_.recordError();
                    }
                    removeClient(ctx->getFd());
                    return;
                }
            }
        }

        ctx->writeBuffer->clear();
        ctx->isWriting = false;
    }

    void handleClientWrite(ClientContext* ctx)
    {
        if (ctx->writeBuffer->availableRead() > 0)
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
        auto it = clients_.find(fd);
        if (it == clients_.end())
        {
            return;
        }

        ClientContext* ctx = it->second.get();
        if (!ctx)
        {
            return;
        }

        handleClientEvents(ctx, revents);
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

            if (it->second->readBuffer)
            {
                releaseReadBuffer(std::move(it->second->readBuffer));
            }
            if (it->second->writeBuffer)
            {
                releaseWriteBuffer(std::move(it->second->writeBuffer));
            }

            clients_.erase(it);

            if (config_.enableStatistics)
            {
                statistics_.recordDisconnection();
            }
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

                if (it->second->readBuffer)
                {
                    releaseReadBuffer(std::move(it->second->readBuffer));
                }
                if (it->second->writeBuffer)
                {
                    releaseWriteBuffer(std::move(it->second->writeBuffer));
                }

                it = clients_.erase(it);

                if (config_.enableStatistics)
                {
                    statistics_.recordDisconnection();
                }
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

    std::unique_ptr<ByteBuffer> acquireReadBuffer()
    {
        std::unique_ptr<ByteBuffer> buffer;
        if (freeReadBuffers_.pop(buffer))
        {
            buffer->clear();
            return buffer;
        }
        return std::make_unique<ByteBuffer>(config_.bufferSize);
    }

    void releaseReadBuffer(std::unique_ptr<ByteBuffer> buffer)
    {
        if (buffer)
        {
            buffer->clear();
            freeReadBuffers_.push(std::move(buffer));
        }
    }

    std::unique_ptr<ByteBuffer> acquireWriteBuffer()
    {
        std::unique_ptr<ByteBuffer> buffer;
        if (freeWriteBuffers_.pop(buffer))
        {
            buffer->clear();
            return buffer;
        }
        return std::make_unique<ByteBuffer>(config_.bufferSize);
    }

    void releaseWriteBuffer(std::unique_ptr<ByteBuffer> buffer)
    {
        if (buffer)
        {
            buffer->clear();
            freeWriteBuffers_.push(std::move(buffer));
        }
    }

private:
    Config config_;
    Transport listenTransport_;
    ConnectionHandler connectionHandler_;
    ErrorHandler errorHandler_;
    StatisticsHandler statisticsHandler_;

    std::unique_ptr<EpollPoller> poller_;
    std::vector<PollEvent> events_;

    StrictPooledQueue<std::unique_ptr<ByteBuffer>> freeReadBuffers_;
    StrictPooledQueue<std::unique_ptr<ByteBuffer>> freeWriteBuffers_;

    std::atomic<bool> running_{false};
    bool initialized_{false};

    std::unordered_map<int, std::unique_ptr<ClientContext>> clients_;

    ServerStatistics statistics_;
};

template <typename Transport>
void printServerStats(const GenericServer<Transport>& server, std::ostream& os = std::cout)
{
    server.printStatistics(os);
}

template <typename Transport>
void enablePeriodicStats(GenericServer<Transport>& server, std::chrono::seconds interval, std::ostream& os = std::cout)
{
    static auto lastPrint = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - lastPrint >= interval)
    {
        server.printStatistics(os);
        lastPrint = now;
    }
}

} // namespace casket