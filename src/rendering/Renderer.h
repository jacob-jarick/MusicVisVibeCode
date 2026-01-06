#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <memory>
#include "../audio/AudioEngine.h"
#include "../Config.h"
#include "../visualizations/BaseVisualization.h"

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

    // Visualization State
    enum class Visualization { Spectrum, CyberValley2, LineFader, Spectrum2, Circle };
    Visualization m_currentVis = Visualization::Spectrum;
    
    // Visualization instances
    std::unique_ptr<BaseVisualization> m_visualizations[5];

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
    bool m_showDisableMenu = false;
    bool m_useNormalized = true;
    bool m_isFullscreen = false;
    
    // Config
    Config m_config;
    
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
    void RenderClock();
    void UpdateTextTexture(const std::string& text, bool rightAlign = false);
    void CreateTextResources();
    
    // Config & Visualization Management
    void LoadConfigIntoState();
    void SaveStateToConfig();
    void ResetToDefaults();
    std::string GetVisualizationName(int vis);
    int GetNextEnabledVis(int currentVis, bool forward = true);
};
