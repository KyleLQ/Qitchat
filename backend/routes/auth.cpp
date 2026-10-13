#include "auth.h"
#include "../db.h"
#include <sodium.h>
#include <SQLiteCpp/Transaction.h>
#include <sqlite3.h>
#include <regex>
#include <string>
#include <iostream>
#include <SQLiteCpp/Database.h>
#include<SQLiteCpp/Statement.h>
#include <stdexcept>

namespace {
    // sets password to be its hash
    void hash_password(std::string& password) {
        char hash[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str_alg(hash, password.c_str(), password.size(),
                                crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                crypto_pwhash_MEMLIMIT_INTERACTIVE,
                                crypto_pwhash_ALG_ARGON2ID13)) {
            throw std::runtime_error("Password hashing failed!");
        }
        password.assign(hash);
        sodium_memzero(hash, sizeof(hash));
    }

    // creates a session for userId and returns unhashed token
    std::string createSession(SQLite::Database& db, long userId) {
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

        SQLite::Statement addTokenHashQuery(
                db, "INSERT INTO sessions (token_hash, user_id, expires_at) VALUES (?, ?, datetime('now', '+30 days'))");
        addTokenHashQuery.bind(1, tokenHashHex);
        addTokenHashQuery.bind(2, static_cast<int64_t>(userId));
        addTokenHashQuery.exec();

        sodium_memzero(charToken, sizeof(charToken));
        sodium_memzero(charHashToken, sizeof(charHashToken));
        return token;
    }
}
void register_auth_routes(AppType& app) {
     // POST: Signup
    CROW_ROUTE(app, "/api/auth/signup").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (auto err = validate_body(body, {
            {"username", crow::json::type::String,},
            {"password", crow::json::type::String}
            }
        )) {
            return crow::response(400, *err);
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

          std::string passwordHash = password;
          hash_password(passwordHash);

          SQLite::Transaction transaction(db);

          SQLite::Statement addNewUserQuery(
              db, "INSERT INTO users (username, password_hash) VALUES (?, ?)");
          addNewUserQuery.bind(1, username);
          addNewUserQuery.bind(2, passwordHash);
          addNewUserQuery.exec();

          long userId = db.getLastInsertRowid();
          std::string token = createSession(db, userId);
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

    // POST: Login
    CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
    ([](const crow::request& req){
        auto body = crow::json::load(req.body);
       if (auto err = validate_body(body, {
            {"username", crow::json::type::String,},
            {"password", crow::json::type::String}
            }
        )) {
            return crow::response(400, *err);
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s();
        if (password.size() > 512) {
            return crow::response(401, "Invalid credentials");
        }

        try {
            SQLite::Database db = get_db();
            SQLite::Statement checkLoginQuery(
                db, "SELECT id, password_hash FROM users WHERE username = ?");
            checkLoginQuery.bind(1, username);
            
            if (!checkLoginQuery.executeStep()) {
                return crow::response(401, "Invalid credentials");
            }

            std::string storedHash = checkLoginQuery.getColumn(1).getString();
            if (crypto_pwhash_str_verify(storedHash.c_str(), password.c_str(), password.size()) != 0) {
                return crow::response(401, "Invalid credentials");
            }
            long userId = checkLoginQuery.getColumn(0).getInt64();
            std::string token = createSession(db, userId);
            crow::json::wvalue res;
            res["token"] = token;
            return crow::response(200, res);

        } catch (const std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
            return crow::response(500);
        }
    });
}