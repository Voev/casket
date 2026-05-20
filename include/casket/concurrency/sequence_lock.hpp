#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <thread>

namespace casket
{

constexpr size_t CACHE_LINE_SIZE = 64;

#ifdef __x86_64__
#define CPU_PAUSE() __builtin_ia32_pause()
#elif defined(__arm__) || defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
#define CPU_PAUSE() std::this_thread::yield()
#endif

template <typename T>
class SequenceLock final
{
private:
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    alignas(CACHE_LINE_SIZE) std::atomic<uint32_t> seq_{0};
    alignas(CACHE_LINE_SIZE) T value_;

public:
    SequenceLock() = default;
    ~SequenceLock() = default;

    SequenceLock(const SequenceLock&) = delete;
    SequenceLock& operator=(const SequenceLock&) = delete;

    void store(const T& value) noexcept
    {
        auto seq = seq_.load(std::memory_order_acquire);
        seq_.store(seq + 1, std::memory_order_release);

        std::atomic_thread_fence(std::memory_order_release);
        value_ = value;
        std::atomic_thread_fence(std::memory_order_release);

        seq_.store(seq + 2, std::memory_order_release);
    }

    void load(T& value) const noexcept
    {
        uint32_t seq;
        uint32_t spins = 0;

        do
        {
            seq = seq_.load(std::memory_order_acquire);

            if (seq & 1)
            {
                auto min = 1u << std::min(10u, spins);
                for (uint32_t i = 0; i < min; ++i)
                {
                    CPU_PAUSE();
                }
                if (spins < 10u)
                {
                    spins++;
                }
                continue;
            }

            spins = 0;

            std::atomic_thread_fence(std::memory_order_acquire);
            value = value_;
            std::atomic_thread_fence(std::memory_order_acquire);

        } while (seq != seq_.load(std::memory_order_acquire));
    }
};

} // namespace casket