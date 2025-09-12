#include <casket/signal/signal_handler.hpp>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <vector>

using namespace casket;

void signalCallback(int signum)
{
    switch (signum)
    {
    case SIGINT:
        std::cout << "Received SIGINT (Ctrl+C). Graceful shutdown initiated." << std::endl;
        break;
    case SIGTERM:
        std::cout << "Received SIGTERM. Graceful shutdown initiated." << std::endl;
        break;
    case SIGHUP:
        std::cout << "Received SIGHUP. Configuration reload." << std::endl;
        // Здесь можно перезагрузить конфигурацию
        break;
    case SIGUSR1:
        std::cout << "Received SIGUSR1. Custom action." << std::endl;
        break;
    default:
        std::cout << "Received signal: " << signum << std::endl;
    }
}

// Пример с использованием error_code
void exampleWithErrorCode()
{
    std::cout << "=== Example with error_code ===" << std::endl;

    SignalHandler handler;
    std::error_code ec;

    // Регистрация нескольких сигналов
    handler.registerSignal(SIGINT, signalCallback, ec);
    if (ec)
    {
        std::cerr << "Failed to register SIGINT: " << ec.message() << std::endl;
        return;
    }

    handler.registerSignal(SIGTERM, signalCallback, ec);
    if (ec)
    {
        std::cerr << "Failed to register SIGTERM: " << ec.message() << std::endl;
        return;
    }

    std::cout << "Signal handler descriptor: " << handler.getDescriptor() << std::endl;
    std::cout << "Send signals using: kill -SIGINT " << getpid() << std::endl;
    std::cout << "Running for 10 seconds..." << std::endl;

    // Основной цикл обработки
    for (int i = 0; i < 10; ++i)
    {
        handler.processSignals(ec);
        if (ec)
        {
            std::cerr << "Error processing signals: " << ec.message() << std::endl;
            ec.clear(); // Очищаем для следующей итерации
        }

        std::cout << "Working... " << (9 - i) << " seconds left" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Пример с использованием исключений
void exampleWithExceptions()
{
    std::cout << "\n=== Example with exceptions ===" << std::endl;

    try
    {
        SignalHandler handler;

        // Регистрация сигналов с помощью span
        std::vector<int> signals = {SIGHUP, SIGUSR1, SIGUSR2};
        handler.registerSignals(signals, signalCallback);

        std::cout << "Registered signals: SIGHUP, SIGUSR1, SIGUSR2" << std::endl;
        std::cout << "Send test signal: kill -SIGUSR1 " << getpid() << std::endl;
        std::cout << "Running for 5 seconds..." << std::endl;

        for (int i = 0; i < 5; ++i)
        {
            handler.processSignals(); // Может бросить исключение
            std::cout << "Processing... " << (4 - i) << " seconds left" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Демонстрация удаления сигнала
        handler.unregisterSignal(SIGUSR2);
        std::cout << "SIGUSR2 unregistered" << std::endl;
    }
    catch (const std::system_error& e)
    {
        std::cerr << "System error: " << e.what() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

// Пример интеграции с event loop (например, epoll)
void exampleWithEventLoop()
{
    std::cout << "\n=== Example with event loop ===" << std::endl;

    SignalHandler handler;
    std::error_code ec;

    // Регистрируем все нужные сигналы
    handler.registerSignal(SIGINT, signalCallback, ec);
    handler.registerSignal(SIGTERM, signalCallback, ec);

    if (ec)
    {
        std::cerr << "Failed to setup signal handler: " << ec.message() << std::endl;
        return;
    }

    int signalFd = handler.getDescriptor();
    std::cout << "Signal FD for event loop: " << signalFd << std::endl;

    // В реальном приложении здесь был бы вызов epoll_ctl или подобного
    // для добавления signalFd в event loop

    std::cout << "Event loop integration ready. Send signals to test." << std::endl;
    std::cout << "Running for 8 seconds..." << std::endl;

    for (int i = 0; i < 8; ++i)
    {
        // В event loop здесь проверяли бы готовность signalFd к чтению
        // if (event_ready(signalFd)) {
        handler.processSignals(ec);
        if (ec)
        {
            std::cerr << "Signal processing error: " << ec.message() << std::endl;
        }
        // }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    std::cout << "SignalHandler Usage Examples" << std::endl;
    std::cout << "Process PID: " << getpid() << std::endl;

    exampleWithErrorCode();
    exampleWithExceptions();
    exampleWithEventLoop();

    std::cout << "\nAll examples completed." << std::endl;
    return 0;
}