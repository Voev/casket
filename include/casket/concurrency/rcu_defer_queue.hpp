#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <utility>

#include <casket/concurrency/rcu.hpp>

namespace casket
{

/// @brief Deferred reclamation queue for an @ref RCU instance.
///
/// Decouples retirement from reclamation: retired objects are queued with the
/// token captured at retirement time and freed later, once @ref RCU reports
/// that the corresponding grace period has elapsed.
///
/// Thread safety: not thread-safe. Use one queue per writer thread, or guard
/// @ref enqueue / @ref reclaim with an external mutex.
///
/// @tparam T  Type of reclaimed objects; the queue takes ownership of raw
///            pointers passed to @ref enqueue.
template <typename T>
class RCUDeferQueue final
{
public:
    /// Callable invoked to free a single retired object.
    using Deleter = std::function<void(T*)>;

    /// @brief Construct a defer queue bound to @p rcu.
    /// @param[in] rcu           RCU instance tracking reader quiescent states.
    /// @param[in] triggerLimit  Queue size that triggers inline reclamation
    ///                          from @ref enqueue. Values < 1 are clamped to 1.
    /// @param[in] maxReclaim    Upper bound on objects freed per reclaim pass.
    ///                          Values < 1 are clamped to 1.
    /// @param[in] deleter       Callable that frees a single object.
    RCUDeferQueue(RCU& rcu, std::size_t triggerLimit, std::size_t maxReclaim, Deleter deleter)
        : rcu_(rcu)
        , triggerLimit_(triggerLimit)
        , maxReclaim_(maxReclaim)
        , deleter_(std::move(deleter))
    {
        if (triggerLimit_ == 0)
            triggerLimit_ = 1;
        if (maxReclaim_ == 0)
            maxReclaim_ = 1;
    }

    /// @brief Drain the queue, then destroy it.
    ~RCUDeferQueue() noexcept
    {
        try
        {
            flush();
        }
        catch (...)
        {
            // Destructors must not throw; remaining objects stay leaked
            // rather than terminating the program.
        }
    }

    RCUDeferQueue(const RCUDeferQueue&) = delete;
    RCUDeferQueue& operator=(const RCUDeferQueue&) = delete;
    RCUDeferQueue(RCUDeferQueue&&) = delete;
    RCUDeferQueue& operator=(RCUDeferQueue&&) = delete;

    /// @brief Retire @p ptr for deferred reclamation. Non-blocking except
    ///        for an inline @ref reclaim when the queue exceeds the trigger.
    /// @param[in] ptr  Object to free later; @c nullptr is ignored.
    void enqueue(T* ptr)
    {
        if (ptr == nullptr)
            return;

        const RCU::Token token = rcu_.startGracePeriod();
        queue_.push_back(Entry{token, ptr});

        if (queue_.size() >= triggerLimit_)
            reclaim();
    }

    /// @brief Attempt to free objects whose grace period has elapsed.
    ///        Non-blocking; frees at most @ref maxReclaim entries from the
    ///        front and stops at the first not-yet-safe token.
    /// @return Number of objects freed in this call.
    std::size_t reclaim() noexcept
    {
        std::size_t reclaimed = 0;
        while (!queue_.empty() && reclaimed < maxReclaim_)
        {
            const Entry& front = queue_.front();

            if (!rcu_.checkGracePeriod(front.token, false))
                break;

            T* ptr = front.ptr;
            queue_.pop_front();
            deleter_(ptr);
            ++reclaimed;
        }
        return reclaimed;
    }

    /// @brief Block until every queued object has been freed.
    void flush()
    {
        while (!queue_.empty())
        {
            const Entry& front = queue_.front();
            rcu_.checkGracePeriod(front.token, true);

            T* ptr = front.ptr;
            queue_.pop_front();
            deleter_(ptr);
        }
    }

    /// @brief Number of objects awaiting reclamation.
    std::size_t size() const noexcept
    {
        return queue_.size();
    }

    /// @brief @c true if no objects are awaiting reclamation.
    bool empty() const noexcept
    {
        return queue_.empty();
    }

private:
    /// A single retired object together with the token captured at retirement.
    struct Entry
    {
        RCU::Token token; ///< Grace-period token captured at retirement.
        T* ptr;           ///< Retired object to free once the token is safe.
    };

    RCU& rcu_;                 ///< Owning RCU instance.
    std::deque<Entry> queue_;  ///< FIFO of retired objects, ordered by token.
    std::size_t triggerLimit_; ///< Size threshold for inline reclamation.
    std::size_t maxReclaim_;   ///< Max objects freed per reclaim pass.
    Deleter deleter_;          ///< Callable that frees a single object.
};

} // namespace casket