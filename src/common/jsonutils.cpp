#include "jsonutils.hpp"
#include <iostream>

void write_uint32(int fd, uint32_t v) {
  uint32_t val = htonl(v);
  write(fd, &val, sizeof(val));
}

uint32_t read_uint32(int fd) {
  uint32_t val = 0;
  read(fd, &val, sizeof(val));
  return ntohl(val);
}

void write_json(int fd, const json &j) {
  json jo;
  jo["data"] = j;
  std::vector<std::uint8_t> bson = json::to_bson(jo);
  write_uint32(fd, bson.size());
  write(fd, &bson[0], bson.size());
}

json read_json(int fd) {
  auto bson = std::vector<std::uint8_t>(read_uint32(fd), 0);
  read(fd, &bson[0], bson.size());
  json j = json::from_bson(bson); // FIXME: try catch safe parsing
  //std::cout << j << std::endl;
  return j.at("data");
}
