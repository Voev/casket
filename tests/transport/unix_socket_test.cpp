#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
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
    std::error_code ec;

    bool connected = client.connect("/tmp/non_existent_socket", false, ec);

    EXPECT_FALSE(connected);
    EXPECT_FALSE(client.isValid());
    EXPECT_NE(ec.value(), 0);
}

TEST_F(UnixSocketTest, ConnectSuccessWhenServerExists)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    bool connected = client.connect(socketPath_, false, ec);

    EXPECT_TRUE(connected);
    EXPECT_TRUE(client.isValid());
    EXPECT_EQ(ec.value(), 0);
}

TEST_F(UnixSocketTest, SendAndReceiveData)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    std::string message = "Hello, Server!";
    ssize_t sent = client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size(), ec);

    EXPECT_EQ(sent, static_cast<ssize_t>(message.size()));
    EXPECT_FALSE(ec);

    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer), ec);

    EXPECT_EQ(received, static_cast<ssize_t>(message.size()));
    EXPECT_EQ(std::string(buffer, buffer + received), message);
    EXPECT_FALSE(ec);
}

TEST_F(UnixSocketTest, SendAndReceiveMultipleMessages)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    std::vector<std::string> messages = {"Message 1", "Message 2", "Message 3"};

    for (const auto& msg : messages)
    {
        ssize_t sent = client.send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), ec);
        EXPECT_EQ(sent, static_cast<ssize_t>(msg.size()));
        EXPECT_FALSE(ec);

        uint8_t buffer[256];
        ssize_t received = clientSocket.recv(buffer, sizeof(buffer), ec);
        EXPECT_EQ(received, static_cast<ssize_t>(msg.size()));
        EXPECT_EQ(std::string(buffer, buffer + received), msg);
        EXPECT_FALSE(ec);
    }
}

TEST_F(UnixSocketTest, SendEmptyMessage)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    ssize_t sent = client.send(nullptr, 0, ec);
    EXPECT_EQ(sent, 0);
    EXPECT_FALSE(ec);

    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer), ec);
    EXPECT_EQ(received, -1);
    EXPECT_EQ(ec, std::errc::resource_unavailable_try_again);
}

TEST_F(UnixSocketTest, RecvWithSmallBuffer)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    std::string message = "This is a long message";
    client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size(), ec);
    ASSERT_FALSE(ec);

    uint8_t smallBuffer[10];
    ssize_t received = clientSocket.recv(smallBuffer, sizeof(smallBuffer), ec);

    EXPECT_EQ(received, sizeof(smallBuffer));
    EXPECT_EQ(std::string(smallBuffer, smallBuffer + received), message.substr(0, 10));
    EXPECT_FALSE(ec);
}

TEST_F(UnixSocketTest, RecvReturnsWouldBlockWhenNoData)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer), ec);

    EXPECT_EQ(received, -1);
    EXPECT_EQ(ec, std::errc::resource_unavailable_try_again);
}

TEST_F(UnixSocketTest, ServerListenAndAccept)
{
    UnixSocket server;
    std::error_code ec;

    bool listening = server.listen(socketPath_, 5, ec);
    ASSERT_TRUE(listening);
    ASSERT_FALSE(ec);
    EXPECT_TRUE(server.isValid());
    EXPECT_GE(server.getFd(), 0);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    EXPECT_TRUE(clientSocket.isValid());
    EXPECT_GE(clientSocket.getFd(), 0);
    EXPECT_FALSE(ec);
}

TEST_F(UnixSocketTest, AcceptWithoutClient)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);

    EXPECT_FALSE(clientSocket.isValid());
    EXPECT_EQ(clientSocket.getFd(), -1);
    // В неблокирующем режиме accept вернёт ошибку EAGAIN/EWOULDBLOCK
    EXPECT_TRUE(ec == std::errc::resource_unavailable_try_again || ec.value() != 0);
}

TEST_F(UnixSocketTest, MultipleClientsConnect)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 10, ec));
    ASSERT_FALSE(ec);

    const int numClients = 5;
    std::vector<UnixSocket> clients;

    for (int i = 0; i < numClients; ++i)
    {
        UnixSocket client;
        ASSERT_TRUE(client.connect(socketPath_, false, ec));
        ASSERT_FALSE(ec);
        clients.push_back(std::move(client));
    }

    for (int i = 0; i < numClients; ++i)
    {
        auto clientSocket = server.accept(ec);
        EXPECT_TRUE(clientSocket.isValid());
        EXPECT_FALSE(ec);
    }
}

