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

void to_json(json &j, const PlayerShortInfo &p) {

  j = json{{"player_id", p.player_id}, {"state", p.state}};
}
void from_json(const json &j, PlayerShortInfo &p) {
  j.at("player_id").get_to(p.player_id);
  j.at("state").get_to(p.state);
}

// Room &at_room_id(std::vector<Room> &rooms, uint32_t room_id) {
//   for (auto r = rooms.begin(); r != rooms.end(); r++) {
//     if (r->room_id == room_id) {
//       return *r;
//     }
//   }
//
//   throw std::out_of_range("Rooms out of range");
// }

uint32_t get_X_players(const std::vector<PlayerShortInfo> &ps, PlayerInfo pi) {
  uint32_t X = 0;
  for (auto p = ps.begin(); p != ps.end(); p++) {
    if (p->state == pi) {
      X++;
    }
  }
  return X;
}
