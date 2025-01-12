#include "client.hpp"
#include <cstdio>

const std::string Client::signature() {
  char *res = nullptr;
  sprintf(res, " client_id=%ld,mainfd=%d", this->client_id, this->fd_main);
  return std::string(res);
}
