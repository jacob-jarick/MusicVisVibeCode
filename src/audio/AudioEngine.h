#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <windows.h>

struct AudioData {
    bool playing = false;
    float Spectrum[256] = {0};
    float History[60][256] = {0};
    float Scale = 1.0f;
    float SpectrumNormalized[256] = {0};
    float HistoryNormalized[60][256] = {0};
    float SpectrumHighestSample[256] = {0};
    
    // Helper for circular buffer index
    int historyIndex = 0;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Initialize();
    void Update(); // Called every frame to process data if needed, or data can be updated in background
    const AudioData& GetData() const { return m_data; }

private:
    void AudioThread();
    void ProcessAudio(const float* buffer, int numFrames);
    void PerformFFT(std::vector<float>& samples);

    AudioData m_data;
    std::atomic<bool> m_running;
    std::thread m_audioThread;
    std::mutex m_mutex;
    
    // Scaling state
    float m_lastScaleUpdateTime = 0.0f;
    LARGE_INTEGER m_frequency;
    LARGE_INTEGER m_lastTime;
};
