#include "messages.h"
#include "../db.h"
#include "common.h"
#include <crow/json.h>
#include <optional>
#include <vector>

void register_message_routes(AppType& app) {
    // GET: Fetch all messages
    CROW_ROUTE(app, "/api/messages").methods("GET"_method)
    ([]() {
        try {
            SQLite::Database db = get_db();
            SQLite::Statement query(db, 
                "SELECT sender, content, datetime(created_at, \'localtime\') FROM messages ORDER BY id ASC");
            
            std::vector<crow::json::wvalue> list_items;
            
            while(query.executeStep()) {
                crow::json::wvalue m;
                m["sender"] = query.getColumn(0).getText();
                m["content"] = query.getColumn(1).getText();
                m["created_at"] = query.getColumn(2).getText();
                list_items.push_back(std::move(m));
            }
            
            crow::json::wvalue result = std::move(list_items);
            return crow::response(result); 
        } catch (const std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
            return crow::response(500);
        }
    });

    // POST: Send a new message
    CROW_ROUTE(app, "/api/messages").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if ( auto err = validate_body(body, {
            {"sender", crow::json::type::String,},
            {"content", crow::json::type::String}
        })) {
            return crow::response(400, *err);
        }

        try {
            SQLite::Database db = get_db();
            SQLite::Statement query(db, "INSERT INTO messages (sender, content) VALUES (?, ?)");
            query.bind(1, std::string(body["sender"].s()));
            query.bind(2, std::string(body["content"].s()));

            query.exec();
            return crow::response(201);
        } catch (const std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
            return crow::response(500);
        }
    });
}