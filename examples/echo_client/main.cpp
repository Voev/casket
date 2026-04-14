#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

#include <casket/transport/unix_socket.hpp>
#include <casket/types/byte_buffer.hpp>

using namespace casket;

int main()
{
    UnixSocket client;
    std::string socketPath = "/tmp/echo_server";

    std::cout << "Connecting to " << socketPath << "..." << std::endl;
    if (!client.connect(socketPath))
    {
        std::cerr << "Failed to connect!" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Connected!" << std::endl;

    std::string message(1024, 'A');
    std::cout << "Sending: " << message << " (" << message.size() << " bytes)" << std::endl;

    ssize_t sent = client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    std::cout << "Sent: " << sent << " bytes" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint8_t buffer[4096];
    ssize_t received = client.recv(buffer, sizeof(buffer));

    if (received > 0)
    {
        std::cout << "Received: " << received << " bytes" << std::endl;
        std::cout << "Response: " << std::string(reinterpret_cast<char*>(buffer), received) << std::endl;
    }
    else
    {
        std::cout << "No response received (received = " << received << ")" << std::endl;
    }

    return EXIT_SUCCESS;
}