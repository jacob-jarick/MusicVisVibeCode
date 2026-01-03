#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include "../audio/AudioEngine.h"

using namespace DirectX;

class Renderer {
public:
    Renderer(AudioEngine& audioEngine);
    ~Renderer();

    bool Initialize(HINSTANCE hInstance, int width, int height, int startVis = -1);
    void Run(float timeoutSeconds = 0.0f);

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Render();
    void UpdateSpectrumVis(float deltaTime);
    void UpdateCyberValley2Vis(float deltaTime);
    void UpdateLineFaderVis(float deltaTime);
    void UpdateSpectrum2Vis(float deltaTime);

    AudioEngine& m_audioEngine;
    HWND m_hwnd;
    int m_width;
    int m_height;

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;

    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_vertexBuffer = nullptr;

    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT4 color;
        XMFLOAT2 texCoord;
    };

    // Visualization State
    enum class Visualization { Spectrum, CyberValley2, LineFader, Spectrum2 };
    Visualization m_currentVis = Visualization::Spectrum;

    // Spectrum Vis State
    float m_peakLevels[16] = {0};
    float m_decayRate = 5.0f; 

    // CyberValley2 Vis State
    float m_cv2Time = 0.0f;           // Day/night cycle timer (0-600 seconds)
    float m_cv2Speed = 50.0f;         // Scroll speed percentage (5% to 200%), 50% = ~2s to horizon
    float m_cv2GridOffset = 0.0f;     // Grid scroll position (0-1)
    bool m_cv2SunMode = false;        // true = Day, false = Night (default night mode)
    bool m_cv2ShowGrid = true;        // Grid visibility toggle
    float m_cv2MountainHistory[60][256] = {{0}};  // Frozen snapshots of spectrum for mountains
    int m_cv2HistoryWriteIndex = 0;   // Current write position in history buffer
    float m_cv2TimeSinceLastLine = 0.0f;  // Time accumulator for line drawing (2 lines/sec)

    // LineFader Vis State
    int m_lfScrollSpeed = 5;          // Scroll speed in pixels per frame (1-50)
    float m_lfFadeRate = 0.005f;      // Fade rate per frame (0.0005 - 0.005, i.e., 0.05% - 0.50%)
    enum class LFMirrorMode { None, BassEdges, BassCenter };
    LFMirrorMode m_lfMirrorMode = LFMirrorMode::BassEdges;
    ID3D11Texture2D* m_lfHistoryTexture = nullptr;
    ID3D11ShaderResourceView* m_lfHistorySRV = nullptr;
    ID3D11RenderTargetView* m_lfHistoryRTV = nullptr;
    ID3D11Texture2D* m_lfTempTexture = nullptr;
    ID3D11ShaderResourceView* m_lfTempSRV = nullptr;
    ID3D11RenderTargetView* m_lfTempRTV = nullptr;

    // Spectrum2 Vis State
    float m_s2PeakLevels[28] = {0};
    float m_s2DecayRate = 5.0f;       // Segments per second (default: 1 segment every 0.2s)
    enum class S2MirrorMode { None, BassEdges, BassCenter };
    S2MirrorMode m_s2MirrorMode = S2MirrorMode::BassEdges;

    LARGE_INTEGER m_lastTime;
    LARGE_INTEGER m_frequency;
    float m_fps = 0.0f;
    int m_frameCount = 0;
    float m_timeElapsed = 0.0f;
    float m_timeoutSeconds = 0.0f;
    float m_runningTime = 0.0f;

    // OSD State
    bool m_showHelp = false;
    bool m_showInfo = false;
    bool m_showClock = false;
    bool m_useNormalized = true;
    bool m_isFullscreen = false;
    
    // Text Rendering
    ID3D11Texture2D* m_textTexture = nullptr;
    ID3D11ShaderResourceView* m_textSRV = nullptr;
    ID3D11SamplerState* m_samplerState = nullptr;
    ID3D11BlendState* m_blendState = nullptr;
    
    // Background
    ID3D11Texture2D* m_backgroundTexture = nullptr;
    ID3D11ShaderResourceView* m_backgroundSRV = nullptr;
    bool m_showBackground = false;
    float m_bgAspectRatio = 1.0f;
    std::wstring m_currentBgPath;
    std::vector<std::wstring> m_backgroundFiles;
    int m_currentBgIndex = -1;
    ULONG_PTR m_gdiplusToken = 0;
    
    void ScanBackgrounds();
    void LoadBackground(int index);
    void LoadRandomBackground();
    
    void HandleInput(WPARAM key);
    void RenderOSD();
    void UpdateTextTexture(const std::string& text, bool rightAlign = false);
    void CreateTextResources();
};
