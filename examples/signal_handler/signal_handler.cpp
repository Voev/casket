#include <casket/signal/signal_handler.hpp>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace casket;

std::atomic<bool> interrupted;

void signalCallback(int signum)
{
    switch (signum)
    {
    case SIGINT:
        std::cout << "Received SIGINT (Ctrl+C). Graceful shutdown initiated." << std::endl;
        interrupted = true;
        break;
    case SIGTERM:
        std::cout << "Received SIGTERM. Graceful shutdown initiated." << std::endl;
        interrupted = true;
        break;
    case SIGHUP:
        std::cout << "Received SIGHUP. Configuration reload." << std::endl;
        break;
    case SIGUSR1:
        std::cout << "Received SIGUSR1. Custom action." << std::endl;
        break;
    default:
        std::cout << "Received signal: " << signum << std::endl;
    }
}

int main()
{
    try
    {
        SignalHandler handler;
        int timeout = 60;
        interrupted = false;

        std::vector<int> signals = {SIGINT, SIGTERM, SIGHUP, SIGUSR1};
        handler.registerSignals(signals, signalCallback);

        std::cout << "Registered signals: SIGINT, SIGTERM, SIGHUP, SIGUSR1" << std::endl;
        std::cout << "Send test signal: kill -SIGUSR1 " << getpid() << std::endl;
        std::cout << "Running for " << timeout << " seconds..." << std::endl;

        for (int i = 0; !interrupted && i < timeout; ++i)
        {
            handler.processSignals();
            std::cout << "Processing... " << (timeout - i) << " seconds left" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Example was finished" << std::endl;
    }
    catch (const std::system_error& e)
    {
        std::cerr << "System error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}