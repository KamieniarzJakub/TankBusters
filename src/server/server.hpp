#pragma once
#include "gameManager.hpp"
#include "networked.hpp"

class Server : public Networked {
  GameManager gameManager;

public:
  virtual void handleNetworkEvent(uint32_t events) override;
  void ctrl_c(int);
};
