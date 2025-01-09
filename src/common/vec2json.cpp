#include <vec2json.hpp>

void to_json(json &j, const Vector2 &v) { j = json{v.x, v.y}; }
void from_json(const json &j, Vector2 &v) {
  v.x = j.at(0);
  v.y = j.at(1);
}
