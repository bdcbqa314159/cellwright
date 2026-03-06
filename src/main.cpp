#include "app/App.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        magic::App app;
        app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
