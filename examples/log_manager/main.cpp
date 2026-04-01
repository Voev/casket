#include <vector>
#include <cstdlib>
#include <string>
#include <casket/log/async_logger.hpp>
#include <casket/log/log_worker.hpp>
#include <casket/log/console.hpp>
#include <casket/log/log_manager.hpp>

using namespace casket;

int main(int argc, char* argv[])
{
    AsyncLogger::Config config;
    config.queue_size = 32768;
    config.pool_size = 65536;

    int msgCount = 10000;

    if(argc > 1)
    {
        msgCount = std::stoi(argv[1]);
    }

    LogManager::Instance().initialize(config, {new ConsoleSink(true)});
    
    std::vector<std::thread> producers;
    for (int i = 0; i < 10; ++i)
    {
        producers.emplace_back(
            [i, &msgCount]()
            {
                for (int j = 0; j < msgCount; ++j)
                {
                    LOG_INFO("[Producer %d] Message %d", i, j);
                }
            });
    }

    for (auto& t : producers)
    {
        t.join();
    }

    LogManager::Instance().shutdown();
    //auto stats = LogManager::Instance().getStats();
    //printf("Produced: %lu, Dropped: %lu\n", stats.total_messages, stats.total_dropped);

    return EXIT_SUCCESS;
}