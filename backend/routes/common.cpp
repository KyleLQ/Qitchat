#include "common.h"
#include <optional>

std::optional<std::string> validate_body(
    const crow::json::rvalue& body,
    std::initializer_list<std::pair<std::string_view, crow::json::type>> fields
) {
    if (!body) return "body is missing";

    for (auto& [name, type] : fields) {
        if (!body.has(std::string(name))) {
            return "body is missing field: " + std::string(name);
        }
        if (body[std::string(name)].t() != type) {
            return "field " + std::string(name) + " is not the correct type";
        }
    }
    return std::nullopt;
}