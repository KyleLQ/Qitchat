#include "crow.h"

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        return "Hello World from Crow and vcpkg!";
    });

    // Run the app on port 8080
    app.port(8080).multithreaded().run();
}
