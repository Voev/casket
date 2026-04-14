#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iomanip>
#include <cstring>
#include <memory>
#include <algorithm>

#include <casket/transport/unix_socket.hpp>
#include <casket/types/byte_buffer.hpp>

using namespace casket;

class EchoClient
{
public:
    EchoClient(const std::string& socketPath, size_t messageSize = 64)
        : socketPath_(socketPath)
        , messageSize_(messageSize)
    {
        message_.resize(messageSize);
        for (size_t i = 0; i < messageSize; ++i)
        {
            message_[i] = static_cast<uint8_t>('A' + (i % 26));
        }
    }

    bool connect()
    {
        return socket_.connect(socketPath_);
    }

    bool isConnected() const
    {
        return socket_.isValid();
    }

    void close()
    {
        socket_.close();
    }

    bool sendMessage()
    {
        if (!socket_.isValid())
        {
            if (!connect())
            {
                return false;
            }
        }

        ssize_t sent = socket_.send(message_.data(), message_.size());
        if (sent != static_cast<ssize_t>(message_.size()))
        {
            socket_.close();
            return false;
        }

        uint8_t buffer[4096];
        ByteBuffer response;

        size_t expectedSize = message_.size();
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(100);

        while (response.size() < expectedSize)
        {
            auto now = std::chrono::steady_clock::now();
            if (now - startTime > timeout)
            {
                socket_.close();
                return false;
            }

            ssize_t received = socket_.recv(buffer, sizeof(buffer));
            if (received > 0)
            {
                response.write(buffer, received);
            }
            else if (received == 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            else
            {
                socket_.close();
                return false;
            }
        }

        return true;
    }

private:
    UnixSocket socket_;
    std::string socketPath_;
    size_t messageSize_;
    std::vector<uint8_t> message_;
};

void printProgress(int percent, size_t currentMessages, double currentRate)
{
    std::cout << "\r[";
    int barWidth = 50;
    int pos = barWidth * percent / 100;

    for (int i = 0; i < barWidth; ++i)
    {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }

    std::cout << "] " << percent << "% ";
    std::cout << "(" << currentMessages << " msgs, ";
    std::cout << std::fixed << std::setprecision(0) << currentRate << " msg/s)";
    std::cout << std::flush;
}

int main(int argc, char* argv[])
{
    std::cout.setf(std::ios::unitbuf);

    std::string socketPath = "/tmp/echo_server";
    size_t durationSeconds = 10;
    size_t messageSize = 64;
    size_t numClients = 1;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc)
        {
            socketPath = argv[++i];
        }
        else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc)
        {
            durationSeconds = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            messageSize = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--clients") == 0 && i + 1 < argc)
        {
            numClients = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --socket PATH     Unix socket path (default: /tmp/echo_server)\n"
                      << "  --duration SEC    Test duration in seconds (default: 10)\n"
                      << "  --size BYTES      Message size in bytes (default: 64)\n"
                      << "  --clients N       Number of concurrent clients (default: 1)\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    std::cout << "=== Echo Server Benchmark ===\n"
              << "Socket: " << socketPath << "\n"
              << "Duration: " << durationSeconds << " seconds\n"
              << "Message size: " << messageSize << " bytes\n"
              << "Concurrent clients: " << numClients << "\n"
              << std::endl;

    std::vector<std::unique_ptr<EchoClient>> clients;
    std::vector<std::thread> threads;
    std::atomic<size_t> totalMessages(0);

    for (size_t i = 0; i < numClients; ++i)
    {
        clients.push_back(std::make_unique<EchoClient>(socketPath, messageSize));
    }

    std::cout << "Connecting clients... " << std::endl;
    for (auto& client : clients)
    {
        if (!client->connect())
        {
            std::cerr << "Failed to connect client" << std::endl;
            return 1;
        }
    }

    std::cout << "Running benchmark..." << std::endl;

    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::seconds(durationSeconds);

    for (size_t i = 0; i < numClients; ++i)
    {
        threads.emplace_back(
            [client = clients[i].get(), &totalMessages, endTime]()
            {
                size_t counter = 0;

                while (std::chrono::steady_clock::now() < endTime)
                {
                    if (client->sendMessage())
                    {
                        counter++;

                        if (counter % 100 == 0)
                        {
                            totalMessages += 100;
                        }

                        client->close();
                        client->connect();
                    }
                    else
                    {
                        totalMessages = 0;
                        counter = 0;

                        client->close();
                        if (!client->connect())
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }
                }

                totalMessages += (counter % 100);
            });
    }

    std::atomic<bool> progressRunning(true);
    std::thread progressThread(
        [&totalMessages, &progressRunning, &startTime, durationSeconds]()
        {
            auto lastUpdate = startTime;
            size_t lastMessages = 0;
            int lastPercent = -1;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            while (progressRunning)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();

                if (elapsed < 0)
                    continue;

                int percent = static_cast<int>(elapsed * 100 / durationSeconds);
                percent = std::clamp(percent, 0, 100);

                size_t currentMessages = totalMessages.load(std::memory_order_relaxed);

                double currentRate = 0.0;
                auto timeSinceLast =
                    std::chrono::duration_cast<std::chrono::duration<double>>(now - lastUpdate).count();

                if (timeSinceLast > 0.5 && lastUpdate != startTime)
                {
                    if (currentMessages >= lastMessages)
                    {
                        currentRate = (currentMessages - lastMessages) / timeSinceLast;
                        currentRate = std::min(currentRate, 10000000.0);
                    }
                }

                if (percent != lastPercent || percent == 100)
                {
                    std::cout << "\r\033[K";
                    std::cout << "[";
                    int barWidth = 50;
                    int pos = barWidth * percent / 100;

                    for (int i = 0; i < barWidth; ++i)
                    {
                        if (i < pos)
                            std::cout << "=";
                        else if (i == pos && percent < 100)
                            std::cout << ">";
                        else
                            std::cout << " ";
                    }

                    std::cout << "] " << percent << "% ";
                    std::cout << "(" << currentMessages << " msgs, ";
                    std::cout << std::fixed << std::setprecision(0) << currentRate << " msg/s)";
                    std::cout << std::flush;

                    lastPercent = percent;
                }

                if (timeSinceLast > 0.5)
                {
                    lastMessages = currentMessages;
                    lastUpdate = now;
                }
            }

            std::cout << "\r\033[K";
            std::cout << "[";
            for (int i = 0; i < 50; ++i)
                std::cout << "=";
            std::cout << "] 100% (" << totalMessages.load() << " msgs, done)";
            std::cout << std::endl;
        });

    for (auto& thread : threads)
    {
        thread.join();
    }

    progressRunning = false;
    if (progressThread.joinable())
    {
        progressThread.join();
    }

    auto endRealTime = std::chrono::steady_clock::now();
    auto actualDuration = std::chrono::duration_cast<std::chrono::duration<double>>(endRealTime - startTime).count();

    size_t finalMessages = totalMessages.load();

    if (finalMessages == 0)
    {
        std::cerr << "\nError: No messages were processed successfully!" << std::endl;
        std::cerr << "Check if server is running and responding correctly." << std::endl;
        return 1;
    }

    double messagesPerSecond = finalMessages / actualDuration;
    double megabytesPerSecond = (finalMessages * messageSize) / (actualDuration * 1024 * 1024);
    double microsecondsPerMessage = (actualDuration * 1000000) / finalMessages;

    std::cout << "\n\n=== Results ===\n"
              << "Total messages: " << finalMessages << "\n"
              << "Total data: " << std::fixed << std::setprecision(2)
              << (finalMessages * messageSize) / (1024.0 * 1024.0) << " MB\n"
              << "Actual duration: " << std::setprecision(3) << actualDuration << " seconds\n"
              << "Messages/second: " << std::setprecision(0) << messagesPerSecond << " msg/s\n"
              << "Throughput: " << std::setprecision(2) << megabytesPerSecond << " MB/s\n"
              << "Latency (avg): " << std::setprecision(2) << microsecondsPerMessage << " μs/msg\n";

    if (numClients > 1)
    {
        double messagesPerClient = finalMessages / static_cast<double>(numClients);
        std::cout << "\nPer client average:\n"
                  << "  Messages/client: " << std::setprecision(0) << messagesPerClient << "\n"
                  << "  Msg/s per client: " << std::setprecision(0) << messagesPerClient / actualDuration << " msg/s\n";
    }

    return 0;
}