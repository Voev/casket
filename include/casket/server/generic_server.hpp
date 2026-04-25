#pragma once

#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>

#include <casket/transport/transport_base.hpp>
#include <casket/transport/unix_socket.hpp>
#include <casket/transport/tcp_socket.hpp>
#include <casket/transport/byte_buffer.hpp>

#include <casket/pack/pack.hpp>

#include <casket/multiplexing/epoll_poller.hpp>
#include <casket/types/object_pool.hpp>
#include <casket/types/hash_table.hpp>

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
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
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
        cacheHits = 0;
        cacheMisses = 0;
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

    void recordCacheHit()
    {
        ++cacheHits;
    }

    void recordCacheMiss()
    {
        ++cacheMisses;
    }

    double getCacheHitRate() const
    {
        uint64_t hits = cacheHits.load();
        uint64_t misses = cacheMisses.load();
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
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
           << "Cache hit rate: " << std::fixed << std::setprecision(2) << (getCacheHitRate() * 100) << "%\n"
           << "Cache hits: " << cacheHits.load() << "\n"
           << "Cache misses: " << cacheMisses.load() << "\n"
           << "\nPerformance metrics:\n"
           << "  Messages/sec: " << std::setprecision(0) << (uptime > 0 ? messages / uptime : 0) << " msg/s\n"
           << "  Throughput (recv): " << std::setprecision(2) << (uptime > 0 ? received / (uptime * 1024 * 1024) : 0)
           << " MB/s\n"
           << "  Throughput (send): " << std::setprecision(2) << (uptime > 0 ? sent / (uptime * 1024 * 1024) : 0)
           << " MB/s\n"
           << "  Average message size: " << std::setprecision(0) << getAverageMessageSize() << " bytes\n";
    }
};

struct GenericServerConfig
{
    size_t byteBufferSize{8192};
    size_t contextPoolSize{1024};
    std::chrono::seconds idleTimeout{10};
    std::chrono::seconds cacheTTL{60};
    int maxEvents{64};
    int waitTimeoutMs{100};
    bool enableStatistics{true};
};

template <typename Transport>
struct Context
{
    Transport transport;
    ByteBuffer readBuffer;
    ByteBuffer writeBuffer;
    std::chrono::steady_clock::time_point lastActivity;
    bool active{false};

    uint64_t bytesReceived{0};
    uint64_t bytesSent{0};
    uint64_t messagesProcessed{0};

    Context<Transport>* activePrev{nullptr};
    Context<Transport>* activeNext{nullptr};

    Context(Context&& other) noexcept = default;
    Context& operator=(Context&& other) noexcept = default;

    explicit Context(size_t bufferSize)
        : transport()
        , readBuffer(bufferSize)
        , writeBuffer(bufferSize)
        , lastActivity(std::chrono::steady_clock::now())
        , active(false)
    {
    }

    bool hasDataToWrite() const
    {
        return writeBuffer.availableRead() > 0;
    }

    int getFd() const
    {
        return transport.getFd();
    }

    void updateActivity()
    {
        lastActivity = std::chrono::steady_clock::now();
    }

    bool isActive() const
    {
        return active;
    }

    template <typename T>
    bool packThenSend(const T& message, std::error_code& ec)
    {
        Packer packer(writeBuffer.getWritePtr(), writeBuffer.availableWrite());

        if (!message.pack(packer))
        {
            return false;
        }

        size_t packedSize = packer.position();
        writeBuffer.commitWrite(packedSize);

        ssize_t n = transport.sendBuffer(writeBuffer, ec);
        if (n > 0)
        {
            bytesSent += n;
            return true;
        }
        return false;
    }

