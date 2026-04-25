#include <iostream>
#include <casket/transport/unix_socket.hpp>
#include <casket/client/generic_client.hpp>

using namespace casket;

struct EchoMessage
{
    std::string_view text;

    PackResult<Packer*> pack(Packer& packer) const
    {
        return packer.pack(text);
    }

    static UnpackResult<EchoMessage> unpack(Unpacker& unpacker)
    {
        auto result = unpacker.unpackString();
        if (!result)
            return UnpackResult<EchoMessage>(UnpackerError::PrematureEnd);
        return EchoMessage{result.value()};
    }
};

int main()
{
    std::error_code ec;
    GenericClient<UnixSocket> client;
    
    // Подключаемся
    if (!client.connect("/tmp/echo_server", -1, false, ec))
    {
        std::cerr << "Failed to connect: " << ec.message() << std::endl;
        return 1;
    }
    
    // Отправляем
    EchoMessage msg{"Hello, Server!"};
    if (!client.send(msg, ec))
    {
        std::cerr << "Failed to send: " << ec.message() << std::endl;
        return 1;
    }
    
    // Получаем ответ
    auto result = client.receive<EchoMessage>(ec);
    if (!result)
    {
        std::cerr << "Failed to receive: " << ec.message() << std::endl;
        return 1;
    }
    
    std::cout << "Response: " << result->text << std::endl;
    
    client.close();
    return 0;
}