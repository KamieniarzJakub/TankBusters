#include <cstdint>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

bool write_uint32(int fd, uint32_t v);

bool read_uint32(int fd, uint32_t &v);

bool write_json(int fd, const json &j);

bool read_json(int fd, json &j, size_t maxsize);

bool expectEvent(int fd, uint32_t expected_event);

bool setEvent(int fd, uint32_t event);
