#pragma once
#include <atomic>
#include <array>
#include <cassert>
#include <cstdint>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace casket
{

class RCU final
{
public:
    using Epoch = uint64_t;
    using Token = uint64_t;

    static constexpr uint32_t kMaxThreads = 256;
    static constexpr uint64_t kCntThrOffline = UINT64_MAX;
    static constexpr uint64_t kCntMax = UINT64_MAX - 1;
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

    void threadRegister(uint32_t threadId)
    {
        if (threadId >= kMaxThreads)
            throw std::out_of_range("RCU: thread_id out of range");

        uint32_t idx = threadId >> 6; // / 64
        uint32_t bit = threadId & 63; // % 64
        uint64_t oldBmap = regBitmap_[idx].load(std::memory_order_relaxed);

        if (oldBmap & (1ULL << bit))
            return;

        uint64_t newBmap;
        do
        {
            newBmap = oldBmap | (1ULL << bit);
        } while (!regBitmap_[idx].compare_exchange_weak(
            oldBmap, newBmap, std::memory_order_release, std::memory_order_relaxed));

        numThreads_.fetch_add(1, std::memory_order_relaxed);
    }

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
            oldBmap, newBmap, std::memory_order_release, std::memory_order_relaxed));

        numThreads_.fetch_sub(1, std::memory_order_relaxed);
    }

    void threadOnline(uint32_t threadId) noexcept
    {
        assert(threadId < kMaxThreads && "threadOnline: invalid thread id");

        const uint64_t cur = qsbrCnt_[threadId].load(std::memory_order_relaxed);
        assert((cur == 0 || cur == kCntThrOffline) &&
               "threadOnline called while thread is already online and holds a snapshot");

        Token t = token_.load(std::memory_order_acquire);
        qsbrCnt_[threadId].store(t, std::memory_order_release);
    }

    void threadOffline(uint32_t threadId) noexcept
    {
        qsbrCnt_[threadId].store(kCntThrOffline, std::memory_order_release);
    }

    void quiescent(uint32_t threadId) noexcept
    {
        Token t = token_.load(std::memory_order_acquire);
        if (t != qsbrCnt_[threadId].load(std::memory_order_relaxed))
        {
            qsbrCnt_[threadId].store(t, std::memory_order_release);
        }
    }

    Token startGracePeriod() noexcept
    {
        return token_.fetch_add(1, std::memory_order_release) + 1;
    }

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
                            // still waiting
                        }
                        else
                        {
                            goto retry_outer;
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

        retry_outer:
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

    static constexpr uint32_t kInvalidThread = UINT32_MAX;

    Epoch getEpoch() const noexcept
    {
        return token_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint32_t kBitmapWords = (kMaxThreads + 63) / 64;

    alignas(64) std::atomic<Token> token_;
    alignas(64) std::atomic<uint32_t> numThreads_;
    alignas(64) std::atomic<uint64_t> ackedToken_;
    alignas(64) std::array<std::atomic<uint64_t>, kMaxThreads> qsbrCnt_;
    alignas(64) std::array<std::atomic<uint64_t>, kBitmapWords> regBitmap_;
};

} // namespace casket