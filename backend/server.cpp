#include "crow.h"
#include "crow/middlewares/cors.h"
#include <crow/http_request.h>
#include <sodium.h>
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_pwhash.h>
#include <sqlite3.h>
#include <regex>
#include <vector>
#include <string>
#include <iostream>

// Helper to open a connection per thread
SQLite::Database get_db() {
    SQLite::Database db("chat.db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    // WAL mode allows concurrent readers while a thread writes
    db.exec("PRAGMA journal_mode=WAL;");
    // Wait up to 5s on locked DB instead of failing immediately with SQLITE_BUSY
    db.exec("PRAGMA busy_timeout = 5000;");
    return db;
}

void init_db() {
    SQLite::Database db = get_db();
    db.exec("CREATE TABLE IF NOT EXISTS messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "sender TEXT NOT NULL, "
            "content TEXT NOT NULL, "
            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
            ");");
    
    // Set message limit to 1000
    db.exec("DROP TRIGGER IF EXISTS limit_messages_count;");
    db.exec("CREATE TRIGGER limit_messages_count "
            "AFTER INSERT ON messages "
            "BEGIN "
                "DELETE FROM messages "
                "WHERE id NOT IN ( "
                    "SELECT id FROM messages "
                    "ORDER BY id DESC "
                    "LIMIT 1000 "
            ");"
            "END;");

    db.exec("CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT UNIQUE NOT NULL, "
        "password_hash TEXT NOT NULL, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");");

    db.exec("CREATE TABLE IF NOT EXISTS sessions ("
        "token_hash TEXT PRIMARY KEY, "
        "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, "
        "expires_at DATETIME NOT NULL, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");");
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl;
        return 1;
    }
    // 1. Initialize the App with the CORS Middleware configuration
    crow::App<crow::CORSHandler> app;

    // 2. Configure CORS globally
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors
      .global()
        .origin("*")
        .methods("POST"_method, "GET"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization");

    init_db();

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
        if (!body) return crow::response(400);

        // Quick check to avoid crashing if malformed JSON slips past
        if (!body.has("sender") || !body.has("content")) {
            return crow::response(400);
        }
        // Validate types before calling .s() — it asserts/throws on non-strings
        if (body["sender"].t() != crow::json::type::String ||
            body["content"].t() != crow::json::type::String) {
            return crow::response(400);
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

    // POST: Signup
    CROW_ROUTE(app, "/api/auth/signup").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "missing body!");

        if (!body.has("username") || !body.has("password")) {
            return crow::response(400, "missing username or password!");
        }

        if (body["username"].t() != crow::json::type::String ||
            body["password"].t() != crow::json::type::String) {
            return crow::response(400, "username and password must be strings!");
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s();

        if (password.length() < 8 || password.length() > 512) {
            return crow::response(400, "password length must be in [8,512]");
        }

        const std::regex usernamePattern(R"(^[A-Za-z0-9_.-]{3,32}$)");
        if (!std::regex_match(username, usernamePattern)) {
          return crow::response(400,
                                "Username must be 3-32 characters and contain "
                                "only letters, numbers, _, ., or -");
        }

        try {
          SQLite::Database db = get_db();

          char charHash[crypto_pwhash_STRBYTES];
          if (crypto_pwhash_str_alg(charHash, password.c_str(), password.size(),
                                    crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                    crypto_pwhash_MEMLIMIT_INTERACTIVE,
                                    crypto_pwhash_ALG_ARGON2ID13)) {
            throw std::runtime_error("Password hashing failed!");
          }

          unsigned char charToken[32];
          randombytes_buf(charToken, sizeof(charToken));

          char hexToken[65];
          sodium_bin2hex(hexToken, sizeof(hexToken), charToken, sizeof(charToken));

          unsigned char charHashToken[crypto_hash_sha256_BYTES];
          crypto_hash_sha256(charHashToken, charToken, sizeof(charToken));

          char hexHashToken[crypto_hash_sha256_BYTES * 2 + 1];
          sodium_bin2hex(hexHashToken, sizeof(hexHashToken), charHashToken, sizeof(charHashToken));

          std::string token = std::string(hexToken);
          std::string tokenHashHex = std::string(hexHashToken);
          std::string passwordHash = std::string(charHash);

          SQLite::Transaction transaction(db);

          SQLite::Statement addNewUserQuery(
              db, "INSERT INTO users (username, password_hash) VALUES (?, ?)");
          addNewUserQuery.bind(1, username);
          addNewUserQuery.bind(2, passwordHash);
          addNewUserQuery.exec();

          long userId = db.getLastInsertRowid();
          SQLite::Statement addTokenHashQuery(
            db, "INSERT INTO sessions (token_hash, user_id, expires_at) VALUES (?, ?, datetime('now', '+30 days'))");
          addTokenHashQuery.bind(1, tokenHashHex);
          addTokenHashQuery.bind(2, static_cast<int64_t>(userId));
          addTokenHashQuery.exec();

          transaction.commit();

          crow::json::wvalue res;
          res["token"] = token;
          return crow::response(201, res);
        } catch (const SQLite::Exception &e) {
          if (e.getErrorCode() == SQLITE_CONSTRAINT ||
              e.getExtendedErrorCode() == SQLITE_CONSTRAINT_UNIQUE) {
            return crow::response(409, "Username already taken");
          }
          std::cerr << e.what() << std::endl;
          return crow::response(500);
        } catch (const std::exception &e) {
          std::cerr << e.what() << std::endl;
          return crow::response(500);
        }
    });

    // Run on port 8080
    app.port(8080).multithreaded().run();
}