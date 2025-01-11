#include "roomManager.hpp"
#include <constants.hpp>

RoomManager::RoomManager(ClientNetworkManager &netManager)
    : networkManager(netManager) {}

void RoomManager::setSelectedRoom(const std::vector<Room> &rooms) {
  if (IsKeyPressed(KEY_SPACE) &&
      rooms.at(selected_room).players != Constants::PLAYERS_MAX) {
    networkManager.todo.push([&]() {
      return networkManager.join_room(rooms.at(selected_room).room_id);
    });
  } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
    selected_room--;
    selected_room %= rooms.size();
  } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
    selected_room++;
    selected_room %= rooms.size();
  }
  // TraceLog(LOG_DEBUG, "SELECTED ROOM: %d", selected_room);
}
