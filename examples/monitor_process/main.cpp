#include <iostream>
#include <iomanip>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <ctime>

#include <casket/stats/cpu.hpp>

using namespace casket;

/**
 * @brief Класс для сглаживания CPU значений
 */
class CpuSmoother {
private:
    std::deque<float> history_;
    unsigned int windowSize_ = 5;
    float smoothedValue_ = 0.0f;
    float alpha_ = 0.3f;
    bool useExponential_ = false;
    bool isInitialized_ = false;
    
public:
    CpuSmoother(int windowSize = 5, float alpha = 0.3f, bool exponential = false)
        : windowSize_(windowSize), alpha_(alpha), useExponential_(exponential) {}
    
    /**
     * @brief Применить сглаживание к значению
     */
    float smooth(float current) {
        if (useExponential_) {
            return exponentialSmooth(current);
        } else {
            return movingAverageSmooth(current);
        }
    }
    
    /**
     * @brief Скользящее среднее (Moving Average)
     */
    float movingAverageSmooth(float current) {
        history_.push_back(current);
        if (history_.size() > windowSize_) {
            history_.pop_front();
        }
        
        float sum = 0.0f;
        for (float v : history_) {
            sum += v;
        }
        return sum / history_.size();
    }
    
    /**
     * @brief Экспоненциальное сглаживание (Exponential Smoothing)
     */
    float exponentialSmooth(float current) {
        if (!isInitialized_) {
            smoothedValue_ = current;
            isInitialized_ = true;
        } else {
            smoothedValue_ = alpha_ * current + (1.0f - alpha_) * smoothedValue_;
        }
        return smoothedValue_;
    }
    
    /**
     * @brief Сбросить историю
     */
    void reset() {
        history_.clear();
        smoothedValue_ = 0.0f;
        isInitialized_ = false;
    }
    
    /**
     * @brief Получить последнее сглаженное значение
     */
    float getSmoothed() const {
        if (useExponential_) {
            return smoothedValue_;
        } else {
            if (history_.empty()) return 0.0f;
            float sum = 0.0f;
            for (float v : history_) {
                sum += v;
            }
            return sum / history_.size();
        }
    }
    
    /**
     * @brief Получить размер истории
     */
    size_t getHistorySize() const {
        return history_.size();
    }
};

/**
 * @brief Расширенный класс мониторинга со сглаживанием
 */
class ProcessStatExtended : public ProcessStat {
private:
    CpuSmoother smoother_;
    bool enableSmoothing_ = false;
    
public:
    ProcessStatExtended() : ProcessStat() {}
    
    /**
     * @brief Включить сглаживание
     */
    void enableSmoothing(int windowSize = 5, bool exponential = false, float alpha = 0.3f) {
        enableSmoothing_ = true;
        smoother_ = CpuSmoother(windowSize, alpha, exponential);
    }
    
    /**
     * @brief Получить статистику со сглаживанием
     */
    int getStatSmoothed(ProcessStatData& stat, ProcessStatData& stat2) {
        int result = ProcessStat::getStat(stat);
        if (result == 0 && enableSmoothing_) {
            stat.cpuUsagePercent = smoother_.smooth(stat2.cpuUsagePercent);
        }
        return result;
    }
    
    /**
     * @brief Получить сырые данные (без сглаживания)
     */
    int getStatRaw(ProcessStatData& stat) {
        return ProcessStat::getStat(stat);
    }
    
    /**
     * @brief Сбросить сглаживание
     */
    void resetSmoothing() {
        smoother_.reset();
    }
};

/**
 * @brief Получить имя процесса по PID
 */
std::string getProcessName(pid_t pid) {
    std::string commPath = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream commFile(commPath);
    if (!commFile.is_open()) return "Unknown";
    std::string name;
    std::getline(commFile, name);
    if (!name.empty() && name.back() == '\n') {
        name.pop_back();
    }
    return name;
}

/**
 * @brief Мониторинг с опциональным сглаживанием
 */
