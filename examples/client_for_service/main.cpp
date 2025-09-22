#include <iostream>
#include <vector>
#include <cstdint>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <string>

// Утилиты для работы с бинарным протоколом
std::vector<uint8_t> create_binary_request(const std::string& command, const std::vector<uint8_t>& data = {}) {
    std::vector<uint8_t> request;
    request.push_back(static_cast<uint8_t>(command.size()));
    request.insert(request.end(), command.begin(), command.end());
    request.insert(request.end(), data.begin(), data.end());
    return request;
}

std::vector<uint8_t> send_request(int sockfd, const std::string& command, const std::vector<uint8_t>& data = {}) {
    // Создаем бинарный запрос
    auto request_data = create_binary_request(command, data);
    
    // Добавляем заголовок длины
    uint32_t total_length = request_data.size();
    std::vector<uint8_t> packet(sizeof(uint32_t) + total_length);
    std::memcpy(packet.data(), &total_length, sizeof(uint32_t));
    std::memcpy(packet.data() + sizeof(uint32_t), request_data.data(), total_length);
    
    // Отправляем запрос с проверкой ошибок
    const uint8_t* packet_data = packet.data();
    size_t remaining = packet.size();
    
    while (remaining > 0) {
        ssize_t sent = write(sockfd, packet_data, remaining);
        if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Попробуем снова
                continue;
            } else {
                throw std::system_error(errno, std::system_category(), "write failed");
            }
        }
        packet_data += sent;
        remaining -= sent;
    }
    
    // Читаем заголовок ответа
    uint32_t response_length;
    ssize_t bytes_read = read(sockfd, &response_length, sizeof(uint32_t));
    if (bytes_read != sizeof(uint32_t)) {
        if (bytes_read == -1) {
            throw std::system_error(errno, std::system_category(), "read header failed");
        } else {
            throw std::runtime_error("Incomplete header received");
        }
    }
    
    // Читаем данные ответа
    std::vector<uint8_t> response(response_length);
    uint8_t* response_data = response.data();
    remaining = response_length;
    
    while (remaining > 0) {
        bytes_read = read(sockfd, response_data, remaining);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                throw std::system_error(errno, std::system_category(), "read data failed");
            }
        } else if (bytes_read == 0) {
            throw std::runtime_error("Connection closed by server");
        }
        response_data += bytes_read;
        remaining -= bytes_read;
    }
    
    return response;
}

int main() {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }
    
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/service_manager.sock", sizeof(addr.sun_path)-1);
    
    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        return 1;
    }
    
    std::cout << "Connected to ServiceManager" << std::endl;
    
    // Тестируем ping
    auto response = send_request(sockfd, "ping");
    std::cout << "Ping response: " << std::string(response.begin(), response.end()) << std::endl;
    
    // Тестируем echo
    std::vector<uint8_t> test_data = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    response = send_request(sockfd, "echo", test_data);
    std::cout << "Echo response: " << std::string(response.begin(), response.end()) << std::endl;
    
    // Тестируем math
    std::vector<uint8_t> math_data;
    math_data.push_back('+'); // операция сложения
    double a = 15.7, b = 3.2;
    math_data.insert(math_data.end(), reinterpret_cast<uint8_t*>(&a), reinterpret_cast<uint8_t*>(&a) + sizeof(double));
    math_data.insert(math_data.end(), reinterpret_cast<uint8_t*>(&b), reinterpret_cast<uint8_t*>(&b) + sizeof(double));
    
    response = send_request(sockfd, "math", math_data);
    if (response.size() == sizeof(double)) {
        double result;
        std::memcpy(&result, response.data(), sizeof(double));
        std::cout << "Math result: " << result << std::endl;
    }
    
    // Тестируем stats
    std::vector<uint8_t> stats_data = {10, 20, 30, 40, 50};
    response = send_request(sockfd, "stats", stats_data);
    std::cout << "Stats response: " << std::string(response.begin(), response.end()) << std::endl;
    
    // Тестируем текстовый обработчик
    std::string text = "hello world";
    response = send_request(sockfd, "upper", std::vector<uint8_t>(text.begin(), text.end()));
    std::cout << "Upper response: " << std::string(response.begin(), response.end()) << std::endl;
    
    // Тестируем info
    response = send_request(sockfd, "info");
    std::cout << "Info response:\n" << std::string(response.begin(), response.end()) << std::endl;
    
    close(sockfd);
    return 0;
}