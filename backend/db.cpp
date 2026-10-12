# include "db.h"

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