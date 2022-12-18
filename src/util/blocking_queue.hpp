#ifndef ECAS_UTIL_BLOCKING_QUEUE_HPP_
#define ECAS_UTIL_BLOCKING_QUEUE_HPP_

#include <queue>
#include <mutex>
#include <condition_variable>

namespace ecas {
namespace util {

template <typename T>
class BlockingQueue {
public:
    BlockingQueue() {};
    ~BlockingQueue() {};

    void push(const T& t);
    bool try_front(T* t);
    bool try_pop(T* t);
    void wait_and_pop(T* t);

    inline bool empty() const {
        std::unique_lock <std::mutex> lock(mutex_);
        return queue_.empty();
    }
    inline int size() const {
        std::unique_lock <std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<T> queue_;
};

template <typename T>
void BlockingQueue<T>::push(const T& t) {
    std::unique_lock <std::mutex> lock(mutex_);
    queue_.push(t);
    lock.unlock();
    cond_var_.notify_one();
}

template <typename T>
bool BlockingQueue<T>::try_front(T* t) {
    std::unique_lock <std::mutex> lock(mutex_);
    if (queue_.empty())
        return false;

    *t = queue_.front();
    return true;
}

template <typename T>
bool BlockingQueue<T>::try_pop(T* t) {
    std::unique_lock <std::mutex> lock(mutex_);
    if (queue_.empty())
        return false;

    *t = queue_.front();
    queue_.pop();
    return true;
}

template <typename T>
void BlockingQueue<T>::wait_and_pop(T* t) {
    std::unique_lock <std::mutex> lock(mutex_);
    while (queue_.empty())
        cond_var_.wait(lock);

    *t = queue_.front();
    queue_.pop();
}

} // namespace util
} // namespace ecas

#endif // ECAS_UTIL_BLOCKING_QUEUE_HPP_