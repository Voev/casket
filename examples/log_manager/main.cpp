#include <vector>
#include <cstdlib>
#include <string>
#include <casket/log/async_logger.hpp>
#include <casket/log/log_worker.hpp>
#include <casket/log/console.hpp>

using namespace casket;

int main(int argc, char* argv[])
{
    int msgCount = 10000;

    if(argc > 1)
    {
        msgCount = std::stoi(argv[1]);
    }

    LogWorker logWorker{new ConsoleSink(true)};
    
    std::vector<std::thread> producers;
    for (int i = 0; i < 10; ++i)
    {
        producers.emplace_back(
            [i, &msgCount]()
            {
                for (int j = 0; j < msgCount; ++j)
                {
                    LOG_INFO("Producer %d, Message %d", i, j);
                }
            });
    }

    for (auto& t : producers)
    {
        t.join();
    }

    logWorker.stop();
    return EXIT_SUCCESS;
}