TEST_F(UnixSocketTest, CloseConnection)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    client.close();

    EXPECT_FALSE(client.isValid());
    EXPECT_EQ(client.getFd(), -1);

    uint8_t data[] = {1, 2, 3};
    ssize_t sent = client.send(data, sizeof(data), ec);

    EXPECT_EQ(sent, -1);
    EXPECT_EQ(ec, std::errc::bad_file_descriptor);
}

TEST_F(UnixSocketTest, ServerCleanupSocketFile)
{
    std::string socketPath = socketPath_;
    std::error_code ec;

    {
        UnixSocket server;
        ASSERT_TRUE(server.listen(socketPath, 5, ec));
        ASSERT_FALSE(ec);

        struct stat st;
        EXPECT_EQ(stat(socketPath.c_str(), &st), 0);
    } // server уничтожается здесь, файл сокета должен быть удалён

    struct stat st;
    EXPECT_NE(stat(socketPath.c_str(), &st), 0);
}

TEST_F(UnixSocketTest, ConcurrentSendReceive)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    std::thread sender(
        [&client]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::string message = "Concurrent message";
            std::error_code sendEc;
            client.send(reinterpret_cast<const uint8_t*>(message.data()), message.size(), sendEc);
        });

    uint8_t buffer[256];
    ssize_t received = 0;
    std::error_code recvEc;

    for (int i = 0; i < 100; ++i)
    {
        received = clientSocket.recv(buffer, sizeof(buffer), recvEc);
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
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    ASSERT_TRUE(client.connect(socketPath_, false, ec));
    ASSERT_FALSE(ec);

    auto serverClient = server.accept(ec);
    ASSERT_TRUE(serverClient.isValid());
    ASSERT_FALSE(ec);

    std::string msg1 = "Client to Server";
    std::string msg2 = "Server to Client";

    client.send(reinterpret_cast<const uint8_t*>(msg1.data()), msg1.size(), ec);
    ASSERT_FALSE(ec);

    uint8_t buffer[256];
    ssize_t received = serverClient.recv(buffer, sizeof(buffer), ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(std::string(buffer, buffer + received), msg1);

    serverClient.send(reinterpret_cast<const uint8_t*>(msg2.data()), msg2.size(), ec);
    ASSERT_FALSE(ec);

    received = client.recv(buffer, sizeof(buffer), ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(std::string(buffer, buffer + received), msg2);
}

TEST_F(UnixSocketTest, ErrorCodeAfterFailedOperation)
{
    UnixSocket socket;
    std::error_code ec;

    uint8_t data[] = {1, 2, 3};
    ssize_t sent = socket.send(data, sizeof(data), ec);

    EXPECT_EQ(sent, -1);
    EXPECT_EQ(ec, std::errc::bad_file_descriptor);
}

TEST_F(UnixSocketTest, ErrorCodeAfterFailedAccept)
{
    UnixSocket notListeningSocket;
    std::error_code ec;

    auto client = notListeningSocket.accept(ec);
    EXPECT_FALSE(client.isValid());
    EXPECT_EQ(ec, std::errc::not_connected);
}

TEST_F(UnixSocketTest, NonBlockingMode)
{
    UnixSocket server;
    std::error_code ec;
    ASSERT_TRUE(server.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    UnixSocket client;
    // Подключаемся в неблокирующем режиме
    ASSERT_TRUE(client.connect(socketPath_, true, ec));
    ASSERT_FALSE(ec);

    auto clientSocket = server.accept(ec);
    ASSERT_TRUE(clientSocket.isValid());
    ASSERT_FALSE(ec);

    // Попытка чтения без данных должна вернуть would_block
    uint8_t buffer[256];
    ssize_t received = clientSocket.recv(buffer, sizeof(buffer), ec);

    EXPECT_EQ(received, -1);
    EXPECT_EQ(ec, std::errc::resource_unavailable_try_again);
}

TEST_F(UnixSocketTest, MoveSemantics)
{
    UnixSocket server1;
    std::error_code ec;
    ASSERT_TRUE(server1.listen(socketPath_, 5, ec));
    ASSERT_FALSE(ec);

    int originalFd = server1.getFd();
    EXPECT_NE(originalFd, -1);

    // Перемещение
    UnixSocket server2 = std::move(server1);
    EXPECT_EQ(server2.getFd(), originalFd);
    EXPECT_EQ(server1.getFd(), -1);
    EXPECT_FALSE(server1.isValid());

    // Перемещение через присваивание
    UnixSocket server3;
    server3 = std::move(server2);
    EXPECT_EQ(server3.getFd(), originalFd);
    EXPECT_EQ(server2.getFd(), -1);
}