    template <typename T>
    UnpackResult<T> readThenUnpack(std::error_code& ec)
    {
        if (readBuffer.availableRead() == 0)
        {
            ssize_t n = transport.recvBuffer(readBuffer, ec);
            if (n <= 0)
            {
                return UnpackResult<T>(UnpackerError::PrematureEnd);
            }
            else
            {
                bytesReceived += n;
            }
        }

        Unpacker unpacker(readBuffer.getReadPtr(), readBuffer.availableRead());
        auto result = T::unpack(unpacker);
        
        if (result)
        {
            readBuffer.commitRead(unpacker.position());
        }
        
        return result;
    }

    void reset()
    {
        transport = Transport();
        readBuffer.clear();
        writeBuffer.clear();
        bytesReceived = 0;
        bytesSent = 0;
        messagesProcessed = 0;
        lastActivity = std::chrono::steady_clock::now();
        active = false;
    }
};

template <typename Transport>
class GenericServer
{
public:
    using ClientContext = Context<Transport>;
    using ConnectionHandler = std::function<void(ClientContext&)>;
    using ErrorHandler = std::function<void(const std::error_code&)>;
    using StatisticsHandler = std::function<void(const ServerStatistics&)>;

    explicit GenericServer(const GenericServerConfig& config = GenericServerConfig{})
        : config_(config)
        , contextPool_(config_.contextPoolSize, config_.byteBufferSize)
        , fdToContext_(config_.contextPoolSize)
        , activeHead_(nullptr)
        , activeTail_(nullptr)
    {
        if (config_.enableStatistics)
        {
            statistics_.reset();
        }
    }

    ~GenericServer() noexcept
    {
        stop();
    }

    bool listen(const std::string& address, int port, int backlog, std::error_code& ec)
    {
        if constexpr (std::is_same_v<Transport, UnixSocket>)
        {
            return listenTransport_.listen(address, backlog, ec);
        }
        else if constexpr (std::is_same_v<Transport, TcpSocket>)
        {
            return listenTransport_.listen(address, port, backlog, ec);
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

        auto* ctx = activeHead_;
        while (ctx)
        {
            ctx->transport = Transport();
            ctx->active = false;
            ctx = ctx->activeNext;
        }

        fdToContext_.clear();
        contextPool_.clear();

        activeHead_ = nullptr;
        activeTail_ = nullptr;

        listenTransport_.close();
        initialized_ = false;
    }

    bool isRunning() const
    {
        return running_;
    }

    size_t getClientCount() const
    {
        size_t count = 0;
        auto* ctx = activeHead_;
        while (ctx)
        {
            ++count;
            ctx = ctx->activeNext;
        }
        return count;
    }

private:
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

    void addToActive(ClientContext* ctx)
    {
        ctx->activePrev = nullptr;
        ctx->activeNext = activeHead_;

        if (activeHead_)
            activeHead_->activePrev = ctx;
        activeHead_ = ctx;

        if (!activeTail_)
            activeTail_ = ctx;
    }

    void removeFromActive(ClientContext* ctx)
    {
        if (ctx->activePrev)
            ctx->activePrev->activeNext = ctx->activeNext;
        else
            activeHead_ = ctx->activeNext;

        if (ctx->activeNext)
            ctx->activeNext->activePrev = ctx->activePrev;
        else
            activeTail_ = ctx->activePrev;

        ctx->activePrev = ctx->activeNext = nullptr;
    }

    ClientContext* acquireContext()
    {
        return contextPool_.acquire();
    }

    void releaseContext(ClientContext* ctx)
    {
        if (ctx)
        {
            ctx->reset();
            contextPool_.release(ctx);
        }
    }

    void acceptNewClient()
    {
        std::error_code ec;
        Transport clientTransport(listenTransport_.accept(ec));

        if (!clientTransport.isValid())
        {
            if (errorHandler_)
                errorHandler_(ec);
            if (config_.enableStatistics)
                statistics_.recordError();
            return;
        }

        int clientFd = clientTransport.getFd();

        auto* ctx = acquireContext();
        if (!ctx)
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

        ctx->transport = std::move(clientTransport);
        ctx->active = true;
        ctx->readBuffer.clear();
        ctx->writeBuffer.clear();
        ctx->bytesReceived = 0;
        ctx->bytesSent = 0;
        ctx->messagesProcessed = 0;
        ctx->updateActivity();

        fdToContext_.insert(clientFd, ctx);
        addToActive(ctx);

        EventType events = EventType::Readable | EventType::HangUp | EventType::EdgeTriggered;
        poller_->add(static_cast<void*>(ctx), clientFd, events, ec);

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

            fdToContext_.remove(clientFd);
            removeFromActive(ctx);
            releaseContext(ctx);
            return;
        }

        if (config_.enableStatistics)
        {
            statistics_.recordConnection();
        }
    }

