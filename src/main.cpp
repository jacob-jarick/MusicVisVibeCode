#include <iostream>
#include <string>
#include "audio/AudioEngine.h"
#include "rendering/Renderer.h"

int main(int argc, char* argv[]) {
    std::cout << "MusicVisVibeCode Starting..." << std::endl;

    // Parse CLI arguments
    int startVis = -1; // -1 = default (Spectrum)
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--vis" || arg == "-v") {
            if (argc > 2) {
                std::string visName = argv[2];
                if (visName == "spectrum" || visName == "0") {
                    startVis = 0;
                    std::cout << "Starting with Spectrum visualization" << std::endl;
                } else if (visName == "cybervalley2" || visName == "cv2" || visName == "1") {
                    startVis = 1;
                    std::cout << "Starting with CyberValley2 visualization" << std::endl;
                } else {
                    std::cout << "Unknown visualization: " << visName << std::endl;
                    std::cout << "Available: spectrum (0), cybervalley2/cv2 (1)" << std::endl;
                }
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: MusicVisVibeCode [--vis <name>]" << std::endl;
            std::cout << "  --vis, -v <name>   Start with specific visualization" << std::endl;
            std::cout << "                     Options: spectrum (0), cybervalley2/cv2 (1)" << std::endl;
            std::cout << "\nControls:" << std::endl;
            std::cout << "  H: Toggle Help" << std::endl;
            std::cout << "  Left/Right: Switch visualization" << std::endl;
            std::cout << "  ESC: Quit" << std::endl;
            return 0;
        }
    }

    AudioEngine audioEngine;
    if (!audioEngine.Initialize()) {
        std::cerr << "Failed to initialize Audio Engine!" << std::endl;
        return -1;
    }

    Renderer renderer(audioEngine);
    if (!renderer.Initialize(GetModuleHandle(NULL), 1280, 720, startVis)) {
        std::cerr << "Failed to initialize Renderer!" << std::endl;
        return -1;
    }

    renderer.Run();

    return 0;
}
