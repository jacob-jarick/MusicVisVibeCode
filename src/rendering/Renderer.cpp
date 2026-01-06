#include "Renderer.h"
#include "../visualizations/SpectrumVis.h"
#include "../visualizations/CyberValley2Vis.h"
#include "../visualizations/LineFaderVis.h"
#include "../visualizations/Spectrum2Vis.h"
#include "../visualizations/CircleVis.h"
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <random>

// Simple Shaders
const char* VS_SRC = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float4 col : COLOR;
    float2 tex : TEXCOORD;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float2 tex : TEXCOORD;
};
PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0);
    output.col = input.col;
    output.tex = input.tex;
    return output;
}
)";

const char* PS_SRC = R"(
Texture2D tex : register(t0);
SamplerState sam : register(s0);

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float2 tex : TEXCOORD;
};
float4 main(PS_INPUT input) : SV_Target {
    if (input.tex.x < 0) return input.col; // Solid color mode
    
    // Texture mode (OSD)
    float4 texColor = tex.Sample(sam, input.tex);
    // GDI doesn't write alpha correctly, so we use luminance as alpha
    // This assumes white text on black background
    float alpha = dot(texColor.rgb, float3(0.299, 0.587, 0.114));
    return float4(texColor.rgb, alpha) * input.col;
}
)";

Renderer::Renderer(AudioEngine& audioEngine) : m_audioEngine(audioEngine) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

Renderer::~Renderer() {
    // Cleanup all visualizations
    for (int i = 0; i < 5; i++) {
        if (m_visualizations[i]) {
            m_visualizations[i]->Cleanup();
        }
    }
    
    if (m_backgroundSRV) m_backgroundSRV->Release();
    if (m_backgroundTexture) m_backgroundTexture->Release();
    if (m_textSRV) m_textSRV->Release();
    if (m_textTexture) m_textTexture->Release();
    if (m_samplerState) m_samplerState->Release();
    if (m_blendState) m_blendState->Release();
    if (m_vertexBuffer) m_vertexBuffer->Release();
    if (m_inputLayout) m_inputLayout->Release();
    if (m_vertexShader) m_vertexShader->Release();
    if (m_pixelShader) m_pixelShader->Release();
    if (m_renderTargetView) m_renderTargetView->Release();
    if (m_swapChain) m_swapChain->Release();
    if (m_context) m_context->Release();
    if (m_device) m_device->Release();
    
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

LRESULT CALLBACK Renderer::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Renderer* pRenderer = reinterpret_cast<Renderer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (pRenderer) {
                pRenderer->HandleInput(wParam);
            }
            return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

bool Renderer::Initialize(HINSTANCE hInstance, int width, int height, int startVis) {
    m_width = width;
    m_height = height;
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastTime);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "MusicVisVibeCode", NULL };
    RegisterClassEx(&wc);
    m_hwnd = CreateWindow("MusicVisVibeCode", "MusicVisVibeCode", WS_OVERLAPPEDWINDOW, 100, 100, width, height, NULL, NULL, wc.hInstance, NULL);

    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = m_hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &scd, &m_swapChain, &m_device, NULL, &m_context);

    ID3D11Texture2D* pBackBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    m_device->CreateRenderTargetView(pBackBuffer, NULL, &m_renderTargetView);
    pBackBuffer->Release();

    m_context->OMSetRenderTargets(1, &m_renderTargetView, NULL);

    D3D11_VIEWPORT viewport = { 0, 0, (float)width, (float)height, 0.0f, 1.0f };
    m_context->RSSetViewports(1, &viewport);
    
    // Create Rasterizer State (Disable Culling)
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.AntialiasedLineEnable = FALSE;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.SlopeScaledDepthBias = 0.0f;

    ID3D11RasterizerState* pRasterState = nullptr;
    m_device->CreateRasterizerState(&rasterDesc, &pRasterState);
    m_context->RSSetState(pRasterState);
    if(pRasterState) pRasterState->Release();

    // Compile Shaders
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    D3DCompile(VS_SRC, strlen(VS_SRC), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &vsBlob, NULL);
    D3DCompile(PS_SRC, strlen(PS_SRC), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &psBlob, NULL);

    m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &m_vertexShader);
    m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &m_pixelShader);

    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    m_device->CreateInputLayout(ied, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);

    // Create Dynamic Vertex Buffer
    D3D11_BUFFER_DESC bd = {0};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * 50000; // Enough for complex visualizations
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&bd, NULL, &m_vertexBuffer);

    // Create Blend State for Alpha Blending (Text)
    D3D11_BLEND_DESC blendDesc = {0};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_device->CreateBlendState(&blendDesc, &m_blendState);

    // Create Sampler State
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    m_device->CreateSamplerState(&sampDesc, &m_samplerState);

    CreateTextResources();
    CreateClockResources();

    // Initialize visualizations
    m_visualizations[0] = std::make_unique<SpectrumVis>();
    m_visualizations[1] = std::make_unique<CyberValley2Vis>();
    m_visualizations[2] = std::make_unique<LineFaderVis>();
    m_visualizations[3] = std::make_unique<Spectrum2Vis>();
    m_visualizations[4] = std::make_unique<CircleVis>();
    
    // Initialize all visualizations
    for (int i = 0; i < 5; i++) {
        m_visualizations[i]->Initialize(m_device, m_context, width, height);
    }

    // Load config and apply settings
    m_config.Load();
    LoadConfigIntoState();
    
    // Apply command line visualization override (after config load)
    if (startVis >= 0 && startVis <= 4) {
        m_currentVis = (Visualization)startVis;
    }
    
    // Mark as dirty so it saves periodically
    m_config.isDirty = true;
    
    // If no background set, start with a random one
    if (!m_showBackground || m_currentBgIndex < 0) {
        m_showBackground = true;
        LoadRandomBackground();
    }

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    return true;
}

