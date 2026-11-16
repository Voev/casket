#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>

namespace casket
{

/// @brief QSBR-based Read-Copy-Update primitive.
///
/// Readers periodically acknowledge a quiescent state via @ref quiescent();
/// writers wait for all registered readers to acknowledge a token before
/// reclaiming retired data. Based on DPDK's @c rte_rcu_qsbr.
///
/// Contract:
///  - Every reader thread must call @ref threadRegister before use and
///    @ref threadUnregister after the last @ref quiescent.
///  - A reader must call @ref quiescent regularly, or go @ref threadOffline
///    around blocking operations; otherwise writers stall indefinitely.
///  - A thread must not hold an RCU snapshot across @ref threadOffline /
///    @ref threadOnline.
class RCU final
{
public:
    using Epoch = uint64_t;  ///< Monotonic epoch counter (alias of Token).
    using Token = uint64_t;  ///< Grace-period token.

    static constexpr uint32_t kMaxThreads    = 256;             ///< Max registrable reader slots.
    static constexpr uint64_t kCntThrOffline = UINT64_MAX;      ///< Sentinel: thread is offline.
    static constexpr uint64_t kCntMax        = UINT64_MAX - 1;  ///< Sentinel: no acked token yet.
    static constexpr uint32_t kInvalidThread = UINT32_MAX;      ///< Sentinel: no caller thread id.

    /// @brief Construct an RCU instance with no registered threads.
    RCU() noexcept
        : token_(0)
        , numThreads_(0)
        , ackedToken_(kCntMax)
    {
        for (auto& c : qsbrCnt_)
            c.store(0, std::memory_order_relaxed);
        for (auto& b : regBitmap_)
            b.store(0, std::memory_order_relaxed);
    }

    ~RCU() noexcept = default;

