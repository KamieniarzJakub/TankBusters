#include <cstdio>
#include <error.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void do_stuff(const char *host, const char *port);
