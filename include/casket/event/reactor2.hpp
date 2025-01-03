#pragma once
#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")


namespace casket::event
{

class Reactor {
public:
    Reactor() : iocpHandle(NULL), isRunning(false) {}

    ~Reactor() {
        stop();
    }

    bool initialize() {
        iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (iocpHandle == NULL) {
            std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
            return false;
        }
        return true;
    }

    bool register_socket(SOCKET socket) {
        HANDLE handle = CreateIoCompletionPort((HANDLE)socket, iocpHandle, (ULONG_PTR)socket, 0);
        if (handle == NULL) {
            std::cerr << "CreateIoCompletionPort for socket failed: " << GetLastError() << std::endl;
            return false;
        }
        // Could optionally add to a map or similar for other purposes.
        return true;
    }

    void start() {
        isRunning = true;
        workerThread = std::thread([this]() { this->event_loop(); });
    }

    void stop() {
        if (isRunning) {
            isRunning = false;
            PostQueuedCompletionStatus(iocpHandle, 0, NULL, NULL); // Wake up the IOCP thread
            workerThread.join();
            CloseHandle(iocpHandle);
        }
    }

private:
    HANDLE iocpHandle;
    bool isRunning;
    std::thread workerThread;

    struct IOContext : OVERLAPPED {
        WSABUF buffer;
        char data[512];
        SOCKET socket;
    };

    void event_loop() {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* overlapped;

        while (isRunning) {
            BOOL result = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);
            if (!result) {
                std::cerr << "GetQueuedCompletionStatus failed: " << GetLastError() << std::endl;
                continue;
            }

            if (overlapped == nullptr && !completionKey) {
                // Should break the loop on graceful shutdown signal.
                break;
            }

            IOContext* context = static_cast<IOContext*>(overlapped);
            if (bytesTransferred > 0) {
                // Handle the data that was received
                std::cout << "Received data: " << std::string(context->data, bytesTransferred) << std::endl;

                // Echo the data back
                DWORD sendBytes = 0;
                WSABUF sendBuffer = { bytesTransferred, context->data };
                int sendResult = WSASend(context->socket, &sendBuffer, 1, &sendBytes, 0, nullptr, nullptr);
                if (sendResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    std::cerr << "WSASend failed: " << WSAGetLastError() << std::endl;
                    closesocket(context->socket);
                }
            } else {
                // Connection was closed or there was an error
                closesocket(context->socket);
                delete context; // Free the context memory when done
            }
        }
    }

    IOContext* create_io_context(SOCKET socket) {
        IOContext* context = new IOContext();
        ZeroMemory(context, sizeof(IOContext));
        context->socket = socket;
        context->buffer.buf = context->data;
        context->buffer.len = sizeof(context->data);
        return context;
    }

public:
    bool post_receive(SOCKET socket) {
        IOContext* context = create_io_context(socket);
        DWORD flags = 0;
        DWORD bytesReceived = 0;
        int result = WSARecv(socket, &context->buffer, 1, &bytesReceived, &flags, context, nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSARecv failed: " << WSAGetLastError() << std::endl;
            delete context;
            return false;
        }
        return true;
    }
};

}