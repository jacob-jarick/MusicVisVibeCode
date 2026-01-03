#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

// Mocking the state
struct AudioData {
    float Scale = 1.0f; // Starts at 1.0 (Peak = 1.0)
};

int main() {
    AudioData m_data;
    float m_frequency = 1000.0f; // Mock frequency
    float m_lastTime = 0.0f;
    
    // Simulation parameters
    float constantInput = 0.5f; // We feed 0.5 consistently
    float dt = 1.0f / 60.0f; // 60 FPS
    float totalTime = 10.0f; // Run for 10 seconds

    std::cout << "Time(s) | Input | Peak (1/Scale) | Scale | Normalized Output" << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;

    for (float t = 0; t < totalTime; t += dt) {
        float maxVal = constantInput;
        
        // --- LOGIC START ---
        // m_data.Scale is the Multiplier (1.0 / Peak).
        // We want to track the Peak.
        float currentPeak = (m_data.Scale > 0.00001f) ? (1.0f / m_data.Scale) : 1.0f;

        if (maxVal > currentPeak) {
            // Expansion (Immediate)
            currentPeak = maxVal;
        } else {
            // Contraction (Gradual)
            // User wants it to "creep up" (Peak creep down) over 5 seconds.
            // Try 50% decay per second.
            float decay = 0.50f * dt;
            currentPeak -= (currentPeak * decay);
        }

        // Safety clamp
        if (currentPeak < 0.0001f) currentPeak = 0.0001f;

        m_data.Scale = 1.0f / currentPeak;
        // --- LOGIC END ---

        float normalizedOutput = maxVal * m_data.Scale;

        // Print every second
        if (std::abs(fmod(t, 1.0f)) < dt) {
            std::cout << std::fixed << std::setprecision(4) 
                      << t << "s   | " 
                      << maxVal << " | " 
                      << currentPeak << "       | " 
                      << m_data.Scale << " | " 
                      << normalizedOutput << std::endl;
        }
    }

    return 0;
}
