#pragma once
#include <cstdint>

struct Networked {
  virtual ~Networked() {}
  virtual void handleNetworkEvent(uint32_t events) = 0;
};
