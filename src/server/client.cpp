#include "client.hpp"
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

Client::Client(int fd) : fd(fd) {}

Client::~Client() {
  if (thread.joinable()) {
    thread.join();
  }
  int res = shutdown(fd, SHUT_RDWR);
  if (res) {
    std::cerr << "Failed shutdown for client: " << player_id
              << ", room: " << room_id;
  }
  res = close(fd);
  if (res) {
    std::cerr << "Failed socket close for client: " << player_id
              << ", room: " << room_id;
  }
}
