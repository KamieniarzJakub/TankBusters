#include <nlohmann/json.hpp>
#include <raylib.h>
using json = nlohmann::json;

void to_json(json &j, const Color &c) { j = json{c.r, c.g, c.b, c.a}; }
void from_json(const json &j, Color &c) {
  c.r = j.at(0);
  c.g = j.at(1);
  c.b = j.at(2);
  c.a = j.at(3);
}
