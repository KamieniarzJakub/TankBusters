#include "client.hpp"
#include <cstdio>

const char *Client::signature() {
  char *res = nullptr;
  sprintf(res, " client_id=%ld,mainfd=%d", this->client_id, this->fd_main);
  return res;
}
