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

        try {
            SQLite::Database db = get_db();
            return crow::response(201, "placeholder");
            // todo now verify password hash. (should probably be a helper too!)

        } catch (const std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
            return crow::response(500);
        }
    });
}