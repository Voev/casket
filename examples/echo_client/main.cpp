#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

#include <casket/client/generic_client.hpp>
#include <casket/opt/opt.hpp>
#include <casket/utils/timer.hpp>

using namespace casket;
using namespace casket::opt;

struct EchoMessage
{
    std::string_view text;

    PackResult<Packer*> pack(Packer& packer) const
    {
        return packer.pack(text);
    }

    static UnpackResult<EchoMessage> unpack(Unpacker& unpacker)
    {
        auto result = unpacker.unpackString();
        if (!result)
            return UnpackResult<EchoMessage>(UnpackerError::PrematureEnd);
        return EchoMessage{result.value()};
    }
};

struct EchoClientOptions
{
    std::string connectAddress;
    std::string customMessage;
    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds interval{0};
    size_t threadCount{1};
    size_t repeatCount{1};
    size_t fixedSize{0};
    bool oncePerSecond{false};
    bool infinite{false};
    bool async{false};
    bool printEachResponse{false};
    bool printStats{true};
};

struct ThreadStats
{
    uint64_t sent{0};
    uint64_t received{0};
    uint64_t errors{0};
    uint64_t totalLatency{0};
    std::chrono::microseconds minLatency{std::chrono::microseconds::max()};
    std::chrono::microseconds maxLatency{0};

    void recordLatency(std::chrono::microseconds latency)
    {
        if (latency < minLatency)
        {
            minLatency = latency;
        }

        if (latency > maxLatency)
        {
            maxLatency = latency;
        }

        totalLatency += latency.count();
    }

    double avgLatency() const
    {
        uint64_t count = received;
        return count > 0 ? static_cast<double>(totalLatency) / count : 0.0;
    }

    void print() const
    {
        std::cout << "  Sent: " << sent << ", Received: " << received << ", Errors: " << errors
                  << ", Latency: " << minLatency.count() << "/" << std::fixed << std::setprecision(2) << avgLatency()
                  << "/" << maxLatency.count() << " mcs" << std::endl;
    }
};

struct TotalStats
{
    uint64_t totalSent{0};
    uint64_t totalReceived{0};
    uint64_t totalErrors{0};
    uint64_t globalTotalLatency{0};
    std::chrono::microseconds globalMinLatency{std::chrono::microseconds::max()};
    std::chrono::microseconds globalMaxLatency{0};

    void accumulate(const ThreadStats& threadStats)
    {
        totalSent += threadStats.sent;
        totalReceived += threadStats.received;
        totalErrors += threadStats.errors;

        if (threadStats.minLatency < globalMinLatency)
        {
            globalMinLatency = threadStats.minLatency;
        }

        if (threadStats.maxLatency > globalMaxLatency)
        {
            globalMaxLatency = threadStats.maxLatency;
        }

        globalTotalLatency += threadStats.totalLatency;
    }

    double avgLatency() const
    {
        uint64_t count = totalReceived;
        return count > 0 ? static_cast<double>(globalTotalLatency) / count : 0.0;
    }

    void print() const
    {
        std::cout << "\n=== Total Statistics ===\n"
                  << "Total messages sent: " << totalSent << "\n"
                  << "Total messages received: " << totalReceived << "\n"
                  << "Total errors: " << totalErrors << "\n"
                  << "Global latency (min/avg/max): " << globalMinLatency.count() << "/" << std::fixed
                  << std::setprecision(2) << avgLatency() << "/" << globalMaxLatency.count() << " mcs\n";
    }
};

class EchoClient final
{
public:
    ~EchoClient() noexcept
    {
    }

    EchoClient()
    {
        // clang-format off
        parser_.add(
            OptionBuilder("help")
                .setDescription("Print help message")
                .build()
        );
        parser_.add(
            OptionBuilder("async")
                .setDescription("Asynchronous connection mode")
                .build()
        );
        parser_.add(
            OptionBuilder("connect", Value(&options_.connectAddress))
                .setDescription("Connection address")
                .setDefaultValue("/tmp/echo_server.sock")
                .build()
        );
        parser_.add(
            OptionBuilder("msg", Value(&options_.customMessage))
                .setDefaultValue("Hello, Server!")
                .setDescription("Custom message text")
                .build()
        );
        parser_.add(
            OptionBuilder("count", Value(&options_.repeatCount))
                .setDescription("Number of messages to send")
                .setDefaultValue(1)
                .build()
        );
        parser_.add(
            OptionBuilder("timeout", Value(&options_.timeout))
                .setDescription("Request timeout (ms)")
                .build()
        );
        parser_.add(
            OptionBuilder("threads", Value(&options_.threadCount))
                .setDescription("Number of parallel threads")
                .setDefaultValue(1)
                .build()
        );
        parser_.add(
            OptionBuilder("interval", Value(&options_.interval))
                .setDescription("Interval between messages (mcs)")
                .setDefaultValue(0)
                .build()
        );
        parser_.add(
            OptionBuilder("size", Value(&options_.fixedSize))
                .setDescription("Fixed message size in bytes (overrides custom message)")
                .setDefaultValue(0)
                .build()
        );
        parser_.add(
            OptionBuilder("once-per-second")
                .setDescription("Send 1 message per second")
                .build()
        );
        parser_.add(
            OptionBuilder("infinite")
                .setDescription("Send messages infinitely")
                .build()
        );
        parser_.add(
            OptionBuilder("print-response")
                .setDescription("Print each response received")
                .build()
        );
        parser_.add(
            OptionBuilder("no-stats")
                .setDescription("Disable statistics printing")
                .build()
        );
        // clang-format on
    }

