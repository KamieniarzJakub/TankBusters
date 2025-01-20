#include <nlohmann/json.hpp>
#include <raymath.h>
using json = nlohmann::json;

void to_json(json &j, const Vector2 &v);
void from_json(const json &j, Vector2 &v);
