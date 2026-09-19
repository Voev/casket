#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <deque>
#include <algorithm>
#include <vector>

// ============================================
// Класс для скользящей медианы
// ============================================
template <size_t WINDOW_SIZE>
class RollingMedian {
public:
    RollingMedian() : window_size_(WINDOW_SIZE) {}

    float operator()(float value) {
        values_.push_back(value);
        if (values_.size() > window_size_) {
            values_.pop_front();
        }
        return calculate_median();
    }

    float operator()() const {
        return calculate_median();
    }

    void reset() {
        values_.clear();
    }

private:
    float calculate_median() const {
        if (values_.empty()) {
            return 0.0f;
        }

        std::vector<float> sorted(values_.begin(), values_.end());
        std::sort(sorted.begin(), sorted.end());

        size_t size = sorted.size();
        if (size % 2 == 1) {
            return sorted[size / 2];
        } else {
            return (sorted[size / 2 - 1] + sorted[size / 2]) / 2.0f;
        }
    }

    std::deque<float> values_;
    const size_t window_size_;
};

// ============================================
// Структура для хранения статистики CPU
// ============================================
struct CpuStat {
    uint64_t user;      // время пользовательских процессов
    uint64_t nice;      // время процессов с повышенным приоритетом
    uint64_t system;    // время системных вызовов
    uint64_t idle;      // время простоя
    uint64_t iowait;    // время ожидания I/O
    uint64_t irq;       // время аппаратных прерываний
    uint64_t softirq;   // время программных прерываний
    uint64_t total;     // общее время

    // Сохраненные значения для расчета разности
    uint64_t user_sav;
    uint64_t nice_sav;
    uint64_t system_sav;
    uint64_t idle_sav;
    uint64_t iowait_sav;
    uint64_t irq_sav;
    uint64_t softirq_sav;
    uint64_t total_sav;

    // Медианный фильтр для idle
    RollingMedian<5> idle_median;

    CpuStat() {
        memset(this, 0, sizeof(*this));
    }
};

// ============================================
// Структура результата
// ============================================
struct CpuUsage {
    float user;         // % загрузки пользовательскими процессами
    float nice;         // % загрузки процессами с повышенным приоритетом
    float system;       // % загрузки системой
    float idle;         // % простоя (сглаженный медианой)
    float iowait;       // % ожидания I/O
    float irq;          // % аппаратных прерываний
    float softirq;      // % программных прерываний
    float total;        // % общей загрузки (100 - idle)
    unsigned int uptime; // время работы системы в секундах
};

// ============================================
// Основной класс для мониторинга CPU
// ============================================
class CpuMonitor {
public:
    CpuMonitor() {
        num_cpus_ = sysconf(_SC_NPROCESSORS_ONLN);
        if (num_cpus_ < 1) {
            num_cpus_ = 1;
        }
        
        // Выделяем память для всех CPU + общий (индекс 0)
        cpu_stats_ = new CpuStat[num_cpus_ + 1];
        
        // Инициализируем первый раз (чтобы были базовые значения)
        update();
    }

    ~CpuMonitor() {
        delete[] cpu_stats_;
    }

    // Обновить статистику (читать /proc/stat)
    bool update() {
        FILE* fp = fopen("/proc/stat", "r");
        if (!fp) {
            return false;
        }

        char line[256];
        int cpu_index = 0;

        // Читаем строки из /proc/stat
        while (fgets(line, sizeof(line), fp)) {
            // Пропускаем строки, не начинающиеся с "cpu"
            if (strncmp(line, "cpu", 3) != 0) {
                continue;
            }

            // Парсим строку
            CpuStat* stat = &cpu_stats_[cpu_index];
            
            if (cpu_index == 0) {
                // Общий CPU: "cpu  ..."
                sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu",
                       &stat->user, &stat->nice, &stat->system,
                       &stat->idle, &stat->iowait, &stat->irq, &stat->softirq);
            } else {
                // Отдельное ядро: "cpu0 ...", "cpu1 ..."
                unsigned int id;
                sscanf(line, "cpu%u %lu %lu %lu %lu %lu %lu %lu",
                       &id, &stat->user, &stat->nice, &stat->system,
                       &stat->idle, &stat->iowait, &stat->irq, &stat->softirq);
            }

            // Вычисляем общее время
            stat->total = stat->user + stat->nice + stat->system + 
                          stat->idle + stat->iowait + stat->irq + stat->softirq;

            cpu_index++;
            
            // Если прочитали все CPU, выходим
            if (cpu_index > num_cpus_) {
                break;
            }
        }

