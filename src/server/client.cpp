#include "client.hpp"

void Client::waitForWrite(bool epollout) {
  epoll_event ee{EPOLLIN | EPOLLRDHUP | (epollout ? EPOLLOUT : 0),
                 {.ptr = this}};
  epoll_ctl(epollout, EPOLL_CTL_MOD, _fd, &ee);
}
