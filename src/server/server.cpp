#include "server.hpp"

void Server::ctrl_c(int) {
  for (Client *client : clients)
    delete client;
  close(servFd);
  printf("Closing server\n");
  exit(0);
}
}
;