    void handleClientEvents(ClientContext* ctx, EventType revents)
    {
        if (!ctx || !ctx->transport.isValid() || !ctx->active)
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

    void handleClientRead(ClientContext* ctx)
    {
        if (connectionHandler_)
        {
            connectionHandler_(*ctx);
        }

        if (ctx->writeBuffer.availableRead() > 0)
        {
            // Пытаемся отправить сразу
            std::error_code ec;
            ssize_t sent = ctx->transport.sendBuffer(ctx->writeBuffer, ec);
            
            if (sent > 0)
            {
                // Частично отправилось - обновляем события
                if (ctx->writeBuffer.availableRead() > 0)
                {
                    updateEvents(ctx, true);
                }
            }
            else if (ec == std::errc::resource_unavailable_try_again)
            {
                // Сокет не готов - добавляем Writable флаг
                updateEvents(ctx, true);
            }
            else if (ec)
            {
                removeClient(ctx->transport.getFd());
                return;
            }
        }

        ctx->updateActivity();
    }

    void handleClientWrite(ClientContext* ctx)
    {
        std::error_code ec;

        if (ctx->writeBuffer.availableRead() > 0)
        {
            ctx->transport.sendBuffer(ctx->writeBuffer, ec);

            if (ec && ec != std::errc::resource_unavailable_try_again)
            {
                removeClient(ctx->transport.getFd());
                return;
            }
        }

        if (ctx->writeBuffer.availableRead() == 0)
        {
            updateEvents(ctx, false);
        }

        ctx->updateActivity();
    }

    void removeClient(int fd)
    {
        auto* ctx = fdToContext_.get(fd);
        if (!ctx || !ctx->active)
        {
            return;
        }

        if (poller_)
        {
            std::error_code ec;
            poller_->remove(fd, ec);
        }

        removeFromActive(ctx);
        ctx->active = false;

        fdToContext_.remove(fd);
        releaseContext(ctx);

        if (config_.enableStatistics)
        {
            statistics_.recordDisconnection();
        }
    }

    void cleanupIdleConnections()
    {
        auto now = std::chrono::steady_clock::now();
        auto* ctx = activeHead_;

        while (ctx)
        {
            auto* next = ctx->activeNext;
            if (now - ctx->lastActivity > config_.idleTimeout)
            {
                removeClient(ctx->getFd());
            }
            ctx = next;
        }
    }

    void updateEvents(ClientContext* ctx, bool writable)
    {
        assert(ctx != nullptr);
        assert(ctx->transport.isValid());
        assert(ctx->active);

        std::error_code ec;
        EventType events = EventType::Readable | EventType::HangUp | EventType::EdgeTriggered;

        if (writable)
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
    GenericServerConfig config_;
    Transport listenTransport_;
    ConnectionHandler connectionHandler_;
    ErrorHandler errorHandler_;
    StatisticsHandler statisticsHandler_;

    std::unique_ptr<EpollPoller> poller_;
    std::vector<PollEvent> events_;

    ObjectPool<ClientContext, StrictHeapPolicy> contextPool_;
    HashTable<SocketType, ClientContext> fdToContext_;

    ClientContext* activeHead_;
    ClientContext* activeTail_;

    std::atomic_bool running_{false};
    bool initialized_{false};

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