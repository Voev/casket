#include <casket/service/service_manager.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <numeric>

using namespace casket;
using namespace std::chrono_literals;

// Бинарный обработчик для ping команды
void ping_handler(const BinaryRequest& request, BinaryResponse& response) {
    (void)request;
    response = {'p', 'o', 'n', 'g'};
    std::cout << "Ping command processed" << std::endl;
}

// Бинарный обработчик для echo команды
void echo_handler(const BinaryRequest& request, BinaryResponse& response) {
    response = request; // Просто возвращаем полученные данные
    std::cout << "Echo command processed, data size: " << request.size() << " bytes" << std::endl;
}

// Обработчик для математических операций
void math_handler(const BinaryRequest& request, BinaryResponse& response) {
    if (request.size() < 2 * sizeof(double) + 1) {
        response = StringToBinary("ERROR: Invalid math request format");
        return;
    }
    
    char operation = static_cast<char>(request[0]);
    double a, b;
    std::memcpy(&a, request.data() + 1, sizeof(double));
    std::memcpy(&b, request.data() + 1 + sizeof(double), sizeof(double));
    
    double result = 0;
    switch (operation) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/': result = (b != 0) ? a / b : 0; break;
        default:
            response = StringToBinary("ERROR: Unknown operation");
            return;
    }
    
    response.resize(sizeof(double));
    std::memcpy(response.data(), &result, sizeof(double));
    std::cout << "Math operation: " << a << " " << operation << " " << b << " = " << result << std::endl;
}

// Обработчик для статистики данных
void stats_handler(const BinaryRequest& request, BinaryResponse& response) {
    if (request.empty()) {
        response = StringToBinary("ERROR: No data provided");
        return;
    }
    
    double sum = std::accumulate(request.begin(), request.end(), 0.0);
    double mean = sum / request.size();
    auto [min_it, max_it] = std::minmax_element(request.begin(), request.end());
    
    std::string result = "Sum: " + std::to_string(sum) + 
                        ", Mean: " + std::to_string(mean) +
                        ", Min: " + std::to_string(*min_it) +
                        ", Max: " + std::to_string(*max_it);
    
    response = StringToBinary(result);
    std::cout << "Stats calculated for " << request.size() << " values" << std::endl;
}

// Текстовый обработчик для совместимости
void text_upper_handler(const std::string& request, std::string& response) {
    response = request;
    std::transform(response.begin(), response.end(), response.begin(), ::toupper);
    std::cout << "Text upper processed: " << request << " -> " << response << std::endl;
}

// Обработчик для получения информации о сервере
void info_handler(const BinaryRequest& request, BinaryResponse& response) {
    (void)request;
    std::string info = "ServiceManager v1.0\n"
                      "Running: true\n"
                      "Protocol: binary\n"
                      "Max requests: 10000\n"
                      "Supported commands: ping, echo, math, stats, upper, info";
    
    response = StringToBinary(info);
    std::cout << "Info command processed" << std::endl;
}

int main() {
    try {
        // Создаем менеджер сервисов
        ServiceManager manager("/tmp/service_manager.sock");
        
        // Регистрируем бинарные обработчики
        manager.register_handler("ping", ping_handler);
        manager.register_handler("echo", echo_handler);
        manager.register_handler("math", math_handler);
        manager.register_handler("stats", stats_handler);
        manager.register_handler("info", info_handler);

        // Запускаем сервер
        if (!manager.start()) {
            std::cerr << "Failed to start ServiceManager" << std::endl;
            return 1;
        }
        
        std::cout << "ServiceManager started successfully!" << std::endl;
        std::cout << "Socket: /tmp/service_manager.sock" << std::endl;
        std::cout << "PID: " << getpid() << std::endl;
        std::cout << "Send test signals:" << std::endl;
        std::cout << "  kill -HUP " << getpid() << "  # Reload configuration" << std::endl;
        std::cout << "  kill -USR1 " << getpid() << " # Print statistics" << std::endl;
        std::cout << "  kill -INT " << getpid() << "  # Graceful shutdown" << std::endl;
        
        // Запускаем основной цикл
        manager.run();
        
        std::cout << "ServiceManager shutdown complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}