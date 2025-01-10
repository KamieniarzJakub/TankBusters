#include "jsonutils.hpp"
#include "raylib.h"
#include <cstdint>
#include <networkEvents.hpp>
#include <sys/types.h>

// returns true if ok
bool write_uint32(int fd, uint32_t v) {
  uint32_t val = htonl(v);
  ssize_t written = write(fd, &val, sizeof(val));
  return written == sizeof(v);
}

// returns true if ok
bool read_uint32(int fd, uint32_t &v) {
  uint32_t val = 0;
  ssize_t readb = read(fd, &val, sizeof(val));
  v = ntohl(val);
  return (readb == sizeof(uint32_t));
}

// returns true if ok
bool write_json(int fd, const json &j) {
  bool status;
  json jo;
  jo["data"] = j;
  std::vector<std::uint8_t> bson;
  try {
    bson = json::to_bson(jo);
  } catch (json::parse_error &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't convert json object to bson");
    return false;
  }
  status = write_uint32(fd, bson.size());
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send bson object size %lu", bson.size());
    return false;
  }

  ssize_t written = write(fd, &bson[0], bson.size());
  status = (written == (ssize_t)bson.size());
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send full bson object, sent %ld/%lu",
             written, bson.size());
  }
  return status;
}

// returns true if ok
// maxsize is optional, disable this check by passing -1 (MAX_SIZE_T)
bool read_json(int fd, json &j, size_t maxsize) {
  bool status;
  uint32_t bson_size;
  status = read_uint32(fd, bson_size);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't read bson object size");
    return false;
  }
  if (bson_size > maxsize) {
    TraceLog(LOG_ERROR,
             "JSON: bson object size %lu exceeds maximum expected size %llu",
             bson_size, maxsize);
    return false;
  }

  auto bson = std::vector<std::uint8_t>(bson_size, 0);
  ssize_t readb = read(fd, &bson[0], bson.size());
  if (readb != (ssize_t)bson.size()) {
    TraceLog(LOG_ERROR, "NET: Couldn't read full bson object, received %ld/%lu",
             read, bson_size);
    return false;
  }

  try {
    json jj = json::from_bson(bson);
    j = jj.at("data");
    return true;
  } catch (json::parse_error &ex) {
    TraceLog(LOG_WARNING, "JSON: parse error at byte %d", ex.byte);
    return false;
  }
}

// returns true if ok
bool expectEvent(int fd, uint32_t expected_event) {
  uint32_t received_event;
  bool status = read_uint32(fd, received_event);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Didn't receive full NetworkEvent");
    return false;
  }
  if (received_event != expected_event) {
    TraceLog(LOG_ERROR,
             ("NET: Received NetworkEvent (" +
              network_event_to_string((NetworkEvents)received_event) +
              ") doesn't match (" +
              network_event_to_string((NetworkEvents)expected_event) + ")")
                 .c_str());
    return false;
  }
  return true;
}

bool setEvent(int fd, uint32_t event) {
  bool status = write_uint32(fd, event);
  if (!status) {
    TraceLog(LOG_ERROR, ("NET: Cannot send NetworkEvent " +
                         network_event_to_string((NetworkEvents)event))
                            .c_str());
  }
  return status;
}
