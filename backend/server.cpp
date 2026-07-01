#include "crow.h"
#include <vector>
#include <string>
#include <mutex>

// Basic message structure
struct Message {
    std::string sender;
    std::string content;
};

int main() {
    crow::SimpleApp app;
    std::vector<Message> messages;
    std::mutex mtx; // Protects the in-memory array

    // GET: Fetch all messages
    CROW_ROUTE(app, "/api/messages").methods("GET"_method)
    ([&messages, &mtx]() {
        std::lock_guard<std::mutex> lock(mtx);
        crow::json::wvalue result;
        
        for (size_t i = 0; i < messages.size(); i++) {
            result[i]["sender"] = messages[i].sender;
            result[i]["content"] = messages[i].content;
        }
        
        auto res = crow::response(result);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // POST: Send a new message
    CROW_ROUTE(app, "/api/messages").methods("POST"_method)
    ([&messages, &mtx](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400);

        std::lock_guard<std::mutex> lock(mtx);
        messages.push_back({body["sender"].s(), body["content"].s()});

        auto res = crow::response(201);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // OPTIONS: Handle CORS preflight for browsers
    CROW_ROUTE(app, "/api/messages").methods("OPTIONS"_method)
    ([]() {
        auto res = crow::response(204);
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    // Run on port 8080
    app.port(8080).multithreaded().run();
}