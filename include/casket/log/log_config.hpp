#pragma once
#include <cstddef>

namespace casket
{

struct LoggerConfig {
    size_t queue_size = 16384;        // размер очереди (будет округлён до степени двойки)
    size_t max_msg_size = 512;        // максимальный размер сообщения
    size_t max_sinks = 32;            // максимальное количество синков
    size_t flush_interval_ms = 100;   // интервал автоматического сброса (миллисекунды)
    size_t backoff_sleep_us = 100;    // сон при отсутствии сообщений (микросекунды)
    bool enable_stats = true;         // собирать статистику
    bool overwrite_on_full = false;   // перезаписывать при переполнении (иначе дропать)
    
    LoggerConfig() = default;

    void setQueueSize(size_t size) {
        queue_size = nextPowerOfTwo(size);
    }
    
private:
    static size_t nextPowerOfTwo(size_t n) {
        if (n == 0) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        n++;
        return n;
    }
};

}