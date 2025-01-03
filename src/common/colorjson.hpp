#include <nlohmann/json.hpp>
#include <raylib.h>
using json = nlohmann::json;

void to_json(json &j, const Color &c);
void from_json(const json &j, Color &c);
