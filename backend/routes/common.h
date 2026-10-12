# pragma once
# include "crow.h"
# include "crow/middlewares/cors.h"
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>

/* file containing common helper and definitions */

using AppType = crow::App<crow::CORSHandler>;

// validate a json body,
// each pair in fields is (field name, intended type)
std::optional<std::string> validate_body(
    const crow::json::rvalue& body,
    std::initializer_list<std::pair<std::string_view, crow::json::type>> fields
);