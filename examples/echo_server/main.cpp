#include <iostream>
#include <chrono>
#include <cstring>

#include <casket/transport/unix_socket.hpp>
#include <casket/server/generic_server.hpp>
#include <casket/signal/signal_handler.hpp>

using namespace casket;

int main()
{
    GenericServer<UnixSocket> server;
    SignalHandler signalHandler;
    std::atomic_bool interrupted{false};

    server.setConnectionHandler(
        [](ByteBuffer& request, ByteBuffer& response)
        {
            size_t available = request.availableRead();
            if (available > 0)
            {
                auto data = request.peekContiguous(available);
                
                response.write(data.data(), data.size());
                
                request.consume(available);
            }
        });

    server.setErrorHandler([](const std::error_code& ec)
    {
        std::cout << "Error: " << ec.message() << std::endl;
    });

    int signals[] = {SIGINT, SIGTERM};

    signalHandler.registerSignals(signals,
                                  [&interrupted](int signum)
                                  {
                                      std::cout << "\nReceived signal " << signum << " (SIGINT), shutting down..."
                                                << std::endl;
                                      interrupted = true;
                                  });

    if (!server.listen("/tmp/echo_server"))
    {
        std::cerr << "Failed to listen" << std::endl;
        return 1;
    }

    server.start();
    std::cout << "Server running in main thread" << std::endl;

    std::error_code ec{};
    while (!interrupted)
    {
        if (!server.step())
        {
            break;
        }

        signalHandler.processSignals(ec);
        if (ec)
        {
            std::cerr << "Error processing signals: " << ec.message() << std::endl;
        }

        enablePeriodicStats(server, std::chrono::seconds(3), std::cout);
    }

    server.stop();
    std::cout << "Server stopped" << std::endl;
    return EXIT_SUCCESS;
}