#include "app/App.hpp"
#include <iostream>

int main() {
    try {
        magic::App app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
