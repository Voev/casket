#pragma once

#include <chrono>
#include <system_error>
#include <cstring>

#include <casket/transport/transport_base.hpp>
#include <casket/transport/unix_socket.hpp>
#include <casket/transport/tcp_socket.hpp>

#include <casket/transport/byte_buffer.hpp>
#include <casket/pack/pack.hpp>

namespace casket
{

template <typename Transport>
class GenericClient
{
public:
    explicit GenericClient(size_t bufferSize = 8192)
        : readBuffer_(bufferSize)
        , writeBuffer_(bufferSize)
    {
    }

    bool connect(const std::string& address, int port, bool nonblocking, std::error_code& ec)
    {
        if constexpr (std::is_same_v<Transport, UnixSocket>)
        {
            return transport_.connect(address, nonblocking, ec);
        }
        else if constexpr (std::is_same_v<Transport, TcpSocket>)
        {
            return transport_.connect(address, port, nonblocking, ec);
        }
        return false;
    }

    bool isConnected(std::chrono::milliseconds timeout, std::error_code& ec)
    {
        return transport_.isConnected(timeout, ec);
    }

    void close()
    {
        transport_.close();
    }

    int getFd() const
    {
        return transport_.getFd();
    }

    bool isValid() const
    {
        return transport_.isValid();
    }

    // Отправка с упаковкой
    template <typename T>
    bool send(const T& message, std::error_code& ec)
    {
        // Пробуем упаковать в текущий буфер
        Packer packer(writeBuffer_.getWritePtr(), writeBuffer_.availableWrite());

        if (!message.pack(packer))
        {
            // Не хватило места - расширяем и пробуем снова
            writeBuffer_.expand(writeBuffer_.capacity() * 2);
            Packer packer2(writeBuffer_.getWritePtr(), writeBuffer_.availableWrite());

            if (!message.pack(packer2))
            {
                ec = std::make_error_code(std::errc::no_buffer_space);
                return false;
            }
            writeBuffer_.commitWrite(packer2.position());
        }
        else
        {
            writeBuffer_.commitWrite(packer.position());
        }

        // Отправляем
        ssize_t sent = transport_.sendBuffer(writeBuffer_, ec);

        if (sent < 0 && ec != std::errc::resource_unavailable_try_again)
        {
            return false;
        }

        return true;
    }

    // Отправка сырых данных
    bool sendRaw(const uint8_t* data, size_t len, std::error_code& ec)
    {
        if (writeBuffer_.availableWrite() < len)
        {
            writeBuffer_.expand(writeBuffer_.capacity() + len);
        }

        std::memcpy(writeBuffer_.getWritePtr(), data, len);
        writeBuffer_.commitWrite(len);

        ssize_t sent = transport_.sendBuffer(writeBuffer_, ec);
        return sent > 0 || ec == std::errc::resource_unavailable_try_again;
    }

    // Получение с распаковкой
    template <typename T>
    UnpackResult<T> receive(std::error_code& ec)
    {
        // Если в буфере нет данных - читаем из сокета
        if (readBuffer_.availableRead() == 0)
        {
            ssize_t n = transport_.recvBuffer(readBuffer_, ec);
            if (n <= 0)
            {
                return UnpackResult<T>(UnpackerError::PrematureEnd);
            }
        }

        // Распаковываем
        Unpacker unpacker(readBuffer_.getReadPtr(), readBuffer_.availableRead());
        auto result = T::unpack(unpacker);

        if (result)
        {
            readBuffer_.commitRead(unpacker.position());
        }

        return result;
    }

    // Получение сырых данных
    std::optional<std::vector<uint8_t>> receiveRaw(size_t maxSize, std::error_code& ec)
    {
        if (readBuffer_.availableRead() == 0)
        {
            ssize_t n = transport_.recvBuffer(readBuffer_, ec);
            if (n <= 0)
            {
                return std::nullopt;
            }
        }

        size_t toRead = std::min(maxSize, readBuffer_.availableRead());
        std::vector<uint8_t> result(readBuffer_.getReadPtr(), readBuffer_.getReadPtr() + toRead);
        readBuffer_.commitRead(toRead);

        return result;
    }

    // Отправка всех накопленных данных
    void flush(std::error_code& ec)
    {
        if (writeBuffer_.availableRead() > 0)
        {
            transport_.sendBuffer(writeBuffer_, ec);
        }
    }

    bool hasDataToWrite() const
    {
        return writeBuffer_.availableRead() > 0;
    }

    bool hasDataToRead() const
    {
        return readBuffer_.availableRead() > 0;
    }

    void clear()
    {
        readBuffer_.clear();
        writeBuffer_.clear();
    }

    Transport& getTransport()
    {
        return transport_;
    }
    const Transport& getTransport() const
    {
        return transport_;
    }

    ByteBuffer& getReadBuffer()
    {
        return readBuffer_;
    }
    ByteBuffer& getWriteBuffer()
    {
        return writeBuffer_;
    }

private:
    Transport transport_;
    ByteBuffer readBuffer_;
    ByteBuffer writeBuffer_;
};

} // namespace casket