void monitorProcess(pid_t pid, bool enableSmoothing = false, int windowSize = 3, 
                    bool exponential = false, float alpha = 0.3f, int intervalSeconds = 1) {
    
    ProcessStatExtended processStat;
    processStat.setPid(pid);
    
    std::string processName = getProcessName(pid);
    
    std::cout << "=== CPU Monitor ===" << std::endl;
    std::cout << "PID: " << pid << std::endl;
    std::cout << "Process: " << processName << std::endl;
    
    if (enableSmoothing) {
        processStat.enableSmoothing(windowSize, exponential, alpha);
        std::cout << "Smoothing: " << (exponential ? "Exponential (α=" : "Moving Average (window=") 
                  << (exponential ? std::to_string(alpha) : std::to_string(windowSize)) << ")"
                  << std::endl;
    } else {
        std::cout << "Smoothing: OFF (raw data)" << std::endl;
    }
    
    std::cout << "Press Ctrl+C to stop" << std::endl << std::endl;
    
    ProcessStatData rawStats;
    ProcessStatData smoothStats;
    
    // Заголовок
    std::cout << std::setw(8) << "Time"
              << std::setw(15) << "Process"
              << std::setw(12) << "CPU (raw)"
              << std::setw(12) << "CPU (smooth)"
              << std::setw(12) << "RSS (KB)"
              << std::setw(12) << "VSS (KB)"
              << std::setw(8) << "State"
              << std::endl;
    std::cout << std::string(79, '-') << std::endl;
    
    // Первый замер для инициализации
    processStat.getStatRaw(rawStats);
    if (enableSmoothing) {
        processStat.getStatSmoothed(smoothStats, rawStats);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int iteration = 0;
    
    while (true) {
        // Получаем сырые данные
        if (processStat.getStatRaw(rawStats) != 0) {
            std::cerr << "\nProcess " << pid << " terminated!" << std::endl;
            break;
        }
        
        // Получаем сглаженные данные (если включено)
        float smoothCpu = 0.0f;
        if (enableSmoothing) {
            if (processStat.getStatSmoothed(smoothStats, rawStats) == 0) {
                smoothCpu = smoothStats.cpuUsagePercent;
            }
        }
        
        // Вывод
        std::cout << std::setw(8) << (iteration * intervalSeconds)
                  << std::setw(15) << processName
                  << std::setw(12) << std::fixed << std::setprecision(1) 
                  << rawStats.cpuUsagePercent
                  << std::setw(12) << std::fixed << std::setprecision(1) 
                  << smoothCpu
                  << std::setw(12) << rawStats.rssKb
                  << std::setw(12) << rawStats.vssKb
                  << std::setw(8) << rawStats.state
                  << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        ++iteration;
    }
}

/**
 * @brief Вывод справки
 */
void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " <PID> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --smooth N      Enable moving average smoothing (window size N)" << std::endl;
    std::cout << "  --exp ALPHA     Enable exponential smoothing (alpha=ALPHA)" << std::endl;
    std::cout << "  --interval N    Update interval in seconds (default: 1)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " 1234" << std::endl;
    std::cout << "  " << programName << " 1234 --smooth 5" << std::endl;
    std::cout << "  " << programName << " 1234 --exp 0.3" << std::endl;
    std::cout << "  " << programName << " 1234 --smooth 3 --interval 2" << std::endl;
}

int main(int argc, char* argv[]) {
    pid_t pid;
    int interval = 1;
    bool enableSmoothing = false;
    int windowSize = 5;
    bool exponential = false;
    float alpha = 0.3f;
    
    // Парсинг аргументов
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }
    
    pid = std::atoi(argv[1]);
    if (pid <= 0) {
        std::cerr << "Invalid PID: " << argv[1] << std::endl;
        return 1;
    }
    
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--smooth") == 0 && i + 1 < argc) {
            enableSmoothing = true;
            exponential = false;
            windowSize = std::atoi(argv[++i]);
            if (windowSize < 1) windowSize = 1;
        } else if (strcmp(argv[i], "--exp") == 0 && i + 1 < argc) {
            enableSmoothing = true;
            exponential = true;
            alpha = std::atof(argv[++i]);
            if (alpha <= 0.0f || alpha > 1.0f) alpha = 0.3f;
        } else if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            interval = std::atoi(argv[++i]);
            if (interval < 1) interval = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printHelp(argv[0]);
            return 1;
        }
    }
    
    // Запускаем мониторинг
    monitorProcess(pid, enableSmoothing, windowSize, exponential, alpha, interval);
    
    return 0;
}

/*
// ============================================
// Пример использования
// ============================================
#include <iostream>
#include <iomanip>
#include <thread>

int main() {
    CpuMonitor monitor;
    
    std::cout << "Количество ядер CPU: " << monitor.get_cpu_count() << std::endl;
    std::cout << std::endl;
    std::cout << "Мониторинг загрузки CPU (обновление каждую секунду)" << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << std::fixed << std::setprecision(1);

    for (int i = 0; i < 10; ++i) {
        CpuUsage usage;
        
        // Получаем загрузку общего CPU
        if (monitor.get_total_usage(usage)) {
            std::cout << "\nВремя работы: " << usage.uptime << " сек" << std::endl;
            std::cout << "Общая загрузка CPU: " << usage.total << "%" << std::endl;
            std::cout << "  ├─ user:   " << std::setw(6) << usage.user << "%" << std::endl;
            std::cout << "  ├─ nice:   " << std::setw(6) << usage.nice << "%" << std::endl;
            std::cout << "  ├─ system: " << std::setw(6) << usage.system << "%" << std::endl;
            std::cout << "  ├─ idle:   " << std::setw(6) << usage.idle << "%" << std::endl;
            std::cout << "  ├─ iowait: " << std::setw(6) << usage.iowait << "%" << std::endl;
            std::cout << "  ├─ irq:    " << std::setw(6) << usage.irq << "%" << std::endl;
            std::cout << "  └─ softirq:" << std::setw(6) << usage.softirq << "%" << std::endl;
        }

        // Ждем 1 секунду перед следующим замером
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}*/