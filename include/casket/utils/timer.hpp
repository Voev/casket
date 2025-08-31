#pragma once

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <casket/nonstd/string_view.hpp>

namespace casket
{

class Timer final
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    Timer() = default;

    ~Timer() noexcept = default;

    inline void start() noexcept
    {
        assert(started_ == false);
        started_ = true;
        start_ = Clock::now();
    }

    inline void stop() noexcept
    {
        assert(started_ == true);
        started_ = false;
        finish_ = Clock::now();
    }

    inline auto elapsedMilliSecs() const noexcept
    {
        assert(started_ == false);
        auto elapsedTime = finish_ - start_;
        auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime);
        return msec.count();
    }

    inline auto elapsedNanoSecs() const noexcept
    {
        assert(started_ == false);
        auto elapsedTime = finish_ - start_;
        auto castedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime);
        return castedTime.count();
    }

private:
    TimePoint start_;
    TimePoint finish_;
    bool started_{false};
};

template <typename T, typename... Args>
inline void Measure(nonstd::string_view prefix, T callable, Args&&... args)
{
    Timer t;
    t.start();
    std::invoke(callable, std::forward<Args>(args)...);
    t.stop();
    std::cout << prefix << ": " << t.elapsedMilliSecs() << " ms" << std::endl;
}

} // namespace casket

