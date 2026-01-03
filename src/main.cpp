#include <iostream>
#include "audio/AudioEngine.h"
#include "rendering/Renderer.h"

int main() {
    std::cout << "MusicVisVibeCode Starting..." << std::endl;

    AudioEngine audioEngine;
    if (!audioEngine.Initialize()) {
        std::cerr << "Failed to initialize Audio Engine!" << std::endl;
        return -1;
    }

    Renderer renderer(audioEngine);
    if (!renderer.Initialize(GetModuleHandle(NULL), 1280, 720)) {
        std::cerr << "Failed to initialize Renderer!" << std::endl;
        return -1;
    }

    renderer.Run();

    return 0;
}
