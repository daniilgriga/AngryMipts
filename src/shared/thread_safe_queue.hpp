// ============================================================
// thread_safe_queue.hpp — Minimal mutex-protected FIFO queue.
// Part of: angry::shared
//
// Provides a tiny cross-thread queue abstraction used for:
//   * Command passing into physics runtime
//   * Event draining out of worker-owned systems
//   * Non-blocking pop via std::optional return
//   * Simplicity-first synchronization with one mutex
// ============================================================

#pragma once
#include <mutex>
#include <optional>
#include <queue>

namespace angry
{

template <typename T>
// Thread-safe FIFO queue with non-blocking pop; used for
// cross-thread command/event passing between runtime and physics.
class ThreadSafeQueue
{
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;

public:
    // Pushes one item into queue with exclusive access.
    void push( T item )
    {
        std::lock_guard lock ( mutex_ );
        queue_.push ( std::move ( item ) );
    }

    // Returns next item when available; nullopt otherwise.
    std::optional<T> try_pop()
    {
        std::lock_guard lock ( mutex_ );
        if ( queue_.empty() )
            return std::nullopt;

        T item = std::move ( queue_.front() );
        queue_.pop();
        return item;
    }

    // Snapshot check; result may become stale immediately.
    bool empty() const
    {
        std::lock_guard lock ( mutex_ );
        return queue_.empty();
    }
};

}  // namespace angry
