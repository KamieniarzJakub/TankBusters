#include "room.hpp"

void to_json(json &j, const Room &r) {

  j = json{
      {"room_id", r.room_id}, {"status", r.status}, {"players", r.players}};
}
void from_json(const json &j, Room &r) {
  j.at("room_id").get_to(r.room_id);
  j.at("status").get_to(r.status);
  j.at("players").get_to(r.players);
}
