#pragma once
#include <sys/eventfd.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>

template <typename T> struct LockingQueue {
private:
  std::queue<T> queue;
  int efd;
  std::mutex mutex;

public:
  LockingQueue() {
    efd = eventfd(0, EFD_SEMAPHORE);
    if (efd == -1) {
      throw std::runtime_error{"Eventfd LockingQueue"};
    }
    std::cout << "Eventfd " << efd << std::endl;
  }
  ~LockingQueue() {
    while (!queue.empty()) {
      queue.pop();
    }
    close(efd);
  }

  int get_event_fd() { return efd; }

  void push(T item) {
    std::cout << "push " << efd << std::endl;
    std::unique_lock<std::mutex> lock(mutex);
    queue.push(item);
    const uint64_t one = 1;
    write(efd, &one, sizeof(one));
  }

  T pop() {
    std::cout << "pop " << efd << std::endl;
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t one;
    read(efd, &one, sizeof(one));
    T item = queue.front();
    queue.pop();
    return item;
  }
};
