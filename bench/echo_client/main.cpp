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
#include <casket/transport/byte_buffer.hpp>

using namespace casket;

struct BenchmarkConfig
{
    std::string socketPath = "/tmp/echo_server";
    size_t durationSeconds = 10;
    size_t messageSize = 64;
    size_t numClients = 1;
    bool reconnectPerMessage = false;
};

class SimpleClient
{
public:
    SimpleClient(const std::string& socketPath, size_t messageSize)
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
        std::error_code ec{};
        return socket_.connect(socketPath_, true, ec);
    }

    void close()
    {
        socket_.close();
    }

    bool isConnected() const
    {
        return socket_.isValid();
    }

    bool sendMessage()
    {
        if (!socket_.isValid())
        {
            if (!connect())
                return false;
        }

        std::error_code ec{};
        ssize_t sent = socket_.send(message_.data(), message_.size(), ec);
        if (sent != static_cast<ssize_t>(message_.size()))
        {
            socket_.close();
            return false;
        }

        uint8_t buffer[4096];
        ByteBuffer response(4096);
        size_t expectedSize = message_.size();
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(100);

        while (response.availableWrite() < expectedSize)
        {
            auto now = std::chrono::steady_clock::now();
            if (now - startTime > timeout)
            {
                socket_.close();
                return false;
            }

            ssize_t received = socket_.recv(buffer, sizeof(buffer), ec);
            if (received > 0)
            {
                //response.write(buffer, received);
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

void printProgress(int percent, size_t current, double rate, const char* unit)
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
    std::cout << "(" << current << " " << unit << ", ";
    std::cout << std::fixed << std::setprecision(0) << rate << " " << unit << "/s)";
    std::cout << std::flush;
}

int main(int argc, char* argv[])
{
    std::cout << std::unitbuf;

    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc)
        {
            config.socketPath = argv[++i];
        }
        else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc)
        {
            config.durationSeconds = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            config.messageSize = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--clients") == 0 && i + 1 < argc)
        {
            config.numClients = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--reconnect") == 0)
        {
            config.reconnectPerMessage = true;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --socket PATH     Unix socket path (default: /tmp/echo_server)\n"
                      << "  --duration SEC    Test duration in seconds (default: 10)\n"
                      << "  --size BYTES      Message size in bytes (default: 64)\n"
                      << "  --clients N       Number of concurrent clients (default: 1)\n"
                      << "  --reconnect       Reconnect for each message (measure connection rate)\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    std::cout << "=== Benchmark ===\n"
              << "Socket: " << config.socketPath << "\n"
              << "Duration: " << config.durationSeconds << "s\n"
              << "Message size: " << config.messageSize << " bytes\n"
              << "Clients: " << config.numClients << "\n"
              << "Mode: " << (config.reconnectPerMessage ? "reconnect (CPS)" : "persistent (msg/s)") << "\n"
              << std::endl;

    std::vector<std::unique_ptr<SimpleClient>> clients;
    std::vector<std::thread> threads;
    std::atomic<size_t> totalSuccess{0};

    for (size_t i = 0; i < config.numClients; ++i)
    {
        clients.push_back(std::make_unique<SimpleClient>(config.socketPath, config.messageSize));
    }

    std::cout << "Connecting clients... " << std::endl;
    if (!config.reconnectPerMessage)
    {
        for (auto& client : clients)
        {
            if (!client->connect())
            {
                std::cerr << "Failed to connect client" << std::endl;
                return 1;
            }
        }
    }

    std::cout << "Running benchmark..." << std::endl;

    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::seconds(config.durationSeconds);

    for (size_t i = 0; i < config.numClients; ++i)
    {
        threads.emplace_back(
            [&, client = clients[i].get()]()
            {
                size_t counter = 0;

                while (std::chrono::steady_clock::now() < endTime)
                {
                    if (config.reconnectPerMessage)
                    {
                        SimpleClient tempClient(config.socketPath, config.messageSize);
                        if (tempClient.connect())
                        {
                            if (tempClient.sendMessage())
                            {
                                counter++;
                            }
                            tempClient.close();
                        }
                    }
                    else
                    {
                        if (client->sendMessage())
                        {
                            counter++;
                        }
                        else
                        {
                            client->close();
                            client->connect();
                        }
                    }
                }

                totalSuccess += counter;
            });
    }

    std::atomic<bool> progressRunning(true);
    std::thread progressThread(
        [&]()
        {
            auto lastUpdate = startTime;
            size_t lastSuccess = 0;

            while (progressRunning)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();

                if (elapsed < 0)
                    continue;

                int percent = std::min(100, static_cast<int>(elapsed * 100 / config.durationSeconds));
                size_t current = totalSuccess.load();

                double rate = 0;
                auto timeSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
                if (timeSinceLast > 500 && lastUpdate != startTime)
                {
                    rate = (current - lastSuccess) * 1000.0 / timeSinceLast;
                    lastSuccess = current;
                    lastUpdate = now;
                }

                const char* unit = config.reconnectPerMessage ? "conns" : "msgs";
                printProgress(percent, current, rate, unit);
            }
            std::cout << std::endl;
        });

    for (auto& t : threads)
    {
        t.join();
    }

    progressRunning = false;
    progressThread.join();

    auto endTimeReal = std::chrono::steady_clock::now();
    double actualDuration = std::chrono::duration_cast<std::chrono::duration<double>>(endTimeReal - startTime).count();

    size_t finalSuccess = totalSuccess.load();

    if (finalSuccess == 0)
    {
        std::cerr << "\nError: No successful operations!" << std::endl;
        return 1;
    }

    double opsPerSecond = finalSuccess / actualDuration;
    const char* unit = config.reconnectPerMessage ? "conns/s" : "msg/s";

    std::cout << "\n\n=== Results ===\n"
              << "Successful: " << finalSuccess << "\n"
              << "Duration: " << std::fixed << std::setprecision(3) << actualDuration << "s\n"
              << "Rate: " << std::setprecision(0) << opsPerSecond << " " << unit << "\n";

    if (!config.reconnectPerMessage)
    {
        double mbps = (finalSuccess * config.messageSize) / (actualDuration * 1024 * 1024);
        double latencyUs = (actualDuration * 1000000) / finalSuccess;
        std::cout << "Throughput: " << std::setprecision(2) << mbps << " MB/s\n"
                  << "Avg latency: " << std::setprecision(2) << latencyUs << " μs\n";
    }

    return 0;
}