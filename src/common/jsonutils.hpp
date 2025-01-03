#include <cstdint>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

void write_uint32(int fd, uint32_t v);

uint32_t read_uint32(int fd);

void write_json(int fd, const json &j);

json read_json(int fd);
