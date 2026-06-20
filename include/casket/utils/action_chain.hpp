#pragma once

#include <functional>
#include <vector>
#include <exception>
#include <utility>
#include <type_traits>

#include <iostream>

namespace casket
{

/// @brief Chain of actions with automatic rollback on error.
///
/// Allows combining multiple operations into a transaction with a guarantee
/// that if an exception occurs, all previous operations will be rolled back
/// in reverse order.
///
/// @example
/// ActionChain chain;
/// Object* obj = nullptr;
///
/// chain.addAction(
///     [&]() { obj = new Object(); },
///     [&]() { delete obj; }
/// );
///
/// chain.addAction(
///     [&]() { writeToFile(obj); },
///     [&]() { removeFile("data.dat"); }
/// );
///
/// chain.execute();
class ActionChain
{
public:
    using Action = std::function<void()>;
    using Rollback = std::function<void()>;

    /// @brief Structure for storing a pair of "action - rollback".
    struct ActionPair
    {
        Action forward;
        Rollback rollback;
        bool executed = false;

        ActionPair(Action fwd, Rollback rlb)
            : forward(std::move(fwd))
            , rollback(std::move(rlb))
        {
        }

        ActionPair(ActionPair&& other) noexcept
            : forward(std::move(other.forward))
            , rollback(std::move(other.rollback))
            , executed(other.executed)
        {
        }

        ActionPair& operator=(ActionPair&& other) noexcept
        {
            if (this != &other)
            {
                forward = std::move(other.forward);
                rollback = std::move(other.rollback);
                executed = other.executed;
            }
            return *this;
        }

        // Copying is prohibited
        ActionPair(const ActionPair&) = delete;
        ActionPair& operator=(const ActionPair&) = delete;
    };

    ActionChain() = default;

    ~ActionChain()
    {
        if (!committed_)
            rollbackAll();
    }

    // Copying is prohibited
    ActionChain(const ActionChain&) = delete;
    ActionChain& operator=(const ActionChain&) = delete;

    // Moving is allowed
    ActionChain(ActionChain&& other) noexcept
        : actions_(std::move(other.actions_))
        , committed_(other.committed_)
    {
        other.committed_ = false;
    }

    ActionChain& operator=(ActionChain&& other) noexcept
    {
        if (this != &other)
        {
            rollbackAll();
            actions_ = std::move(other.actions_);
            committed_ = other.committed_;
            other.committed_ = false;
        }
        return *this;
    }

    /// @brief Add an action with a rollback function.
    ///
    /// @tparam Fwd Type of the forward action function.
    /// @tparam Rlb Type of the rollback function.
    /// @param forward Function that performs the main action.
    /// @param rollback Function that rolls back the action on error.
    ///
    /// @return ActionChain& Reference to the current object for chaining.
    template <typename Fwd, typename Rlb>
    ActionChain& addAction(Fwd&& forward, Rlb&& rollback)
    {
        static_assert(std::is_invocable_v<Fwd>, "forward must be callable with no arguments");
        static_assert(std::is_invocable_v<Rlb>, "rollback must be callable with no arguments");

        actions_.emplace_back(std::forward<Fwd>(forward), std::forward<Rlb>(rollback));
        return *this;
    }

    /// @brief Add an action with forward only (no rollback)
    ///
    /// @tparam Fwd Type of the forward action function
    ///
    /// @param forward Function that performs the main action.
    ///
    /// @return ActionChain& Reference to the current object for chaining.
    template <typename Fwd>
    ActionChain& addAction(Fwd&& forward)
    {
        static_assert(std::is_invocable_v<Fwd>, "forward must be callable with no arguments");

        actions_.emplace_back(std::forward<Fwd>(forward),
                              []()
                              {
                              } // Empty rollback
        );
        return *this;
    }

    /// @brief Execute all added actions.
    ///
    /// Actions are executed sequentially. If any action throws an exception,
    /// all previously executed actions are rolled back in reverse order.
    ///
    /// @throw std::exception Any exception from actions or rollbacks.
    void execute()
    {
        if (committed_)
        {
            return; // Already executed
        }

        committed_ = true; // Temporary commit, will be undone on error

        try
        {
            for (size_t i = 0; i < actions_.size(); ++i)
            {
                try
                {
                    actions_[i].forward();
                    actions_[i].executed = true;
                }
                catch (...)
                {
                    // Roll back all previously executed actions
                    rollbackExecuted(i);
                    committed_ = false;
                    throw; // Rethrow the original exception
                }
            }
        }
        catch (...)
        {
            committed_ = false;
            throw;
        }
    }

    /// @brief Force commit the chain (disable automatic rollback).
    ///
    /// Use after successful execution to prevent rollback in the destructor.
    void commit() noexcept
    {
        committed_ = true;
    }

    /// @brief Roll back all executed actions
    ///
    /// Can be called manually for explicit rollback.
    void rollback() noexcept
    {
        rollbackAll();
        committed_ = false;
    }

    /// @brief Check if the chain is committed.
    bool isCommitted() const noexcept
    {
        return committed_;
    }

    /// @brief Get the number of actions in the chain.
    size_t size() const noexcept
    {
        return actions_.size();
    }

    /// @brief Clear all actions.
    void clear() noexcept
    {
        rollbackAll();
        actions_.clear();
        committed_ = false;
    }

private:
    std::vector<ActionPair> actions_;
    bool committed_ = false;

    /// @brief Roll back all executed actions in reverse order.
    void rollbackAll() noexcept
    {
        if (!actions_.empty())
        {
            rollbackExecuted(actions_.size());
        }
    }

    /// @brief Roll back executed actions up to the specified index.
    ///
    /// @param untilIndex Index up to which to roll back (exclusive).
    ///
    void rollbackExecuted(size_t untilIndex) noexcept
    {
        // Go in reverse order
        for (size_t i = untilIndex; i > 0; --i)
        {
            size_t idx = i - 1;
            if (actions_[idx].executed)
            {
                try
                {
                    actions_[idx].rollback();
                    actions_[idx].executed = false;
                }
                catch (...)
                {
                    // Log rollback error but continue
                    // Can use your own logger if available
                }
            }
        }
    }
};

} // namespace casket