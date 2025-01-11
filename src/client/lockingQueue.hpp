#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T> struct LockingQueue {
private:
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cond;

public:
  // no thread safety
  bool isEmpty() { return queue.empty(); }

  void push(T item) {
    std::unique_lock<std::mutex> lock(mutex);
    queue.push(item);
    cond.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [this]() { return !queue.empty(); });
    T item = queue.front();
    queue.pop();
    return item;
  }
};