    /// @brief Register a reader thread.
    /// @param[in] threadId  Slot id in [0, kMaxThreads).
    /// @throws std::out_of_range if @p threadId is out of range.
    void threadRegister(uint32_t threadId)
    {
        if (threadId >= kMaxThreads)
            throw std::out_of_range("RCU: thread_id out of range");

        uint32_t idx = threadId >> 6;
        uint32_t bit = threadId & 63;
        uint64_t oldBmap = regBitmap_[idx].load(std::memory_order_relaxed);

        if (oldBmap & (1ULL << bit))
            return;

        uint64_t newBmap;
        do
        {
            newBmap = oldBmap | (1ULL << bit);
        } while (!regBitmap_[idx].compare_exchange_weak(
            oldBmap, newBmap,
            std::memory_order_release, std::memory_order_relaxed));

        numThreads_.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Unregister a reader thread.
    /// @param[in] threadId  Slot id; ignored if out of range.
    /// @note Must be called only after @ref threadOffline and after the
    ///       thread has stopped touching RCU-protected data.
    void threadUnregister(uint32_t threadId)
    {
        if (threadId >= kMaxThreads)
            return;

        uint32_t idx = threadId >> 6;
        uint32_t bit = threadId & 63;
        uint64_t oldBmap = regBitmap_[idx].load(std::memory_order_relaxed);

        if (!(oldBmap & (1ULL << bit)))
            return;

        uint64_t newBmap;
        do
        {
            newBmap = oldBmap & ~(1ULL << bit);
        } while (!regBitmap_[idx].compare_exchange_weak(
            oldBmap, newBmap,
            std::memory_order_release, std::memory_order_relaxed));

        numThreads_.fetch_sub(1, std::memory_order_relaxed);
    }

    /// @brief Mark the thread online with the current token.
    /// @param[in] threadId  Slot id.
    /// @note The slot must currently be @c 0 (fresh) or @ref kCntThrOffline.
    void threadOnline(uint32_t threadId) noexcept
    {
        assert(threadId < kMaxThreads && "threadOnline: invalid thread id");

        const uint64_t cur = qsbrCnt_[threadId].load(std::memory_order_relaxed);
        assert((cur == 0 || cur == kCntThrOffline) &&
               "threadOnline called while thread is already online and holds a snapshot");

        Token t = token_.load(std::memory_order_acquire);
        qsbrCnt_[threadId].store(t, std::memory_order_release);
    }

    /// @brief Mark the thread offline; writers will not wait for it.
    /// @param[in] threadId  Slot id.
    void threadOffline(uint32_t threadId) noexcept
    {
        qsbrCnt_[threadId].store(kCntThrOffline, std::memory_order_release);
    }

    /// @brief Acknowledge that all reads started before the current token
    ///        have finished (a quiescent state).
    /// @param[in] threadId  Slot id.
    void quiescent(uint32_t threadId) noexcept
    {
        Token t = token_.load(std::memory_order_acquire);
        if (t != qsbrCnt_[threadId].load(std::memory_order_relaxed))
        {
            qsbrCnt_[threadId].store(t, std::memory_order_release);
        }
    }

    /// @brief Bump the global token and return the new value.
    /// @return New token; all readers must eventually reach >= it.
    Token startGracePeriod() noexcept
    {
        return token_.fetch_add(1, std::memory_order_release) + 1;
    }

    /// @brief Check whether all registered readers have acknowledged @p token.
    /// @param[in] token  Grace-period token from @ref startGracePeriod.
    /// @param[in] wait   If @c true, block until the check passes; otherwise
    ///                   return immediately.
    /// @return @c true if every reader is offline or has acked >= @p token.
    bool checkGracePeriod(Token token, bool wait) noexcept
    {
        int spin = 0;
        while (true)
        {
            uint64_t ackedToken = kCntMax;
            bool allPassed = true;

            for (uint32_t i = 0; i < kBitmapWords; ++i)
            {
                uint64_t bmap = regBitmap_[i].load(std::memory_order_acquire);
                uint32_t baseId = i << 6;

                while (bmap)
                {
                    uint32_t bit = __builtin_ctzll(bmap);
                    uint32_t tid = baseId + bit;

                    uint64_t c = qsbrCnt_[tid].load(std::memory_order_acquire);

                    if (c != kCntThrOffline && c < token)
                    {
                        allPassed = false;
                        if (!wait)
                            return false;
                        bmap = regBitmap_[i].load(std::memory_order_acquire);
                        if (bmap & (1ULL << bit))
                        {
                            // still registered: keep waiting
                        }
                        else
                        {
                            goto retryOuter;
                        }
                    }

                    if (c != kCntThrOffline && ackedToken > c)
                        ackedToken = c;

                    bmap &= ~(1ULL << bit);
                }
            }

            if (allPassed)
            {
                if (ackedToken != kCntMax)
                    ackedToken_.store(ackedToken, std::memory_order_relaxed);
                return true;
            }

        retryOuter:
            if (spin < 64)
            {
                ++spin;
                std::this_thread::yield();
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    }

    /// @brief Perform a full grace period (blocking).
    /// @param[in] callerThreadId  Optional id of the calling thread if it is
    ///                            registered as a reader; it is taken offline
    ///                            for the duration of the wait and restored
    ///                            afterwards. Pass @ref kInvalidThread if the
    ///                            caller is not a registered reader.
    void synchronize(uint32_t callerThreadId = kInvalidThread) noexcept
    {
        const bool wasRegistered = (callerThreadId != kInvalidThread);

        if (wasRegistered)
            threadOffline(callerThreadId);

        Token t = startGracePeriod();
        checkGracePeriod(t, true);

        if (wasRegistered)
            threadOnline(callerThreadId);
    }

    /// @brief Return the current token.
    /// @return Current epoch value.
    Epoch getEpoch() const noexcept
    {
        return token_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint32_t kBitmapWords = (kMaxThreads + 63) / 64; ///< Number of 64-bit words in the registration bitmap.

    alignas(64) std::atomic<Token>    token_;       ///< Global grace-period token.
    alignas(64) std::atomic<uint32_t> numThreads_;  ///< Number of registered threads.
    alignas(64) std::atomic<uint64_t> ackedToken_;  ///< Minimum acked token (best-effort cache).
    alignas(64) std::array<std::atomic<uint64_t>, kMaxThreads>   qsbrCnt_;    ///< Per-thread acked token or kCntThrOffline.
    alignas(64) std::array<std::atomic<uint64_t>, kBitmapWords>  regBitmap_;  ///< Registered-thread bitmap.
};

} // namespace casket