#pragma once
#include <atomic>
#include <type_traits>

namespace casket
{

template <typename T>
class SequenceLock
{
public:
    static_assert(std::is_nothrow_copy_assignable<T>::value, "T must satisfy is_nothrow_copy_assignable");
    static_assert(std::is_trivially_copy_assignable<T>::value, "T must satisfy is_trivially_copy_assignable");

    SequenceLock()
        : seq_(0)
    {
    }

    T load() const noexcept
    {
        T copy;
        std::size_t seq0, seq1;
        do
        {
            seq0 = seq_.load(std::memory_order_acquire);
            std::atomic_signal_fence(std::memory_order_acq_rel);
            copy = value_;
            std::atomic_signal_fence(std::memory_order_acq_rel);
            seq1 = seq_.load(std::memory_order_acquire);
        } while (seq0 != seq1 || seq0 & 1);
        return copy;
    }

    void store(const T& desired) noexcept
    {
        std::size_t seq0 = seq_.load(std::memory_order_relaxed);
        seq_.store(seq0 + 1, std::memory_order_release);
        std::atomic_signal_fence(std::memory_order_acq_rel);
        value_ = desired;
        std::atomic_signal_fence(std::memory_order_acq_rel);
        seq_.store(seq0 + 2, std::memory_order_release);
    }

private:
    static const std::size_t kFalseSharingRange = 128;

    // Align to prevent false sharing with adjecent data
    alignas(kFalseSharingRange) T value_;
    std::atomic<std::size_t> seq_;
    // Padding to prevent false sharing with adjecent data
    char padding_[kFalseSharingRange - ((sizeof(value_) + sizeof(seq_)) % kFalseSharingRange)];
    static_assert(((sizeof(value_) + sizeof(seq_) + sizeof(padding_)) % kFalseSharingRange) == 0,
                  "sizeof(Seqlock<T>) should be a multiple of kFalseSharingRange");
};

} // namespace casket