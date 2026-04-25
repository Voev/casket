#include <iostream>
#include <chrono>
#include <cstring>

#include <casket/transport/unix_socket.hpp>
#include <casket/server/generic_server.hpp>
#include <casket/signal/signal_handler.hpp>

using namespace casket;

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
        {
            return UnpackResult<EchoMessage>(UnpackerError::PrematureEnd);
        }
        return EchoMessage{result.value()};
    }
};

int main()
{
    GenericServer<UnixSocket> server;
    SignalHandler signalHandler;
    std::atomic_bool interrupted{false};

    server.setConnectionHandler(
        [](Context<UnixSocket>& ctx)
        {
            std::error_code ec;
            auto result = ctx.readThenUnpack<EchoMessage>(ec);
            if (!result)
            {
                std::cerr << "Error: " << (int)result.error() << std::endl;
            }
            else
            {
                std::cout << "Received: " << result->text << std::endl;
                ctx.packThenSend(EchoMessage{result->text}, ec);
            }
        });

    server.setErrorHandler([](const std::error_code& ec) { std::cout << "Error: " << ec.message() << std::endl; });

    int signals[] = {SIGINT, SIGTERM};

    signalHandler.registerSignals(signals,
                                  [&interrupted](int signum)
                                  {
                                      std::cout << "\nReceived signal " << signum << " (SIGINT), shutting down..."
                                                << std::endl;
                                      interrupted = true;
                                  });

    std::error_code ec{};
    if (!server.listen("/tmp/echo_server", -1, 128, ec))
    {
        std::cerr << "Failed to listen: " << ec.message() << std::endl;
        return 1;
    }

    server.start();
    std::cout << "Server running in main thread" << std::endl;

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