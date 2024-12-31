#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <error.h>
#include <iostream>
#include <list>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#include "client.hpp"
#include "constants.hpp"
#include "gameManager.hpp"
#include "networkUtils.hpp"
#include "server.hpp"

int main(int argc, char **argv) {
  if (argc != 2)
    error(1, 0, "Need 1 arg (port)");
  auto port = readPort(argv[1]);

  signal(SIGINT, exit); // imperfect, but objects should clean after themselves
  signal(SIGPIPE, SIG_IGN);
  auto server = Server(port);
}
