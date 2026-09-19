#pragma once

#include <casket/utils/noncopyable.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/time.h>

namespace casket
{

/**
 * @brief Structure containing process statistics
 */
struct ProcessStatData
{
    float cpuUsagePercent = 0.0f;
    unsigned long rssKb = 0;
    unsigned long vssKb = 0;
    char state = 'N';
};

/**
 * @brief Class for monitoring CPU and memory usage of a specific process
 */
class ProcessStat : public NonCopyable
{
public:
    // ===== Constructors =====

    ProcessStat()
    {
        resetMeasurement();
        //if (init() != 0)
        {
            // Log initialization failure if needed
        }
    }

    ~ProcessStat() = default;

    // ===== Move operations =====

    /**
     * @brief Move constructor
     * Transfers ownership of resources
     */
    ProcessStat(ProcessStat&& other) noexcept
        : pid_(other.pid_)
        , prev_(other.prev_)
        , hz_(other.hz_)
        , bootTime_(other.bootTime_)
    {

        // Reset source object to valid but empty state
        other.pid_ = -1;
        other.prev_ = PreviousMeasurement{};
        other.hz_ = 100;
        other.bootTime_ = 0;
    }

    /**
     * @brief Move assignment operator
     * Transfers ownership of resources
     */
    ProcessStat& operator=(ProcessStat&& other) noexcept
    {
        if (this != &other)
        {
            // Release current resources (none in this case, but good practice)
            // Transfer resources
            pid_ = other.pid_;
            prev_ = other.prev_;
            hz_ = other.hz_;
            bootTime_ = other.bootTime_;

            // Reset source
            other.pid_ = -1;
            other.prev_ = PreviousMeasurement{};
            other.hz_ = 100;
            other.bootTime_ = 0;
        }
        return *this;
    }

    // ===== Public interface =====

    void setPid(pid_t pid)
    {
        pid_ = pid;
        resetMeasurement();
    }

    pid_t getPid() const noexcept
    {
        return pid_;
    }

    int getStat(ProcessStatData& stat) noexcept
    {
        if (pid_ == -1)
        {
            return -1;
        }
        return getUsage(stat);
    }

private:
    struct PreviousMeasurement
    {
        timespec timestamp{};
        unsigned long long utime = 0;
        unsigned long long stime = 0;
    };

    pid_t pid_ = -1;
    PreviousMeasurement prev_;
    unsigned int hz_ = 100;
    time_t bootTime_ = 0;

    // ===== Private methods =====

    [[nodiscard]] int init() noexcept
    {
        std::ifstream uptimeFile("/proc/uptime");
        if (!uptimeFile.is_open())
        {
            return -1;
        }

        double uptimeSeconds = 0.0;
        uptimeFile >> uptimeSeconds;

        if (uptimeFile.fail() || uptimeSeconds < 0)
        {
            return -1;
        }

        bootTime_ = std::time(nullptr) - static_cast<time_t>(uptimeSeconds);
        hz_ = sysconf(_SC_CLK_TCK);

        if (hz_ < 100)
        {
            hz_ = 100;
        }

        return 0;
    }

    void resetMeasurement() noexcept
    {
        prev_ = PreviousMeasurement{};
    }

    [[nodiscard]] float calculateCpuUsage(unsigned long long startTime, unsigned long long utime,
                                          unsigned long long stime) noexcept
    {

        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);

        float cpuPercent = 0.0f;

        if (prev_.timestamp.tv_sec == 0)
        {
            long secondsSinceBoot = currentTime.tv_sec + 1 - bootTime_;
            if (secondsSinceBoot <= 0)
            {
                secondsSinceBoot = 1;
            }

            const unsigned long long usedJiffies = utime + stime;
            long long availableJiffies = secondsSinceBoot * hz_ - startTime;

            if (availableJiffies <= 0)
            {
                availableJiffies = 1;
            }

            cpuPercent = (static_cast<float>(usedJiffies) * 100.0f) / static_cast<float>(availableJiffies);
        }
        else
        {
            const unsigned long long deltaJiffies = (utime + stime) - (prev_.utime + prev_.stime);

            const float deltaSeconds = (currentTime.tv_sec - prev_.timestamp.tv_sec) +
                                       std::abs(currentTime.tv_nsec - prev_.timestamp.tv_nsec) / 1000000000.0f;

            const float scale = 100.0f / (static_cast<float>(hz_) * deltaSeconds);
            cpuPercent = static_cast<float>(deltaJiffies) * scale;
        }

        /*const long numCpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (numCpus > 0)
        {
            cpuPercent /= static_cast<float>(numCpus);
        }*/

        cpuPercent = std::clamp(cpuPercent, 0.0f, 100.0f);

        prev_.timestamp = currentTime;
        prev_.utime = utime;
        prev_.stime = stime;

        return cpuPercent;
    }

    [[nodiscard]] int getUsage(ProcessStatData& stat) noexcept
    {
        std::string statPath = "/proc/" + std::to_string(pid_) + "/stat";
        std::ifstream statFile(statPath);

        if (!statFile.is_open())
        {
            return -1;
        }

        std::string content;
        if (!std::getline(statFile, content))
        {
            return -1;
        }

        if (content.empty())
        {
            return -1;
        }

        // Находим конец имени процесса (второе поле в скобках)
        size_t nameEnd = content.find(')');
        if (nameEnd == std::string::npos)
        {
            return -1;
        }

        const char* fields = content.c_str() + nameEnd + 2; // Пропускаем ") "

        unsigned long rssPages = 0;
        unsigned long vssBytes = 0;
        unsigned long long utime = 0;
        unsigned long long stime = 0;
        unsigned long long startTime = 0;
        char state = 'N';

        // Четко указываем все поля, которые нужно пропустить
        // Формат: state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt
        //         utime stime cutime cstime priority nice num_threads it_real_value start_time
        //         vsize rss
        const int parsedFields = std::sscanf(fields,
                                             "%c "   // 3: state
                                             "%*d "  // 4: ppid
                                             "%*d "  // 5: pgrp
                                             "%*d "  // 6: session
                                             "%*d "  // 7: tty_nr
                                             "%*d "  // 8: tpgid
                                             "%*u "  // 9: flags
                                             "%*u "  // 10: minflt
                                             "%*u "  // 11: cminflt
                                             "%*u "  // 12: majflt
                                             "%*u "  // 13: cmajflt
                                             "%llu " // 14: utime
                                             "%llu " // 15: stime
                                             "%*u "  // 16: cutime
                                             "%*u "  // 17: cstime
                                             "%*d "  // 18: priority
                                             "%*d "  // 19: nice
                                             "%*d "  // 20: num_threads
                                             "%*d "  // 21: it_real_value
                                             "%llu " // 22: start_time
                                             "%lu "  // 23: vsize
                                             "%lu",  // 24: rss
                                             &state,
                                             &utime,
                                             &stime,
                                             &startTime,
                                             &vssBytes,
                                             &rssPages);

        if (parsedFields < 6)
        {
            return -1;
        }

        const int pageSizeKb = getpagesize() / 1024;

        if (rssPages > (std::numeric_limits<unsigned long>::max() / static_cast<unsigned long>(pageSizeKb)))
        {
            return -1;
        }

        const float cpuUsage = calculateCpuUsage(startTime, utime, stime);

        stat.cpuUsagePercent = cpuUsage;
        stat.state = state;
        stat.vssKb = vssBytes / 1024;
        stat.rssKb = rssPages * pageSizeKb;

        return 0;
    }
};

} // namespace casket