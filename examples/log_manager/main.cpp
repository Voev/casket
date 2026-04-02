#include <vector>
#include <cstdlib>
#include <string>
#include <casket/log/log.hpp>

using namespace casket;

int main(int argc, char* argv[])
{
    int msgCount = 10000;

    if (argc > 1)
    {
        msgCount = std::stoi(argv[1]);
    }

    LogWorker logWorker(std::make_unique<ConsoleSink>());

    AsyncLogger::getInstance().setLevel(LogLevel::DEBUG);

    std::vector<std::thread> producers;
    for (int i = 0; i < 10; ++i)
    {
        producers.emplace_back(
            [i, &msgCount]()
            {
                for (int j = 0; j < msgCount; ++j)
                {
                    CSK_LOG_DEBUG("Producer %d, Message %d", i, j);
                }
            });
    }

    for (auto& t : producers)
    {
        t.join();
    }

    logWorker.stop();

    AsyncLogger::getInstance().printStats();
    return EXIT_SUCCESS;
}