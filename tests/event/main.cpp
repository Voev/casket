#include <iostream>
#include <casket/event/EventLoop.hpp>

int main()
{
    EventLoop eventLoop;

    // Запускаем цикл обработки в отдельном потоке
    std::thread eventThread(&EventLoop::start, &eventLoop);

    // Регистрация событий и их обработчиков
    eventLoop.add([](EventLoop&) { std::cout << "Event onStart triggered!" << std::endl; }, std::ref(eventLoop));

    eventLoop.add([]() { std::cout << "Data received!" << std::endl; });

    eventLoop.add([]() { std::cout << "Event onEnd triggered!" << std::endl; });

    std::this_thread::sleep_for(std::chrono::seconds(1)); // Имитация работы
    eventLoop.stop();

    // Ожидание завершения потока
    eventThread.join();

    eventLoop.start();
    eventLoop.add([](EventLoop&) { std::cout << "Event onStart triggered!" << std::endl; }, std::ref(eventLoop));
    eventLoop.stop();

    return 0;
}