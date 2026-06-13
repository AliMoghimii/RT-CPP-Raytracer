#include "renderer/Renderer.hpp"
#include <iostream>
#include <stdexcept>

using namespace std;

int main() {
    Renderer engine;
    engine.sceneManager.loadInitial("assets/scenes/default.json");

    try {
        engine.run();
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
