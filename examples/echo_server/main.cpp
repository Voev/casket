#include <iostream>
#include <thread>
#include <chrono>

#include <casket/transport/unix_socket.hpp>
#include <casket/server/generic_server.hpp>

using namespace casket;

class EchoServer
{
private:
    GenericServer<UnixSocket> server_;

public:
    EchoServer()
    {
        server_.setConnectionHandler(
            [](UnixSocket& client, const std::vector<uint8_t>& data)
            {
                std::string message(data.begin(), data.end());
                std::cout << "Received: " << message << std::endl;

                std::string response = "Echo: " + message;
                client.send(reinterpret_cast<const uint8_t*>(response.c_str()), response.size());
            });

        server_.setErrorHandler([](const std::error_code& ec)
                                { std::cerr << "Server error: " << ec.message() << std::endl; });
    }

    bool start(const std::string& address)
    {
        if (!server_.listen(address))
        {
            std::cerr << "Failed to listen on " << address << std::endl;
            return false;
        }

        std::cout << "Echo server started on " << address << std::endl;
        server_.start();
        return true;
    }

    void stop()
    {
        server_.stop();
        std::cout << "Server stopped" << std::endl;
    }

    bool isRunning() const
    {
        return server_.isRunning();
    }

    size_t getClientCount() const
    {
        return server_.getClientCount();
    }
};

int main()
{
    EchoServer server;

    if (!server.start("/tmp/echo_server"))
    {
        return 1;
    }

    while (server.isRunning())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Active connections: " << server.getClientCount() << std::endl;
    }

    return 0;
}