#include <chrono>
#include <networking.hpp>
#include <vector>

struct RoomManager {
  const std::chrono::milliseconds room_fetch_interval{5000};
  std::chrono::steady_clock::time_point last_room_fetch =
      std::chrono::steady_clock::time_point{};
  uint8_t selected_room;

  ClientNetworkManager &networkManager;

  RoomManager(ClientNetworkManager &netManager);

  void setSelectedRoom(const std::vector<Room> &rooms);
};
