#pragma once
#include <cstdint>
void ctrl_c(int);

uint16_t readPort(char *txt);

void setReuseAddr(int sock);
