#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    // Создайте сокет
    SOCKET connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        std::cerr << "ERROR on socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Установите адрес и порт сервера
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(27015);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    // Подключитесь к серверу
    if (connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "ERROR on connect: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // Отправьте сообщение серверу
    std::string message = "Hello, server!";
    int bytesSent = send(connectSocket, message.c_str(), message.length(), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "ERROR on send: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "Message was sended: " << message << std::endl;

    // Получите ответ от сервера
    char recvBuffer[512];
    int bytesReceived = recv(connectSocket, recvBuffer, sizeof(recvBuffer), 0);
    if (bytesReceived > 0) {
        std::cout << "Received from server: " << std::string(recvBuffer, bytesReceived) << std::endl;
    } else if (bytesReceived == 0) {
        std::cout << "Connection is closed" << std::endl;
    } else {
        std::cerr << "ERROR on recv: " << WSAGetLastError() << std::endl;
    }

    // Закрытие соединения
    closesocket(connectSocket);
    WSACleanup();

    return 0;
}