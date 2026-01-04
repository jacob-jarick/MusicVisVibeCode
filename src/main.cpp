#include <iostream>
#include <string>
#include "audio/AudioEngine.h"
#include "rendering/Renderer.h"

int main(int argc, char* argv[]) {
    std::cout << "MusicVisVibeCode Starting..." << std::endl;

    // Parse CLI arguments
    int startVis = -1; // -1 = default (Spectrum)
    float timeoutSeconds = 0.0f; // 0 = no timeout
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--timeout" || arg == "-t") {
            if (i + 1 < argc) {
                timeoutSeconds = std::stof(argv[i + 1]);
                std::cout << "Will exit after " << timeoutSeconds << " seconds" << std::endl;
                i++; // Skip next arg
            }
        } else if (arg == "--vis" || arg == "-v") {
            if (i + 1 < argc) {
                std::string visName = argv[i + 1];
                if (visName == "spectrum" || visName == "0") {
                    startVis = 0;
                    std::cout << "Starting with Spectrum visualization" << std::endl;
                } else if (visName == "cybervalley2" || visName == "cv2" || visName == "1") {
                    startVis = 1;
                    std::cout << "Starting with CyberValley2 visualization" << std::endl;
                } else if (visName == "linefader" || visName == "lf" || visName == "2") {
                    startVis = 2;
                    std::cout << "Starting with LineFader visualization" << std::endl;
                } else if (visName == "spectrum2" || visName == "s2" || visName == "3") {
                    startVis = 3;
                    std::cout << "Starting with Spectrum2 visualization" << std::endl;
                } else if (visName == "circle" || visName == "4") {
                    startVis = 4;
                    std::cout << "Starting with Circle visualization" << std::endl;
                } else {
                    std::cout << "Unknown visualization: " << visName << std::endl;
                    std::cout << "Available: spectrum (0), cybervalley2/cv2 (1), linefader/lf (2), spectrum2/s2 (3), circle (4)" << std::endl;
                }
                i++; // Skip next arg
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: MusicVisVibeCode [options]" << std::endl;
            std::cout << "  --vis, -v <name>      Start with specific visualization" << std::endl;
            std::cout << "                        Options: spectrum (0), cybervalley2/cv2 (1), linefader/lf (2), spectrum2/s2 (3), circle (4)" << std::endl;
            std::cout << "  --timeout, -t <sec>   Exit after N seconds (for testing)" << std::endl;
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

    renderer.Run(timeoutSeconds);

    return 0;
}
