#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <casket/transport/unix_socket.hpp>

using namespace casket;

class UnixSocketTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        socketPath_ = "/tmp/unix_socket_test_" + std::to_string(getpid());
    }

    void TearDown() override
    {
        unlink(socketPath_.c_str());
    }

    std::string socketPath_;
};

TEST_F(UnixSocketTest, ConnectFailsWhenServerNotExists)
{
    UnixSocket client;

    bool connected = client.connect("/tmp/non_existent_socket");

    EXPECT_FALSE(connected);
    EXPECT_FALSE(client.isConnected());
    EXPECT_NE(client.lastError().value(), 0);
}

TEST_F(UnixSocketTest, ConnectSuccessWhenServerExists)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    bool connected = client.connect(socketPath_);

    EXPECT_TRUE(connected);
    EXPECT_TRUE(client.isConnected());
    EXPECT_EQ(client.lastError().value(), 0);
}

TEST_F(UnixSocketTest, SendAndReceiveData)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    std::string message = "Hello, Server!";
    ssize_t sent = client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size());

    EXPECT_EQ(sent, static_cast<ssize_t>(message.size()));
    EXPECT_EQ(client.lastError().value(), 0);

    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer));

    EXPECT_EQ(received, static_cast<ssize_t>(message.size()));
    EXPECT_EQ(std::string(buffer, buffer + received), message);
    EXPECT_EQ(clientSocket.lastError().value(), 0);
}

TEST_F(UnixSocketTest, SendAndReceiveMultipleMessages)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    std::vector<std::string> messages = {"Message 1", "Message 2", "Message 3"};

    for (const auto& msg : messages)
    {
        ssize_t sent = client.send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
        EXPECT_EQ(sent, static_cast<ssize_t>(msg.size()));

        uint8_t buffer[256];
        ssize_t received = clientSocket.recv(buffer, sizeof(buffer));
        EXPECT_EQ(received, static_cast<ssize_t>(msg.size()));
        EXPECT_EQ(std::string(buffer, buffer + received), msg);
    }
}

TEST_F(UnixSocketTest, SendEmptyMessage)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    ssize_t sent = client.send(nullptr, 0);
    EXPECT_EQ(sent, 0);

    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer));
    EXPECT_EQ(received, -1);
    EXPECT_EQ(clientSocket.lastError(), std::errc::resource_unavailable_try_again);
}

TEST_F(UnixSocketTest, RecvWithSmallBuffer)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    std::string message = "This is a long message";
    client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size());

    uint8_t smallBuffer[10];
    ssize_t received = clientSocket.recv(smallBuffer, sizeof(smallBuffer));

    EXPECT_EQ(received, sizeof(smallBuffer));
    EXPECT_EQ(std::string(smallBuffer, smallBuffer + received), message.substr(0, 10));
}

TEST_F(UnixSocketTest, RecvReturnsWouldBlockWhenNoData)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer));

    EXPECT_EQ(received, -1);
    EXPECT_EQ(clientSocket.lastError(), std::errc::resource_unavailable_try_again);
}

TEST_F(UnixSocketTest, SendReturnsWouldBlockWhenBufferFull)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    std::vector<uint8_t> largeBuffer(1024 * 1024, 'x');
    ssize_t totalSent = 0;
    bool wouldBlock = false;

    for (int i = 0; i < 1000; ++i)
    {
        ssize_t sent = client.send(largeBuffer.data(), largeBuffer.size());
        if (sent < 0)
        {
            if (client.lastError() == std::errc::resource_unavailable_try_again)
            {
                wouldBlock = true;
                break;
            }
        }
        else
        {
            totalSent += sent;
        }
    }

    EXPECT_TRUE(wouldBlock || totalSent > 0);
}

TEST_F(UnixSocketTest, ServerListenAndAccept)
{
    UnixSocket server;

    bool listening = server.listen(socketPath_);
    ASSERT_TRUE(listening);
    EXPECT_TRUE(server.isConnected());
    EXPECT_GE(server.getFd(), 0);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    EXPECT_TRUE(clientSocket.isConnected());
    EXPECT_GE(clientSocket.getFd(), 0);
}

TEST_F(UnixSocketTest, AcceptWithoutClient)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    auto clientSocket = server.accept();

    EXPECT_FALSE(clientSocket.isConnected());
    EXPECT_EQ(clientSocket.getFd(), -1);
}

TEST_F(UnixSocketTest, MultipleClientsConnect)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    const int numClients = 5;
    std::vector<UnixSocket> clients;

    for (int i = 0; i < numClients; ++i)
    {
        UnixSocket client;
        ASSERT_TRUE(client.connect(socketPath_));
        clients.push_back(std::move(client));
    }

    for (int i = 0; i < numClients; ++i)
    {
        auto clientSocket = server.accept();
        EXPECT_TRUE(clientSocket.isConnected());
    }
}

TEST_F(UnixSocketTest, CloseConnection)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    client.close();

    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.getFd(), -1);

    uint8_t data[] = {1, 2, 3};
    ssize_t sent = client.send(data, sizeof(data));

    EXPECT_EQ(sent, -1);
    EXPECT_EQ(client.lastError(), std::errc::not_connected);
}

TEST_F(UnixSocketTest, ServerCleanupSocketFile)
{
    std::string socketPath = socketPath_;

    {
        UnixSocket server;
        ASSERT_TRUE(server.listen(socketPath));

        struct stat st;
        EXPECT_EQ(stat(socketPath.c_str(), &st), 0);

        server.close();
    }

    struct stat st;
    EXPECT_NE(stat(socketPath.c_str(), &st), 0);
}

TEST_F(UnixSocketTest, ConcurrentSendReceive)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto clientSocket = server.accept();
    ASSERT_TRUE(clientSocket.isConnected());

    std::thread sender(
        [&client]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::string message = "Concurrent message";
            client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size());
        });

    uint8_t buffer[256];
    ssize_t received = 0;

    for (int i = 0; i < 100; ++i)
    {
        received = clientSocket.recv(buffer, sizeof(buffer));
        if (received > 0)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sender.join();

    EXPECT_GT(received, 0);
}

TEST_F(UnixSocketTest, BidirectionalCommunication)
{
    UnixSocket server;
    ASSERT_TRUE(server.listen(socketPath_));

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_));

    auto serverClient = server.accept();
    ASSERT_TRUE(serverClient.isConnected());

    std::string msg1 = "Client to Server";
    std::string msg2 = "Server to Client";

    client.send(reinterpret_cast<const uint8_t*>(msg1.data()), msg1.size());

    uint8_t buffer[256];
    ssize_t received = serverClient.recv(buffer, sizeof(buffer));
    EXPECT_EQ(std::string(buffer, buffer + received), msg1);

    serverClient.send(reinterpret_cast<const uint8_t*>(msg2.data()), msg2.size());

    received = client.recv(buffer, sizeof(buffer));
    EXPECT_EQ(std::string(buffer, buffer + received), msg2);
}

TEST_F(UnixSocketTest, ErrorCodeAfterFailedOperation)
{
    UnixSocket socket;

    uint8_t data[] = {1, 2, 3};
    ssize_t sent = socket.send(data, sizeof(data));

    EXPECT_EQ(sent, -1);
    EXPECT_EQ(socket.lastError(), std::errc::not_connected);
}

TEST_F(UnixSocketTest, ErrorCodeAfterFailedAccept)
{
    UnixSocket notListeningSocket;

    auto client = notListeningSocket.accept();
    EXPECT_FALSE(client.isConnected());
    EXPECT_FALSE(notListeningSocket.lastError());
}
