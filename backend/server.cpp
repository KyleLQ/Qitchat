#include "crow.h"
#include "crow/middlewares/cors.h"
#include <vector>
#include <string>
#include <mutex>

struct Message {
    std::string sender;
    std::string content;
};

int main() {
    // 1. Initialize the App with the CORS Middleware configuration
    crow::App<crow::CORSHandler> app;

    // 2. Configure CORS globally
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors
      .global()
        .origin("*")
        .methods("POST"_method, "GET"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization");

    std::vector<Message> messages;
    std::mutex mtx;

    // GET: Fetch all messages (Notice how clean this is now!)
    CROW_ROUTE(app, "/api/messages").methods("GET"_method)
    ([&messages, &mtx]() {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::vector<crow::json::wvalue> list_items;
        list_items.reserve(messages.size());
        
        for (const auto& msg : messages) {
            crow::json::wvalue m;
            m["sender"] = msg.sender;
            m["content"] = msg.content;
            list_items.push_back(std::move(m));
        }
        
        crow::json::wvalue result = std::move(list_items);
        return crow::response(result); 
    });

    // POST: Send a new message
    CROW_ROUTE(app, "/api/messages").methods("POST"_method)
    ([&messages, &mtx](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400);

        // Quick check to avoid crashing if malformed JSON slips past
        if (!body.has("sender") || !body.has("content")) {
            return crow::response(400);
        }

        std::lock_guard<std::mutex> lock(mtx);
        messages.push_back({body["sender"].s(), body["content"].s()});

        return crow::response(201);
    });

    // Run on port 8080
    app.port(8080).multithreaded().run();
}