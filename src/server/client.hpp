#pragma once
#include "networked.hpp"
#include <list>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

class Client : public Networked {
  int _fd;
  char *readBuffer;
  uint16_t bufsize;
  std::list<char *> dataToWrite;

  void waitForWrite(bool epollout);

public:
  Client(int fd) : _fd(fd) { this->waitForWrite(false); }
  ~Client() override {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, _fd, nullptr);
    shutdown(_fd, SHUT_RDWR);
    close(_fd);
  }

  int fd() const { return _fd; }

  void handleNetworkEvent(uint32_t events) override;

  void write(char *buffer, int count);

  void remove();
};
