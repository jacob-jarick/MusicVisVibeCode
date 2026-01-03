#include "AudioEngine.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cmath>
#include <complex>
#include <iostream>

#define M_PI 3.14159265358979323846

AudioEngine::AudioEngine() : m_running(false) {
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastTime);
}

AudioEngine::~AudioEngine() {
    m_running = false;
    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }
}

bool AudioEngine::Initialize() {
    m_running = true;
    m_audioThread = std::thread(&AudioEngine::AudioThread, this);
    return true;
}

void AudioEngine::Update() {
    // Main thread updates if necessary
}

// Simple FFT implementation
void fft(std::vector<std::complex<float>>& x) {
    const size_t N = x.size();
    if (N <= 1) return;

    std::vector<std::complex<float>> even(N / 2), odd(N / 2);
    for (size_t i = 0; i < N / 2; ++i) {
        even[i] = x[2 * i];
        odd[i] = x[2 * i + 1];
    }

    fft(even);
    fft(odd);

    for (size_t k = 0; k < N / 2; ++k) {
        std::complex<float> t = std::polar(1.0f, (float)(-2 * M_PI * k / N)) * odd[k];
        x[k] = even[k] + t;
        x[k + N / 2] = even[k] - t;
    }
}

void AudioEngine::AudioThread() {
    HRESULT hr;
    CoInitialize(NULL);

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;
    WAVEFORMATEX* pwfx = NULL;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) return;

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) return;

    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) return;

    // Initialize for loopback capture
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pwfx, NULL);
    if (FAILED(hr)) return;

    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) return;

    hr = pAudioClient->Start();
    if (FAILED(hr)) return;

    UINT32 packetLength = 0;
    BYTE* pData;
    UINT32 numFramesAvailable;
    DWORD flags;

    // FFT buffer
    const int FFT_SIZE = 512; // 256 bins
    std::vector<float> sampleBuffer;
    sampleBuffer.reserve(FFT_SIZE);

    while (m_running) {
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        while (packetLength != 0) {
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr)) break;

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                m_data.playing = false;
            } else {
                m_data.playing = true;
                
                // Process audio data
                // Assuming float format (WAVE_FORMAT_IEEE_FLOAT) which is standard for WASAPI Shared Mode
                float* pFloatData = (float*)pData;
                int channels = pwfx->nChannels;

                for (UINT32 i = 0; i < numFramesAvailable; i++) {
                    // Mix down to mono
                    float sample = 0;
                    for (int c = 0; c < channels; c++) {
                        sample += pFloatData[i * channels + c];
                    }
                    sample /= channels;
                    
                    sampleBuffer.push_back(sample);

                    if (sampleBuffer.size() >= FFT_SIZE) {
                        PerformFFT(sampleBuffer);
                        sampleBuffer.clear();
                    }
                }
            }

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr)) break;

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    pAudioClient->Stop();
    CoTaskMemFree(pwfx);
    if (pEnumerator) pEnumerator->Release();
    if (pDevice) pDevice->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pCaptureClient) pCaptureClient->Release();
    CoUninitialize();
}

void AudioEngine::PerformFFT(std::vector<float>& samples) {
    const int FFT_SIZE = 512;
    if (samples.size() < FFT_SIZE) return;

    // DC Removal (High-pass filter)
    float sum = 0.0f;
    for (float s : samples) sum += s;
    float mean = sum / samples.size();
    for (float& s : samples) s -= mean;

    std::vector<std::complex<float>> complexSamples(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) {
        // Apply Hanning window
        float window = 0.5f * (1.0f - cos(2.0f * M_PI * i / (FFT_SIZE - 1)));
        complexSamples[i] = samples[i] * window;
    }

    fft(complexSamples);

    // Update Data
    // std::lock_guard<std::mutex> lock(m_mutex); // Optional: if strict thread safety needed, but atomic types might suffice for simple vis

    // Update History Index
    m_data.historyIndex = (m_data.historyIndex + 1) % 60;

    float maxVal = 0.0f;

    for (int i = 0; i < 256; i++) {
        float magnitude = std::abs(complexSamples[i]);
        // Logarithmic scaling or simple magnitude? Let's stick to magnitude for now, maybe sqrt
        magnitude = sqrt(magnitude); 
        
        m_data.Spectrum[i] = magnitude;
        m_data.History[m_data.historyIndex][i] = magnitude;

        if (magnitude > maxVal) maxVal = magnitude;
    }

    // Auto-scale Logic
    // Dynamic Scaling (AGC)
    // Expansion: If maxVal > currentScale (Peak), snap to it immediately.
    // Contraction: If maxVal < currentScale (Peak), decay by 5% per second.
    
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    float deltaTime = (float)(currentTime.QuadPart - m_lastTime.QuadPart) / m_frequency.QuadPart;
    m_lastTime = currentTime;

    // m_data.Scale is the Multiplier (1.0 / Peak).
    // We want to track the Peak.
    float currentPeak = (m_data.Scale > 0.00001f) ? (1.0f / m_data.Scale) : 1.0f;

    if (maxVal > currentPeak) {
        // Expansion (Immediate)
        currentPeak = maxVal;
        // Cap Scale at 1.5 (which means minimum peak of 1.0/1.5 = 0.667)
        if (currentPeak < 0.667f) currentPeak = 0.667f;
    } else {
        // Contraction (Gradual)
        // User wants it to "creep up" (Peak creep down) over 5 seconds.
        // 50% decay per second.
        float decay = 0.50f * deltaTime;
        currentPeak -= (currentPeak * decay);
    }

    // Safety clamp
    if (currentPeak < 0.0001f) currentPeak = 0.0001f;

    m_data.Scale = 1.0f / currentPeak;

    // Normalize
    for (int i = 0; i < 256; i++) {
        m_data.SpectrumNormalized[i] = m_data.Spectrum[i] * m_data.Scale;
        if (m_data.SpectrumNormalized[i] > 1.0f) m_data.SpectrumNormalized[i] = 1.0f;
        
        m_data.HistoryNormalized[m_data.historyIndex][i] = m_data.SpectrumNormalized[i];

        // Calculate Highest Sample
        float highest = 0.0f;
        for (int h = 0; h < 6; h++) {
            int idx = (m_data.historyIndex - h + 60) % 60;
            if (m_data.HistoryNormalized[idx][i] > highest) {
                highest = m_data.HistoryNormalized[idx][i];
            }
        }
        m_data.SpectrumHighestSample[i] = highest;
    }
}
