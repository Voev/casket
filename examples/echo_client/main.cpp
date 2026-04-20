#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

#include <casket/transport/unix_socket.hpp>
#include <casket/types/byte_buffer.hpp>
#include <casket/utils/exception.hpp>

using namespace casket;

int main()
{
    try
    {
        UnixSocket client;
        std::error_code ec;
        std::string socketPath = "/tmp/echo_server";

        std::cout << "Connecting to " << socketPath << "..." << std::endl;
        client.connect(socketPath, true, ec);
        ThrowIfError(ec, "failed to connect");
        std::cout << "Connected!" << std::endl;

        std::string message(1024, 'A');
        std::cout << "Sending: " << message << " (" << message.size() << " bytes)" << std::endl;
        ssize_t sent = client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size(), ec);
        ThrowIfError(ec, "failed to send data");

        std::cout << "Sent: " << sent << " bytes" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint8_t buffer[4096];
        ssize_t received = client.recv(buffer, sizeof(buffer), ec);
        client.close();
        ThrowIfError(ec, "failed to get receive data");

        if (received > 0)
        {
            std::cout << "Received: " << received << " bytes" << std::endl;
            std::cout << "Response: " << std::string(reinterpret_cast<char*>(buffer), received) << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}