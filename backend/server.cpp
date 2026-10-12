#include "crow/middlewares/cors.h"
#include <sodium.h>
#include "db.h"
#include <iostream>
#include "routes/auth.h"
#include "routes/common.h"
#include "routes/messages.h"



int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl;
        return 1;
    }

    AppType app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors
      .global()
        .origin("*")
        .methods("POST"_method, "GET"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization");

    init_db();
    register_message_routes(app);
    register_auth_routes(app);
   
    // Run on port 8080
    app.port(8080).multithreaded().run();
}