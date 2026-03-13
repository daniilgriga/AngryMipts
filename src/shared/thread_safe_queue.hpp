#pragma once
#include <mutex>
#include <optional>
#include <queue>

namespace angry
{

template <typename T>
class ThreadSafeQueue
{
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;

public:
    void push( T item )
    {
        std::lock_guard lock ( mutex_ );
        queue_.push ( std::move ( item ) );
    }

    std::optional<T> try_pop()
    {
        std::lock_guard lock ( mutex_ );
        if ( queue_.empty() )
            return std::nullopt;

        T item = std::move ( queue_.front() );
        queue_.pop();
        return item;
    }

    bool empty() const
    {
        std::lock_guard lock ( mutex_ );
        return queue_.empty();
    }
};

}  // namespace angry
