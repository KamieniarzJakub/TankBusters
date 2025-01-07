#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <error.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "networkUtils.hpp"
#include "server.hpp"

int main(int argc, char **argv) {
  if (argc != 2)
    error(1, 0, "Need 1 arg (port)");
  auto port = readPort(argv[1]);

  signal(SIGINT, exit); // imperfect, but objects should clean after themselves
  signal(SIGPIPE, SIG_IGN);
  auto server = Server(port);
  std::cout << "Server is running on localhost:" << argv[1] << std::endl;
}
