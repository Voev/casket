#include <iostream>
#include <chrono>
#include <cstring>

#include <casket/transport/unix_socket.hpp>
#include <casket/server/generic_server.hpp>

using namespace casket;

int main()
{
    GenericServer<UnixSocket> server;
    
    server.setConnectionHandler(
        [](ByteBuffer& request, ByteBuffer& response)
        {
            size_t readable = request.readable();
            if (readable == 0) return;
            
            const char* prefix = "Echo: ";
            response.write(reinterpret_cast<const uint8_t*>(prefix), std::strlen(prefix));
            
            while (readable > 0)
            {
                size_t avail;
                uint8_t* data = request.peek(avail);
                if (data && avail > 0)
                {
                    size_t toWrite = std::min(avail, readable);
                    response.write(data, toWrite);
                    request.skip(toWrite);
                    readable -= toWrite;
                }
                else
                {
                    break;
                }
            }
        });
    
    if (!server.listen("/tmp/echo_server"))
    {
        std::cerr << "Failed to listen" << std::endl;
        return 1;
    }
    
    server.start();
    std::cout << "Server running in main thread" << std::endl;

    while (true)
    {
        if (!server.step())
        {
            break;
        }
        
        static auto lastLog = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastLog > std::chrono::seconds(5))
        {
            std::cout << "Active connections: " << server.getClientCount() << std::endl;
            lastLog = now;
        }
    }

    server.stop();
    return EXIT_SUCCESS;
}