        fclose(fp);
        return true;
    }

    // Получить загрузку CPU в процентах
    // cpu_index: 0 - общий CPU, 1 - первое ядро, и т.д.
    bool get_usage(int cpu_index, CpuUsage& usage) {
        if (cpu_index < 0 || cpu_index > num_cpus_) {
            return false;
        }

        // Сначала обновляем данные
        if (!update()) {
            return false;
        }

        CpuStat* stat = &cpu_stats_[cpu_index];

        // Вычисляем разности
        uint64_t d_user = stat->user - stat->user_sav;
        uint64_t d_nice = stat->nice - stat->nice_sav;
        uint64_t d_system = stat->system - stat->system_sav;
        uint64_t d_idle = stat->idle - stat->idle_sav;
        uint64_t d_iowait = stat->iowait - stat->iowait_sav;
        uint64_t d_irq = stat->irq - stat->irq_sav;
        uint64_t d_softirq = stat->softirq - stat->softirq_sav;
        uint64_t d_total = stat->total - stat->total_sav;

        // Защита от деления на ноль
        if (d_total < 1) {
            d_total = 1;
        }

        // Вычисляем проценты
        float scale = 100.0f / (float)d_total;
        
        usage.user = (float)d_user * scale;
        usage.nice = (float)d_nice * scale;
        usage.system = (float)d_system * scale;
        usage.iowait = (float)d_iowait * scale;
        usage.irq = (float)d_irq * scale;
        usage.softirq = (float)d_softirq * scale;
        
        // Idle сглаживаем медианой
        float idle_value = (float)d_idle * scale;
        usage.idle = stat->idle_median(idle_value);
        
        // Общая загрузка = 100% - idle
        usage.total = 100.0f - usage.idle;

        // Получаем uptime
        usage.uptime = get_uptime();

        // Сохраняем текущие значения для следующего расчета
        stat->user_sav = stat->user;
        stat->nice_sav = stat->nice;
        stat->system_sav = stat->system;
        stat->idle_sav = stat->idle;
        stat->iowait_sav = stat->iowait;
        stat->irq_sav = stat->irq;
        stat->softirq_sav = stat->softirq;
        stat->total_sav = stat->total;

        return true;
    }

    // Получить загрузку общего CPU (всех ядер)
    bool get_total_usage(CpuUsage& usage) {
        return get_usage(0, usage);
    }

    // Получить загрузку конкретного ядра
    bool get_cpu_usage(int cpu_index, CpuUsage& usage) {
        if (cpu_index < 1 || cpu_index > num_cpus_) {
            return false;
        }
        return get_usage(cpu_index, usage);
    }

    // Получить количество ядер
    int get_cpu_count() const {
        return num_cpus_;
    }

private:
    // Получить время работы системы
    unsigned int get_uptime() {
        FILE* fp = fopen("/proc/uptime", "r");
        if (!fp) {
            return 0;
        }

        double uptime;
        int n = fscanf(fp, "%lf", &uptime);
        fclose(fp);
        
        if (n != 1) {
            return 0;
        }
        
        return (unsigned int)uptime;
    }

    int num_cpus_;          // количество ядер
    CpuStat* cpu_stats_;    // массив статистики [0] - общий, [1..N] - ядра
};
