#pragma once

#include <vector>
#include <optional>
#include <chrono>
#include <thread>
#include <casket/transport/transport_base.hpp>
#include <casket/transport/unix_socket.hpp>

namespace casket
{

template <typename Transport>
class GenericClient
{
public:
    GenericClient() = default;

    bool connect(const std::string& address, int port = 0)
    {
        if constexpr (std::is_same_v<Transport, UnixSocket>)
        {
            return transport_.connect(address);
        }
    }

    bool isConnected() const
    {
        return transport_.isConnected();
    }

    const std::error_code& lastError() const
    {
        return transport_.lastError();
    }

    void close()
    {
        transport_.close();
    }

    std::optional<std::vector<uint8_t>> sendSync(const uint8_t* data, size_t length,
                                                 std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        if (!transport_.isConnected())
        {
            return std::nullopt;
        }

        ssize_t sent = transport_.send(data, length);
        if (sent != static_cast<ssize_t>(length))
        {
            return std::nullopt;
        }

        auto start = std::chrono::steady_clock::now();
        std::vector<uint8_t> response;
        uint8_t buffer[4096];

        while (std::chrono::steady_clock::now() - start < timeout)
        {
            ssize_t received = transport_.recv(buffer, sizeof(buffer));

            if (received > 0)
            {
                response.insert(response.end(), buffer, buffer + received);
                return response;
            }
            else if (transport_.lastError() != std::errc::resource_unavailable_try_again)
            {
                return std::nullopt;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        return std::nullopt;
    }

    bool sendAsync(const uint8_t* data, size_t length)
    {
        if (!transport_.isConnected())
        {
            return false;
        }

        ssize_t sent = transport_.send(data, length);
        return sent == static_cast<ssize_t>(length);
    }

    std::optional<std::vector<uint8_t>> receive(std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
    {
        std::vector<uint8_t> result;
        uint8_t buffer[4096];

        auto start = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - start < timeout)
        {
            ssize_t received = transport_.recv(buffer, sizeof(buffer));

            if (received > 0)
            {
                result.insert(result.end(), buffer, buffer + received);
                return result;
            }
            else if (transport_.lastError() != std::errc::resource_unavailable_try_again)
            {
                return std::nullopt;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        return std::nullopt;
    }

    Transport& getTransport()
    {
        return transport_;
    }

    const Transport& getTransport() const
    {
        return transport_;
    }

private:
    Transport transport_;
};

} // namespace casket