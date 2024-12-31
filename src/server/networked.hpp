#pragma once
#include <cstdint>

struct Networked {
  int fd;
  virtual ~Networked() = 0;
  virtual void handleNetworkEvent(uint32_t events) = 0;
};
