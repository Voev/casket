#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <casket/event/reactor2.hpp>

#pragma comment(lib, "Ws2_32.lib")


// Handler for client communication
/*class ClientHandler : public casket::event::EventHandler
{
public:
    ClientHandler(SOCKET clientSocket)
        : clientSocket(clientSocket)
    {
    }

    void handle_event(int fd) override
    {
        char buffer[512];
        int result = recv((SOCKET)fd, buffer, sizeof(buffer), 0);
        if (result > 0)
        {
            std::cout << "Received data: " << std::string(buffer, result) << std::endl;
            send((SOCKET)fd, buffer, result, 0); // Echo the data back
        }
        else if (result == 0)
        {
            std::cout << "Connection closed by client" << std::endl;
            closesocket(clientSocket);
        }
        else
        {
            std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket);
        }
    }

private:
    SOCKET clientSocket;
};

// Handler for accepting new connections
class AcceptHandler : public casket::event::EventHandler
{
public:
    AcceptHandler(SOCKET listenSocket, casket::event::Reactor& reactor)
        : listenSocket(listenSocket)
        , reactor(reactor)
    {
    }

    void handle_event(int fd) override
    {
        SOCKET clientSocket = accept((SOCKET)fd, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            return;
        }
        std::cout << "Accepted new connection" << std::endl;
        // Register the new client socket for read events
        reactor.register_handler((int)clientSocket, new ClientHandler(clientSocket));
    }

private:
    SOCKET listenSocket;
    casket::event::Reactor& reactor;
};
*/

class EchoServer {
public:
    EchoServer(const char* ip, int port) : listenSocket(INVALID_SOCKET), reactor() {
        sockaddr_in service;
        service.sin_family = AF_INET;
        inet_pton(AF_INET, ip, &service.sin_addr.s_addr);
        service.sin_port = htons(port);

        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            return;
        }

        if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
            std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
            return;
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
            return;
        }

        if (!reactor.initialize()) {
            std::cerr << "Reactor initialization failed." << std::endl;
            closesocket(listenSocket);
        }
    }

    ~EchoServer() {
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
        }
        reactor.stop();
    }

    void run() {
        std::cout << "Server is running..." << std::endl;
        reactor.start();
        accept_connections();
    }

private:
    SOCKET listenSocket;
    casket::event::Reactor reactor;

    void accept_connections() {
        while (true) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
                continue;
            }

            // Register the client socket with the reactor
            if (!reactor.register_socket(clientSocket)) {
                std::cerr << "Failed to register client socket with reactor." << std::endl;
                closesocket(clientSocket);
                continue;
            }

            // Start receiving on the client socket
            if (!reactor.post_receive(clientSocket)) {
                std::cerr << "Failed to post receive on client socket." << std::endl;
                closesocket(clientSocket);
            }
        }
    }
};

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    EchoServer server("127.0.0.1", 27015);
    server.run();

    WSACleanup();
    return 0;
}