    int run(int argc, char* argv[])
    {
        parser_.parse(argc, argv);
        if (parser_.isUsed("help"))
        {
            parser_.help(std::cout, argv[0]);
            return EXIT_SUCCESS;
        }
        parser_.validate();

        options_.async = parser_.isUsed("async");
        options_.oncePerSecond = parser_.isUsed("once-per-second");
        options_.infinite = parser_.isUsed("infinite");
        options_.printEachResponse = parser_.isUsed("print-response");
        options_.printStats = !parser_.isUsed("no-stats");

        std::string messageText;
        if (options_.fixedSize > 0)
        {
            messageText = generateFixedSizeMessage(options_.fixedSize);
        }
        else
        {
            messageText = options_.customMessage;
        }

        EchoMessage msg{messageText};

        Timer timer;
        TotalStats totalStats;
        std::error_code ec;
        
        timer.start();

        if (options_.threadCount == 1)
        {
            GenericClient<UnixSocket> client;
            if (!connectClient(client))
            {
                return EXIT_FAILURE;
            }

            ThreadStats stats;
            runSync(client, msg, stats);
            totalStats.accumulate(stats);
        }
        else
        {
            std::vector<std::thread> threads;
            std::vector<ThreadStats> threadStats(options_.threadCount);

            for (size_t i = 0; i < options_.threadCount; ++i)
            {
                threads.emplace_back(
                    [this, &msg, &threadStats, i]()
                    {
                        GenericClient<UnixSocket> client;
                        if (!connectClient(client))
                        {
                            return;
                        }

                        runSync(client, msg, threadStats[i]);
                    });
            }

            for (auto& thread : threads)
            {
                thread.join();
            }

            for (const auto& stats : threadStats)
            {
                totalStats.accumulate(stats);
            }
        }

        timer.stop();

        if (options_.printStats)
        {
            totalStats.print();
            
            auto totalTime = timer.elapsedMicroSecs();
            std::cout << "Total time: " << totalTime << " mcs" << std::endl;
            if (totalTime > 0)
            {
                double throughput = (totalStats.totalSent * 1000000.0) / totalTime;
                std::cout << "Total throughput: " << std::fixed << std::setprecision(2) 
                          << throughput << " msg/s" << std::endl;
                
                double avgThroughputPerThread = throughput / options_.threadCount;
                std::cout << "Avg throughput per thread: " << avgThroughputPerThread << " msg/s" << std::endl;
            }
        }

        return EXIT_SUCCESS;
    }

private:
    void runSync(GenericClient<UnixSocket>& client, EchoMessage& msg, ThreadStats& stats)
    {
        std::error_code ec;
        size_t sent = 0;
        size_t total = options_.infinite ? SIZE_MAX : options_.repeatCount;

        while (sent < total)
        {
            Timer timer;
            timer.start();

            if (!client.send(msg, ec))
            {
                stats.errors++;
                std::cerr << "Send error: " << ec.message() << std::endl;
                if (options_.oncePerSecond || options_.interval.count() > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                continue;
            }
            stats.sent++;

            EchoMessage result;
            bool received = false;

            /// Receiving with timeout

            if (options_.timeout.count() > 0)
            {
                auto deadline = std::chrono::steady_clock::now() + options_.timeout;

                while (std::chrono::steady_clock::now() < deadline)
                {
                    auto recvResult = client.receive<EchoMessage>(ec);
                    if (recvResult)
                    {
                        result = *recvResult;
                        received = true;
                        break;
                    }

                    if (ec == std::errc::resource_unavailable_try_again)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    break;
                }

                if (!received && !ec)
                {
                    SetSystemError(ec, std::errc::timed_out);
                }
            }
            else
            {
                auto recvResult = client.receive<EchoMessage>(ec);
                if (recvResult)
                {
                    result = *recvResult;
                    received = true;
                }
            }

            timer.stop();

            if (received)
            {
                auto latency = timer.elapsedMicroSecs();
                stats.recordLatency(std::chrono::microseconds(latency));
                stats.received++;

                if (options_.printEachResponse)
                {
                    std::cout << "Response #" << sent << ": " << result.text << " (latency: " << latency << " mcs)"
                              << std::endl;
                }
            }
            else
            {
                stats.errors++;
                if (ec && options_.printEachResponse)
                {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                }
            }

            sent++;

            if (sent < total)
            {
                if (options_.oncePerSecond)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                else if (options_.interval.count() > 0)
                {
                    std::this_thread::sleep_for(options_.interval);
                }
            }
        }
    }

    bool connectClient(GenericClient<UnixSocket>& client)
    {
        std::error_code ec;
        
        std::cout << "Connecting to " << options_.connectAddress << " (thread " 
                  << std::this_thread::get_id() << ")..." << std::endl;
        
        if (!client.connect(options_.connectAddress, -1, options_.async, ec))
        {
            std::cerr << "Failed to connect: " << ec.message() << std::endl;
            return false;
        }

        if (!client.isConnected(options_.timeout, ec))
        {
            std::cerr << "Connection timeout: " << ec.message() << std::endl;
            return false;
        }

        return true;
    }

    std::string generateFixedSizeMessage(size_t size)
    {
        std::string result;
        result.reserve(size);

        const char* pattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        size_t patternLen = std::strlen(pattern);

        for (size_t i = 0; i < size; ++i)
        {
            result += pattern[i % patternLen];
        }

        return result;
    }

private:
    opt::CmdLineOptionsParser parser_;
    EchoClientOptions options_;
};

int main(int argc, char* argv[])
{
    int ret{EXIT_SUCCESS};
    try
    {
        EchoClient client;
        ret = client.run(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        ret = EXIT_FAILURE;
    }
    return ret;
}