void Renderer::Run(float timeoutSeconds) {
    m_timeoutSeconds = timeoutSeconds;
    m_runningTime = 0.0f;
    
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Render();
            
            // Periodically save config if dirty (every 5 seconds)
            m_config.timeSinceLastSave += 0.016f; // Approximate frame time
            if (m_config.isDirty && m_config.timeSinceLastSave >= 5.0f) {
                m_config.Save();
                m_config.timeSinceLastSave = 0.0f;
            }
            
            // Check timeout
            if (m_timeoutSeconds > 0.0f && m_runningTime >= m_timeoutSeconds) {
                std::cout << "Timeout reached (" << m_timeoutSeconds << "s), exiting..." << std::endl;
                // Save config before exit
                if (m_config.isDirty) {
                    SaveStateToConfig();
                    m_config.Save();
                }
                PostQuitMessage(0);
            }
        }
    }
    
    // Save config on exit
    if (m_config.isDirty) {
        SaveStateToConfig();
        m_config.Save();
    }
}

void Renderer::Render() {
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    float deltaTime = (float)(currentTime.QuadPart - m_lastTime.QuadPart) / m_frequency.QuadPart;
    m_lastTime = currentTime;
    m_runningTime += deltaTime;
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(m_renderTargetView, clearColor);

    // Set Common States
    m_context->OMSetBlendState(m_blendState, NULL, 0xffffffff);
    m_context->PSSetSamplers(0, 1, &m_samplerState);

    // Draw Background
    if (m_showBackground && m_backgroundSRV && (m_currentVis == Visualization::Spectrum || m_currentVis == Visualization::LineFader || m_currentVis == Visualization::Spectrum2 || m_currentVis == Visualization::Circle)) {
        float screenAR = (float)m_width / (float)m_height;
        float imageAR = m_bgAspectRatio;
        
        float uMin = 0.0f, uMax = 1.0f;
        float vMin = 0.0f, vMax = 1.0f;
        
        if (screenAR > imageAR) {
            // Screen is wider than image. Match width. Crop height.
            float range = imageAR / screenAR;
            vMin = 0.5f - range * 0.5f;
            vMax = 0.5f + range * 0.5f;
        } else {
            // Screen is taller than image. Match height. Crop width.
            float range = screenAR / imageAR;
            uMin = 0.5f - range * 0.5f;
            uMax = 0.5f + range * 0.5f;
        }

        // Full screen quad with adjusted UVs
        std::vector<Vertex> bgVertices = {
            { {-1.0f, 1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {uMin, vMin} },
            { {1.0f, 1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {uMax, vMin} },
            { {-1.0f, -1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {uMin, vMax} },
            { {1.0f, 1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {uMax, vMin} },
            { {1.0f, -1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {uMax, vMax} },
            { {-1.0f, -1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {uMin, vMax} }
        };

        D3D11_MAPPED_SUBRESOURCE ms;
        m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, bgVertices.data(), bgVertices.size() * sizeof(Vertex));
        m_context->Unmap(m_vertexBuffer, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        m_context->IASetInputLayout(m_inputLayout);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_vertexShader, NULL, 0);
        m_context->PSSetShader(m_pixelShader, NULL, 0);
        m_context->PSSetShaderResources(0, 1, &m_backgroundSRV);

        m_context->Draw(bgVertices.size(), 0);
        
        // Unbind SRV
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
    }

    // FPS Calculation
    m_frameCount++;
    m_timeElapsed += deltaTime;
    if (m_timeElapsed >= 1.0f) {
        m_fps = (float)m_frameCount / m_timeElapsed;
        m_frameCount = 0;
        m_timeElapsed = 0.0f;
    }

    // Update current visualization
    const AudioData& audioData = m_audioEngine.GetData();
    int visIndex = (int)m_currentVis;
    if (visIndex >= 0 && visIndex < 5 && m_visualizations[visIndex]) {
        m_visualizations[visIndex]->Update(deltaTime, audioData, m_useNormalized,
                                          m_vertexBuffer, m_inputLayout,
                                          m_vertexShader, m_pixelShader);
    }
    
    RenderOSD();

    m_swapChain->Present(1, 0);
}

void Renderer::CreateTextResources() {
    // Create a texture for text rendering (GDI compatible)
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = 1024;
    desc.Height = 1024;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // GDI compatible format
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

    m_device->CreateTexture2D(&desc, NULL, &m_textTexture);
    m_device->CreateShaderResourceView(m_textTexture, NULL, &m_textSRV);
}

void Renderer::CreateClockResources() {
    // Create texture for clock rendering (256x256 for clock display)
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = 256;
    desc.Height = 256;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // GDI compatible format
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

    m_device->CreateTexture2D(&desc, NULL, &m_clockTexture);
    m_device->CreateShaderResourceView(m_clockTexture, NULL, &m_clockSRV);

    // Initialize with empty clock text
    UpdateClockTexture("");
}

void Renderer::UpdateClockTexture(const std::string& text) {
    if (!m_clockTexture) return;

    // Get texture from D3D11 (use IDXGISurface1 for GetDC support)
    IDXGISurface1* pSurface = nullptr;
    m_clockTexture->QueryInterface(__uuidof(IDXGISurface1), (void**)&pSurface);
    if (!pSurface) return;

    HDC hdc = nullptr;
    pSurface->GetDC(FALSE, &hdc);
    if (hdc) {
        // Clear to transparent black
        RECT rect = {0, 0, 256, 256};
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

        // Create larger font for clock (48pt)
        HFONT hFont = CreateFontA(
            48,                       // Height (larger for clock)
            0,                        // Width
            0,                        // Escapement
            0,                        // Orientation
            FW_BOLD,                  // Weight (bold)
            FALSE,                    // Italic
            FALSE,                    // Underline
            FALSE,                    // StrikeOut
            DEFAULT_CHARSET,          // CharSet
            OUT_DEFAULT_PRECIS,       // OutputPrecision
            CLIP_DEFAULT_PRECIS,      // ClipPrecision
            CLEARTYPE_QUALITY,        // Quality
            DEFAULT_PITCH | FF_DONTCARE, // PitchAndFamily
            "Consolas");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);

        // Draw text right-aligned
        DrawTextA(hdc, text.c_str(), -1, &rect, DT_RIGHT | DT_TOP | DT_NOCLIP);

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);

        pSurface->ReleaseDC(nullptr);
    }
    pSurface->Release();
}

void Renderer::UpdateTextTexture(const std::string& text, bool rightAlign) {
    if (!m_textTexture) return;

    IDXGISurface1* pSurface = nullptr;
    m_textTexture->QueryInterface(__uuidof(IDXGISurface1), (void**)&pSurface);
    
    if (pSurface) {
        HDC hdc;
        pSurface->GetDC(FALSE, &hdc);

        // 1. Clear background to BLACK (Transparent in our shader logic)
        RECT fullRect = {0, 0, 1024, 1024};
        HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &fullRect, hBlackBrush);
        DeleteObject(hBlackBrush);

        // 2. Setup Font (slightly smaller to fit more text)
        HFONT hFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Comic Sans MS");
        SelectObject(hdc, hFont);

        // 3. Measure Text
        RECT textRect = {0, 0, 1024, 1024};
        DrawText(hdc, text.c_str(), -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
        
        // 4. Calculate Box Size (25% wider and taller to ensure no cutoff)
        int textWidth = textRect.right - textRect.left;
        int textHeight = textRect.bottom - textRect.top;
        int boxWidth = (int)(textWidth * 1.25f);
        int boxHeight = (int)(textHeight * 1.25f);
        
        // Ensure box doesn't exceed texture bounds
        if (boxWidth > 1004) boxWidth = 1004;  // Leave 20px total padding
        if (boxHeight > 1004) boxHeight = 1004;
        
        // 5. Position Box
        RECT boxRect;
        if (rightAlign) {
            boxRect.right = 1024 - 10; // 10px padding from edge
            boxRect.left = boxRect.right - boxWidth;
        } else {
            boxRect.left = 10;
            boxRect.right = boxRect.left + boxWidth;
        }
        boxRect.top = 10;
        boxRect.bottom = boxRect.top + boxHeight;

        // 6. Draw Tinted Box
        // Using RGB(40,40,40) creates a dark semi-transparent box with the current shader
        HBRUSH hBoxBrush = CreateSolidBrush(RGB(40, 40, 40)); 
        FillRect(hdc, &boxRect, hBoxBrush);
        DeleteObject(hBoxBrush);

        // 7. Draw Text Centered in Box
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        // Adjust rect for text drawing to be centered
        // DrawText with DT_VCENTER only works for single line, so we manually offset
        int paddingX = (boxWidth - textWidth) / 2;
        int paddingY = (boxHeight - textHeight) / 2;
        RECT drawRect = boxRect;
        drawRect.left += paddingX;
        drawRect.top += paddingY;
        
        DrawText(hdc, text.c_str(), -1, &drawRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        
        DeleteObject(hFont);
        pSurface->ReleaseDC(nullptr);
        pSurface->Release();
    }
}

void Renderer::RenderOSD() {
    // Render clock first if enabled (independent of other overlays)
    if (m_showClock) {
        RenderClock();
    }
    
    if (!m_showHelp && !m_showInfo && !m_showDisableMenu) return;

    std::string osdText;
    bool rightAlign = false;

    if (m_showDisableMenu) {
        osdText = "VISUALIZATION MENU:\n\n";
        osdText += "Press number to toggle:\n\n";
        
        for (int i = 0; i < 4; i++) {
            osdText += std::to_string(i + 1) + ": " + GetVisualizationName(i);
            osdText += (m_config.visEnabled[i] ? " [ENABLED]\n" : " [DISABLED]\n");
        }
        
        osdText += "\nD: Close Menu\n";
        osdText += "X: Reset All Settings";
    } else if (m_showHelp) {
        osdText = "HELP (UNIVERSAL CONTROLS):\n\n"
                  "H: Toggle Help\n"
                  "I: Toggle Info (Vis Settings)\n"
                  "C: Toggle Clock\n"
                  "D: Disable Menu\n"
                  "X: Reset All Settings\n"
                  "F: Toggle Fullscreen\n"
                  "B: Random Background\n"
                  "[/]: Prev/Next Background\n"
                  "1-5: Jump to Vis\n"
                  "Left/Right: Change Vis\n"
                  "R: Random Vis\n"
                  "ESC: Quit\n\n"
                  "Press I to see current\n"
                  "visualization settings";
    } else if (m_showInfo) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "INFO: " << GetVisualizationName((int)m_currentVis) << "\n\n";
        ss << "FPS: " << m_fps << "\n";
        ss << "Audio Scale: " << m_audioEngine.GetData().Scale << "\n";
        ss << "Playing: " << (m_audioEngine.GetData().playing ? "Yes" : "No") << "\n\n";
        
        // Show visualization-specific settings and controls
        if (m_currentVis == Visualization::Spectrum) {
            ss << "SETTINGS:\n";
            ss << "Decay Rate: " << m_config.spectrumDecayRate << "\n";
            ss << "  -/=: Adjust Decay\n";
        } else if (m_currentVis == Visualization::CyberValley2) {
            ss << "SETTINGS:\n";
            ss << "Speed: " << m_config.cv2Speed << "\n";
            ss << "  -/=: Adjust Speed\n";
            ss << "Sun/Moon: " << (m_config.cv2SunMode ? "Sun" : "Moon") << "\n";
            ss << "  V: Toggle Sun/Moon\n";
            ss << "Grid: " << (m_config.cv2ShowGrid ? "On" : "Off") << "\n";
            ss << "  G: Toggle Grid\n";
        } else if (m_currentVis == Visualization::LineFader) {
            ss << "SETTINGS:\n";
            ss << "Scroll Speed: " << m_config.lfScrollSpeed << "\n";
            ss << "  -/=: Adjust Speed\n";
            ss << "Fade Rate: " << (m_config.lfFadeRate * 100.0f) << "%\n";
            ss << "  ,/.: Adjust Fade\n";
            const char* mirrorModes[] = {"None", "Horizontal", "Vertical"};
            ss << "Mirror: " << mirrorModes[m_config.lfMirrorMode] << "\n";
            ss << "  M: Cycle Mirror\n";
        } else if (m_currentVis == Visualization::Spectrum2) {
            ss << "SETTINGS:\n";
            ss << "Decay Rate: " << m_config.s2DecayRate << "\n";
            ss << "  -/=: Adjust Decay\n";
            const char* mirrorModes[] = {"None", "Horizontal", "Vertical"};
            ss << "Mirror: " << mirrorModes[m_config.s2MirrorMode] << "\n";
            ss << "  M: Cycle Mirror\n";
        } else if (m_currentVis == Visualization::Circle) {
            ss << "SETTINGS:\n";
            ss << "Rotation: " << m_config.circleRotationSpeed << "\n";
            ss << "  K/L: Adjust Rotation\n";
            ss << "Fade: " << m_config.circleFadeRate << "%\n";
            ss << "  ,/.: Adjust Fade\n";
            ss << "Zoom: " << m_config.circleZoomRate << "%\n";
            ss << "  -/=: Adjust Zoom\n";
            ss << "Blur: " << m_config.circleBlurRate << "%\n";
            ss << "  ;/': Adjust Blur\n";
            const char* peakModes[] = {"Inside", "Outside", "Both"};
            ss << "Peak: " << peakModes[m_config.circlePeakMode] << "\n";
            ss << "  M: Toggle Peak\n";
            ss << "Zoom Dir: " << (m_config.circleZoomOut ? "Out" : "In") << "\n";
            ss << "  Z: Toggle Direction\n";
            ss << "Fill: " << (m_config.circleFillMode ? "On" : "Off") << "\n";
            ss << "  P: Toggle Fill\n";
        }
        osdText = ss.str();
    }

    UpdateTextTexture(osdText, rightAlign);

    // Render Quad with Text Texture
    std::vector<Vertex> vertices;
    float w = 0.8f; // Width relative to screen
    float h = 0.8f; // Height relative to screen
    float padding = 0.05f;
    float x = 1.0f - w - padding; // Top Right
    float y = 1.0f - padding;

    XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    
    vertices.push_back({ {x, y, 0.0f}, color, {0.0f, 0.0f} });
    vertices.push_back({ {x + w, y, 0.0f}, color, {1.0f, 0.0f} });
    vertices.push_back({ {x, y - h, 0.0f}, color, {0.0f, 1.0f} });

    vertices.push_back({ {x + w, y, 0.0f}, color, {1.0f, 0.0f} });
    vertices.push_back({ {x + w, y - h, 0.0f}, color, {1.0f, 1.0f} });
    vertices.push_back({ {x, y - h, 0.0f}, color, {0.0f, 1.0f} });

    D3D11_MAPPED_SUBRESOURCE ms;
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader, NULL, 0);
    m_context->PSSetShader(m_pixelShader, NULL, 0);
    m_context->PSSetShaderResources(0, 1, &m_textSRV);

    m_context->Draw(vertices.size(), 0);
    
    // Unbind SRV to allow update next frame
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

void Renderer::ScanBackgrounds() {
    namespace fs = std::filesystem;
    m_backgroundFiles.clear();
    std::string bgPath = "Backgrounds";
    
    if (!fs::exists(bgPath)) return;

    for (const auto& entry : fs::directory_iterator(bgPath)) {
        if (entry.is_regular_file()) {
            m_backgroundFiles.push_back(entry.path().wstring());
        }
    }
}

void Renderer::LoadBackground(int index) {
    if (m_backgroundFiles.empty()) return;
    if (index < 0 || index >= m_backgroundFiles.size()) return;

    m_currentBgIndex = index;
    std::wstring selectedFile = m_backgroundFiles[index];
    m_currentBgPath = selectedFile;

    // Load with GDI+
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(selectedFile.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        return;
    }

    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
    
    // Lock bits
    if (bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) == Gdiplus::Ok) {
        
        // Release old texture
        if (m_backgroundSRV) { m_backgroundSRV->Release(); m_backgroundSRV = nullptr; }
        if (m_backgroundTexture) { m_backgroundTexture->Release(); m_backgroundTexture = nullptr; }

        // Create D3D Texture
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = bitmap->GetWidth();
        desc.Height = bitmap->GetHeight();
        
        m_bgAspectRatio = (float)desc.Width / (float)desc.Height;

        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // GDI+ uses BGRA
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = bitmapData.Scan0;
        initData.SysMemPitch = bitmapData.Stride;

        HRESULT hr = m_device->CreateTexture2D(&desc, &initData, &m_backgroundTexture);
        if (SUCCEEDED(hr)) {
            hr = m_device->CreateShaderResourceView(m_backgroundTexture, NULL, &m_backgroundSRV);
        }

        bitmap->UnlockBits(&bitmapData);
    }
    delete bitmap;
}

void Renderer::LoadRandomBackground() {
    if (m_backgroundFiles.empty()) ScanBackgrounds();
    if (m_backgroundFiles.empty()) return;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    int newIndex = -1;
    int attempts = 0;
    do {
        std::uniform_int_distribution<> dis(0, m_backgroundFiles.size() - 1);
        newIndex = dis(gen);
        attempts++;
    } while (newIndex == m_currentBgIndex && m_backgroundFiles.size() > 1 && attempts < 10);

    LoadBackground(newIndex);
}

void Renderer::HandleInput(WPARAM key) {
    // First, try delegating to current visualization
    int visIndex = (int)m_currentVis;
    if (visIndex >= 0 && visIndex < 5 && m_visualizations[visIndex]) {
        m_visualizations[visIndex]->HandleInput(key);
        // If visualization handled it, we might need to save config
        SaveStateToConfig();
    }
    
    // Handle global/renderer-specific keys
    if (key == 'H') {
        m_showHelp = !m_showHelp;
        if (m_showHelp) {
            m_showInfo = false;
            m_showDisableMenu = false;
        }
    } else if (key == 'I') {
        m_showInfo = !m_showInfo;
        if (m_showInfo) { m_showHelp = false; m_showDisableMenu = false; }
    } else if (key == 'C') {
        m_showClock = !m_showClock;
        SaveStateToConfig();
    } else if (key == 'D') {
        m_showDisableMenu = !m_showDisableMenu;
        if (m_showDisableMenu) { m_showHelp = false; m_showInfo = false; m_showClock = false; }
    } else if (key == 'X') {
        // Reset all settings to defaults
        ResetToDefaults();
        m_showDisableMenu = false;
        m_showHelp = false;
        m_showInfo = false;
        m_showClock = false;
    } else if (key == 'F') {
        m_isFullscreen = !m_isFullscreen;
        m_swapChain->SetFullscreenState(m_isFullscreen, NULL);
        SaveStateToConfig();
    } else if (key == 'B') {
        // Always load new background, ensure it's shown
        m_showBackground = true;
        LoadRandomBackground();
        SaveStateToConfig();
    } else if (key == VK_OEM_4) { // '[' Key
        m_showBackground = true;
        if (m_backgroundFiles.empty()) ScanBackgrounds();
        if (!m_backgroundFiles.empty()) {
            m_currentBgIndex--;
            if (m_currentBgIndex < 0) m_currentBgIndex = m_backgroundFiles.size() - 1;
            LoadBackground(m_currentBgIndex);
            SaveStateToConfig();
        }
    } else if (key == VK_OEM_6) { // ']' Key
        m_showBackground = true;
        if (m_backgroundFiles.empty()) ScanBackgrounds();
        if (!m_backgroundFiles.empty()) {
            m_currentBgIndex++;
            if (m_currentBgIndex >= m_backgroundFiles.size()) m_currentBgIndex = 0;
            LoadBackground(m_currentBgIndex);
            SaveStateToConfig();
        }
    } else if (key == VK_LEFT) {
        if (m_showDisableMenu) {
            // Disable menu is showing, don't change vis
        } else {
            int nextVis = GetNextEnabledVis((int)m_currentVis, false);
            m_currentVis = (Visualization)nextVis;
            SaveStateToConfig();
        }
    } else if (key == VK_RIGHT) {
        if (m_showDisableMenu) {
            // Disable menu is showing, don't change vis
        } else {
            int nextVis = GetNextEnabledVis((int)m_currentVis, true);
            m_currentVis = (Visualization)nextVis;
            SaveStateToConfig();
        }
    } else if (key == '1') {
        if (m_showDisableMenu) {
            // Toggle Spectrum enabled/disabled
            m_config.visEnabled[0] = !m_config.visEnabled[0];
            m_config.isDirty = true;
        } else {
            if (m_config.visEnabled[0]) {
                m_currentVis = Visualization::Spectrum;
                SaveStateToConfig();
            }
        }
    } else if (key == '2') {
        if (m_showDisableMenu) {
            // Toggle CyberValley2 enabled/disabled
            m_config.visEnabled[1] = !m_config.visEnabled[1];
            m_config.isDirty = true;
        } else {
            if (m_config.visEnabled[1]) {
                m_currentVis = Visualization::CyberValley2;
                SaveStateToConfig();
            }
        }
    } else if (key == '3') {
        if (m_showDisableMenu) {
            // Toggle LineFader enabled/disabled
            m_config.visEnabled[2] = !m_config.visEnabled[2];
            m_config.isDirty = true;
        } else {
            if (m_config.visEnabled[2]) {
                m_currentVis = Visualization::LineFader;
                SaveStateToConfig();
            }
        }
    } else if (key == '4') {
        if (m_showDisableMenu) {
            // Toggle Spectrum2 enabled/disabled
            m_config.visEnabled[3] = !m_config.visEnabled[3];
            m_config.isDirty = true;
        } else {
            if (m_config.visEnabled[3]) {
                m_currentVis = Visualization::Spectrum2;
                SaveStateToConfig();
            }
        }
    } else if (key == '5') {
        if (m_showDisableMenu) {
            // Toggle Circle enabled/disabled
            m_config.visEnabled[4] = !m_config.visEnabled[4];
            m_config.isDirty = true;
        } else {
            if (m_config.visEnabled[4]) {
                m_currentVis = Visualization::Circle;
                SaveStateToConfig();
            }
        }
    } else if (key == 'R') {
        if (m_showDisableMenu) {
            // Disable menu is showing, don't do random
        } else {
            // Random enabled visualization
            std::vector<int> enabledVis;
            for (int i = 0; i < 5; i++) {
                if (m_config.visEnabled[i]) enabledVis.push_back(i);
            }
            if (!enabledVis.empty()) {
                int randomIndex = rand() % enabledVis.size();
                m_currentVis = (Visualization)enabledVis[randomIndex];
                SaveStateToConfig();
            }
        }
    } else if (key >= '0' && key <= '9') {
        // TODO: Switch to visualization index
    } else if (key == VK_ESCAPE) {
        PostQuitMessage(0);
    }
}

std::string Renderer::GetVisualizationName(int vis) {
    switch (vis) {
        case 0: return "Spectrum";
        case 1: return "CyberValley2";
        case 2: return "LineFader";
        case 3: return "Spectrum2";
        case 4: return "Circle";
        default: return "Unknown";
    }
}

int Renderer::GetNextEnabledVis(int currentVis, bool forward) {
    int numVis = 5;
    int nextVis = currentVis;
    int attempts = 0;
    
    do {
        if (forward) {
            nextVis++;
            if (nextVis >= numVis) nextVis = 0;
        } else {
            nextVis--;
            if (nextVis < 0) nextVis = numVis - 1;
        }
        
        attempts++;
        if (attempts >= numVis) {
            // All disabled, stay on current
            return currentVis;
        }
    } while (!m_config.visEnabled[nextVis]);
    
    return nextVis;
}

void Renderer::LoadConfigIntoState() {
    m_useNormalized = m_config.useNormalized;
    m_isFullscreen = m_isFullscreen;
    m_showBackground = m_config.showBackground;
    m_showClock = m_config.clockEnabled;
    m_currentBgIndex = m_config.currentBgIndex;
    m_currentBgPath = m_config.currentBgPath;
    m_currentVis = (Visualization)m_config.currentVis;
    
    // Load visualization states
    for (int i = 0; i < 5; i++) {
        if (m_visualizations[i]) {
            m_visualizations[i]->LoadState(m_config, i);
        }
    }
    
    // Reload background if one was set
    if (m_showBackground && m_currentBgIndex >= 0) {
        if (m_backgroundFiles.empty()) ScanBackgrounds();
        if (m_currentBgIndex < m_backgroundFiles.size()) {
            LoadBackground(m_currentBgIndex);
        }
    }
}

void Renderer::SaveStateToConfig() {
    m_config.useNormalized = m_useNormalized;
    m_config.isFullscreen = m_isFullscreen;
    m_config.showBackground = m_showBackground;
    m_config.clockEnabled = m_showClock;
    m_config.currentBgIndex = m_currentBgIndex;
    m_config.currentBgPath = m_currentBgPath;
    m_config.currentVis = (int)m_currentVis;
    
    // Save visualization states
    for (int i = 0; i < 5; i++) {
        if (m_visualizations[i]) {
            m_visualizations[i]->SaveState(m_config, i);
        }
    }
    
    m_config.isDirty = true;
}

void Renderer::ResetToDefaults() {
    m_config.Reset();
    LoadConfigIntoState();
    
    // Reset all visualizations to defaults
    for (int i = 0; i < 5; i++) {
        if (m_visualizations[i]) {
            m_visualizations[i]->ResetToDefaults();
        }
    }
    
    std::cout << "All settings reset to defaults" << std::endl;
}

void Renderer::RenderClock() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << ltm->tm_hour << ":"
       << std::setfill('0') << std::setw(2) << ltm->tm_min << ":"
       << std::setfill('0') << std::setw(2) << ltm->tm_sec << "\n";
       
    char buffer[80];
    strftime(buffer, 80, "%d/%m/%Y", ltm);
    ss << buffer;
    ss << "\n";
    
    char dayBuffer[80];
    strftime(dayBuffer, 80, "%A", ltm);
    ss << dayBuffer;
    
    std::string clockText = ss.str();
    
    // Update clock-specific texture with larger font
    UpdateClockTexture(clockText);

    // Clock dimensions (smaller dedicated area, top-right)
    float clockWidth = 0.25f;   // Narrower than OSD
    float clockHeight = 0.15f;  // Shorter than OSD
    float padding = 0.02f;
    
    // Position in top-right corner
    float x = 1.0f - clockWidth - padding;
    float y = 1.0f - padding;

    // Draw semi-transparent black background (50% opacity, 20% larger)
    float bgPadding = 0.2f; // 20% larger
    float bgWidth = clockWidth * (1.0f + bgPadding);
    float bgHeight = clockHeight * (1.0f + bgPadding);
    float bgX = x - (clockWidth * bgPadding * 0.5f);
    float bgY = y + (clockHeight * bgPadding * 0.5f);
    
    std::vector<Vertex> bgVertices;
    XMFLOAT4 bgColor = {0.0f, 0.0f, 0.0f, 0.5f}; // Black, 50% transparent
    
    bgVertices.push_back({ {bgX, bgY, 0.0f}, bgColor, {-1.0f, -1.0f} });
    bgVertices.push_back({ {bgX + bgWidth, bgY, 0.0f}, bgColor, {-1.0f, -1.0f} });
    bgVertices.push_back({ {bgX, bgY - bgHeight, 0.0f}, bgColor, {-1.0f, -1.0f} });

    bgVertices.push_back({ {bgX + bgWidth, bgY, 0.0f}, bgColor, {-1.0f, -1.0f} });
    bgVertices.push_back({ {bgX + bgWidth, bgY - bgHeight, 0.0f}, bgColor, {-1.0f, -1.0f} });
    bgVertices.push_back({ {bgX, bgY - bgHeight, 0.0f}, bgColor, {-1.0f, -1.0f} });

    D3D11_MAPPED_SUBRESOURCE ms;
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, bgVertices.data(), bgVertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader, NULL, 0);
    m_context->PSSetShader(m_pixelShader, NULL, 0);

    // Draw background (no texture)
    m_context->Draw(6, 0);

    // Now draw clock text on top
    std::vector<Vertex> textVertices;
    XMFLOAT4 textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    
    textVertices.push_back({ {x, y, 0.0f}, textColor, {0.0f, 0.0f} });
    textVertices.push_back({ {x + clockWidth, y, 0.0f}, textColor, {1.0f, 0.0f} });
    textVertices.push_back({ {x, y - clockHeight, 0.0f}, textColor, {0.0f, 1.0f} });

    textVertices.push_back({ {x + clockWidth, y, 0.0f}, textColor, {1.0f, 0.0f} });
    textVertices.push_back({ {x + clockWidth, y - clockHeight, 0.0f}, textColor, {1.0f, 1.0f} });
    textVertices.push_back({ {x, y - clockHeight, 0.0f}, textColor, {0.0f, 1.0f} });

    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, textVertices.data(), textVertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);

    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->PSSetShaderResources(0, 1, &m_clockSRV);

    m_context->Draw(6, 0);
    
    // Unbind texture
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}
