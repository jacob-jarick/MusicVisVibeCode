#include "Renderer.h"
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
    if (m_lfHistorySRV) m_lfHistorySRV->Release();
    if (m_lfHistoryRTV) m_lfHistoryRTV->Release();
    if (m_lfHistoryTexture) m_lfHistoryTexture->Release();
    if (m_lfTempSRV) m_lfTempSRV->Release();
    if (m_lfTempRTV) m_lfTempRTV->Release();
    if (m_lfTempTexture) m_lfTempTexture->Release();
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
    
    // Set starting visualization if specified
    if (startVis >= 0 && startVis <= 3) {
        m_currentVis = (Visualization)startVis;
    }

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

    // Load config and apply settings
    m_config.Load();
    LoadConfigIntoState();
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

    if (m_currentVis == Visualization::Spectrum) {
        UpdateSpectrumVis(deltaTime);
    } else if (m_currentVis == Visualization::CyberValley2) {
        UpdateCyberValley2Vis(deltaTime);
    } else if (m_currentVis == Visualization::LineFader) {
        UpdateLineFaderVis(deltaTime);
    } else if (m_currentVis == Visualization::Spectrum2) {
        UpdateSpectrum2Vis(deltaTime);
    } else if (m_currentVis == Visualization::Circle) {
        UpdateCircleVis(deltaTime);
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

        // 2. Setup Font
        HFONT hFont = CreateFont(56, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Comic Sans MS");
        SelectObject(hdc, hFont);

        // 3. Measure Text
        RECT textRect = {0, 0, 1024, 1024};
        DrawText(hdc, text.c_str(), -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
        
        // 4. Calculate Box Size (20% wider and taller)
        int textWidth = textRect.right - textRect.left;
        int textHeight = textRect.bottom - textRect.top;
        int boxWidth = (int)(textWidth * 1.2f);
        int boxHeight = (int)(textHeight * 1.2f);
        
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
        if (m_currentVis == Visualization::Spectrum) {
            osdText = "HELP (SPECTRUM):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "D: Disable Menu\n"
                      "X: Reset Settings\n"
                      "F: Toggle Fullscreen\n"
                      "B: Random Background\n"
                      "[/]: Prev/Next Background\n"
                      "-/=: Adjust Decay\n"
                      "1-4: Jump to Vis\n"
                      "Left/Right: Change Vis\n"
                      "R: Random Vis\n"
                      "ESC: Quit";
        } else if (m_currentVis == Visualization::CyberValley2) {
            osdText = "HELP (CYBER VALLEY 2):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "D: Disable Menu\n"
                      "X: Reset Settings\n"
                      "F: Toggle Fullscreen\n"
                      "1-4: Jump to Vis\n"
                      "Left/Right: Change Vis\n"
                      "R: Random Vis\n"
                      "V: Toggle Sun/Moon\n"
                      "G: Toggle Grid\n"
                      "-/=: Adjust Speed\n"
                      "ESC: Quit";
        } else if (m_currentVis == Visualization::LineFader) {
            osdText = "HELP (LINE FADER):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "D: Disable Menu\n"
                      "X: Reset Settings\n"
                      "F: Toggle Fullscreen\n"
                      "B: Random Background\n"
                      "[/]: Prev/Next Background\n"
                      ",/.: Adjust Fade Rate\n"
                      "-/=: Adjust Scroll Speed\n"
                      "M: Cycle Mirror Mode\n"
                      "1-5: Jump to Vis\n"
                      "Left/Right: Change Vis\n"
                      "ESC: Quit";
        } else if (m_currentVis == Visualization::Spectrum2) {
            osdText = "HELP (SPECTRUM 2):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "D: Disable Menu\n"
                      "X: Reset Settings\n"
                      "F: Toggle Fullscreen\n"
                      "B: Random Background\n"
                      "[/]: Prev/Next Background\n"
                      "-/=: Adjust Decay\n"
                      "M: Cycle Mirror Mode\n"
                      "1-5: Jump to Vis\n"
                      "Left/Right: Change Vis\n"
                      "R: Random Vis\n"
                      "ESC: Quit";
        } else if (m_currentVis == Visualization::Circle) {
            osdText = "HELP (CIRCLE):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "D: Disable Menu\n"
                      "X: Reset Settings\n"
                      "F: Toggle Fullscreen\n"
                      "B: Random Background\n"
                      "[/]: Prev/Next Background\n"
                      ",/.: Adjust Fade %\n"
                      "-/=: Adjust Zoom-out %\n"
                      ";/': Adjust Blur %\n"
                      "K/L: Adjust Rotation Speed\n"
                      "M: Toggle Peaks Inside/Outside\n"
                      "1-5: Jump to Vis\n"
                      "Left/Right: Change Vis\n"
                      "R: Random Vis\n"
                      "ESC: Quit";
        }
    } else if (m_showInfo) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "INFO OVERLAY:\n\n";
        ss << "FPS: " << m_fps << "\n";
        if (m_currentVis == Visualization::Spectrum) {
            ss << "Decay Rate: " << m_decayRate << "\n";
        } else if (m_currentVis == Visualization::CyberValley2) {
            ss << "Speed: " << m_cv2Speed << "\n";
            ss << "Mode: " << (m_cv2SunMode ? "Day (Sun)" : "Night (Moon)") << "\n";
        } else if (m_currentVis == Visualization::LineFader) {
            ss << "Scroll Speed: " << m_lfScrollSpeed << " px\n";
            ss << "Fade Rate: " << std::fixed << std::setprecision(2) << (m_lfFadeRate * 100.0f) << "%\n";
            if (m_lfMirrorMode == LFMirrorMode::None) {
                ss << "Mirror: None\n";
            } else if (m_lfMirrorMode == LFMirrorMode::BassEdges) {
                ss << "Mirror: Bass at Edges\n";
            } else {
                ss << "Mirror: Bass in Center\n";
            }
        } else if (m_currentVis == Visualization::Spectrum2) {
            ss << "Decay Rate: " << m_s2DecayRate << "\n";
            if (m_s2MirrorMode == S2MirrorMode::None) {
                ss << "Mirror: None\n";
            } else if (m_s2MirrorMode == S2MirrorMode::BassEdges) {
                ss << "Mirror: Bass at Edges\n";
            } else {
                ss << "Mirror: Bass in Center\n";
            }
        } else if (m_currentVis == Visualization::Circle) {
            ss << "Peaks: " << (m_circlePeaksInside ? "Inside" : "Outside") << "\n";
            ss << "Fade: " << m_circleFadeRate << "%\n";
            ss << "Zoom: " << m_circleZoomRate << "%\n";
            ss << "Blur: " << m_circleBlurRate << "%\n";
            ss << "Rotation: " << m_circleRotationSpeed << " deg\n";
        }
        ss << "Audio Scale: " << m_audioEngine.GetData().Scale << "\n";
        ss << "Playing: " << (m_audioEngine.GetData().playing ? "Yes" : "No");
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
    if (key == VK_OEM_COMMA) {  // ',' Key
        if (m_currentVis == Visualization::LineFader) {
            m_lfFadeRate = std::max(0.0005f, m_lfFadeRate - 0.0005f);  // Min 0.05%
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Circle) {
            m_circleFadeRate = std::max(0.0f, m_circleFadeRate - 0.05f);  // Min 0%
            SaveStateToConfig();
        }
    } else if (key == VK_OEM_PERIOD) {  // '.' Key
        if (m_currentVis == Visualization::LineFader) {
            m_lfFadeRate = std::min(0.005f, m_lfFadeRate + 0.0005f);  // Max 0.50%
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Circle) {
            m_circleFadeRate = std::min(5.0f, m_circleFadeRate + 0.05f);  // Max 5%
            SaveStateToConfig();
        }
    } else if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        if (m_currentVis == Visualization::Spectrum) {
            m_decayRate = std::max(0.1f, m_decayRate - 0.5f);
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::CyberValley2) {
            m_cv2Speed = std::max(5.0f, m_cv2Speed - 5.0f);  // Minimum 5% (very slow, 20s to horizon)
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::LineFader) {
            m_lfScrollSpeed = std::max(1, m_lfScrollSpeed - 1);  // Minimum 1 pixel
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Spectrum2) {
            m_s2DecayRate = std::max(0.1f, m_s2DecayRate - 0.5f);
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Circle) {
            m_circleZoomRate = std::max(0.0f, m_circleZoomRate - 0.05f);  // Min 0%
            SaveStateToConfig();
        }
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        if (m_currentVis == Visualization::Spectrum) {
            m_decayRate = std::min(20.0f, m_decayRate + 0.5f);
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::CyberValley2) {
            m_cv2Speed = std::min(200.0f, m_cv2Speed + 5.0f);  // Maximum 200% (very fast, 0.5s to horizon)
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::LineFader) {
            m_lfScrollSpeed = std::min(50, m_lfScrollSpeed + 1);  // Maximum 50 pixels
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Spectrum2) {
            m_s2DecayRate = std::min(20.0f, m_s2DecayRate + 0.5f);
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Circle) {
            m_circleZoomRate = std::min(5.0f, m_circleZoomRate + 0.05f);  // Max 5%
            SaveStateToConfig();
        }
    } else if (key == 'H') {
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
    } else if (key == 'V') {
        if (m_currentVis == Visualization::CyberValley2) {
            m_cv2SunMode = !m_cv2SunMode;
            SaveStateToConfig();
        }
    } else if (key == 'G') {
        if (m_currentVis == Visualization::CyberValley2) {
            m_cv2ShowGrid = !m_cv2ShowGrid;
            SaveStateToConfig();
        }
    } else if (key == 'M') {
        if (m_currentVis == Visualization::LineFader) {
            // Cycle through mirror modes
            if (m_lfMirrorMode == LFMirrorMode::None) {
                m_lfMirrorMode = LFMirrorMode::BassEdges;
            } else if (m_lfMirrorMode == LFMirrorMode::BassEdges) {
                m_lfMirrorMode = LFMirrorMode::BassCenter;
            } else {
                m_lfMirrorMode = LFMirrorMode::None;
            }
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Spectrum2) {
            // Cycle through mirror modes
            if (m_s2MirrorMode == S2MirrorMode::None) {
                m_s2MirrorMode = S2MirrorMode::BassEdges;
            } else if (m_s2MirrorMode == S2MirrorMode::BassEdges) {
                m_s2MirrorMode = S2MirrorMode::BassCenter;
            } else {
                m_s2MirrorMode = S2MirrorMode::None;
            }
            SaveStateToConfig();
        } else if (m_currentVis == Visualization::Circle) {
            // Toggle peaks inside/outside
            m_circlePeaksInside = !m_circlePeaksInside;
            SaveStateToConfig();
        }
    } else if (key == VK_OEM_1) {  // ';' Key
        if (m_currentVis == Visualization::Circle) {
            m_circleBlurRate = std::max(0.0f, m_circleBlurRate - 0.05f);  // Min 0%
            SaveStateToConfig();
        }
    } else if (key == VK_OEM_7) {  // '\'' Key
        if (m_currentVis == Visualization::Circle) {
            m_circleBlurRate = std::min(10.0f, m_circleBlurRate + 0.05f);  // Max 10%
            SaveStateToConfig();
        }
    } else if (key == 'K') {
        if (m_currentVis == Visualization::Circle) {
            m_circleRotationSpeed = std::max(-1.5f, m_circleRotationSpeed - 0.1f);  // Min -1.5 degrees
            SaveStateToConfig();
        }
    } else if (key == 'L') {
        if (m_currentVis == Visualization::Circle) {
            m_circleRotationSpeed = std::min(1.5f, m_circleRotationSpeed + 0.1f);  // Max 1.5 degrees
            SaveStateToConfig();
        }
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

void Renderer::UpdateSpectrumVis(float deltaTime) {
    const AudioData& data = m_audioEngine.GetData();
    
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
//     float deltaTime = (float)(currentTime.QuadPart - m_lastTime.QuadPart) / m_frequency.QuadPart;
//     m_lastTime = currentTime;

    // FPS Calculation
//     m_frameCount++;
//     m_timeElapsed += deltaTime;
    if (m_timeElapsed >= 1.0f) {
        m_fps = (float)m_frameCount / m_timeElapsed;
        m_frameCount = 0;
        m_timeElapsed = 0.0f;
    }

    std::vector<Vertex> vertices;

    // 16 bars
    float barWidth = 2.0f / 16.0f;
    float gap = 0.01f;
    
    for (int i = 0; i < 16; i++) {
        // Trim highest 32 frequencies (use bins 0-223 = 224 bins total)
        // Divide 224 bins into 16 buckets = 14 bins per bucket
        // Use MAX value from each bucket to set each bar
        float barValue = 0.0f;
        for (int j = 0; j < 14; j++) {
            int binIndex = i * 14 + j;
            if (binIndex < 224) {
                float val = m_useNormalized ? data.SpectrumNormalized[binIndex] : data.Spectrum[binIndex];
                if (val > barValue) barValue = val;
            }
        }
        
        // 16 segments per bar
        float currentHeightSegments = barValue * 16.0f;
        int numSegments = (int)currentHeightSegments;
        
        // Update Peak
        if (currentHeightSegments > m_peakLevels[i]) {
            m_peakLevels[i] = currentHeightSegments;
        } else {
            m_peakLevels[i] -= m_decayRate * deltaTime;
            if (m_peakLevels[i] < 0.0f) m_peakLevels[i] = 0.0f;
        }

        float x = -1.0f + i * barWidth + gap;
        float w = barWidth - 2 * gap;
        float h = 2.0f / 16.0f;
        float segGap = 0.005f;

        // Draw Segments
        for (int s = 0; s < numSegments; s++) {
            float y = -1.0f + s * h + segGap;
            float segH = h - 2 * segGap;

            // Color gradient
            XMFLOAT4 color;
            if (s < 8) color = {0.0f, 1.0f, 0.0f, 0.5f}; // Green, 50% alpha
            else if (s < 12) color = {1.0f, 1.0f, 0.0f, 0.5f}; // Yellow, 50% alpha
            else if (s < 14) color = {1.0f, 0.5f, 0.0f, 0.5f}; // Orange, 50% alpha
            else color = {1.0f, 0.0f, 0.0f, 0.5f}; // Red, 50% alpha

            // Quad
            vertices.push_back({ {x, y + segH, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w, y + segH, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x, y, 0.0f}, color, {-1.0f, -1.0f} });

            vertices.push_back({ {x + w, y + segH, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w, y, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x, y, 0.0f}, color, {-1.0f, -1.0f} });
        }

        // Draw Peak
        int peakSegment = (int)m_peakLevels[i];
        if (peakSegment >= 0 && peakSegment < 17) { // Allow going one above
             float y = -1.0f + peakSegment * h + segGap;
             float segH = h - 2 * segGap;
             XMFLOAT4 color = {1.0f, 0.0f, 0.0f, 0.5f}; // Red peak, 50% alpha

             vertices.push_back({ {x, y + segH, 0.0f}, color, {-1.0f, -1.0f} });
             vertices.push_back({ {x + w, y + segH, 0.0f}, color, {-1.0f, -1.0f} });
             vertices.push_back({ {x, y, 0.0f}, color, {-1.0f, -1.0f} });

             vertices.push_back({ {x + w, y + segH, 0.0f}, color, {-1.0f, -1.0f} });
             vertices.push_back({ {x + w, y, 0.0f}, color, {-1.0f, -1.0f} });
             vertices.push_back({ {x, y, 0.0f}, color, {-1.0f, -1.0f} });
        }
    }

    if (vertices.empty()) return;

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

    m_context->Draw(vertices.size(), 0);
}

void Renderer::UpdateCyberValley2Vis(float deltaTime) {
    const AudioData& data = m_audioEngine.GetData();
    std::vector<Vertex> vertices;
    
    // Constants
    const float HORIZON_Y = 0.2f;  // 40% from top (NDC: 1.0 is top, -1.0 is bottom, so 0.2 is 40% down)
    const int NUM_MOUNTAIN_POINTS = 112;  // Points per side of mountain (higher resolution)
    const int NUM_DEPTH_LINES = 60;      // Number of lines going toward horizon (more lines for smoother scrolling)
    const float MAX_HEIGHT = 0.9f;       // Maximum mountain height (increased 50% for more dramatic peaks)
    const int NUM_FREQ_BINS = 224;       // Number of frequency bins to use (256 - 32 high-end bins)
    
    // CRITICAL: Capture spectrum peaks into our frozen history buffer at controlled rate
    // Draw 30 mountain lines per second, using highest values since last draw
    m_cv2TimeSinceLastLine += deltaTime;
    const float LINE_DRAW_INTERVAL = 0.0333333f;  // ~0.033 seconds = 30 lines per second
    
    if (m_cv2TimeSinceLastLine >= LINE_DRAW_INTERVAL) {
        m_cv2TimeSinceLastLine -= LINE_DRAW_INTERVAL;
        
        // Use SpectrumHighestSample which already tracks peak values over last 6 frames
        for (int i = 0; i < 256; i++) {
            m_cv2MountainHistory[m_cv2HistoryWriteIndex][i] = data.SpectrumHighestSample[i];
        }
        m_cv2HistoryWriteIndex = (m_cv2HistoryWriteIndex + 1) % 60;
    }
    
    // Update timers
    m_cv2Time += deltaTime;
    if (m_cv2Time > 600.0f) m_cv2Time -= 600.0f;  // 10 minute cycle
    
    // Speed: controls how fast lines move toward horizon
    // Speed is percentage (50% = 2 seconds to horizon, 100% = 1 second)
    // Convert percentage to scroll speed: distance/second = speed / 100
    float scrollSpeed = m_cv2Speed / 100.0f;  // 50% -> 0.5 units/sec -> 2 seconds to horizon
    m_cv2GridOffset += scrollSpeed * deltaTime;
    if (m_cv2GridOffset > 1.0f) m_cv2GridOffset -= 1.0f;
    
    // Day/Night colors based on V key toggle
    XMFLOAT4 colorSkyTop, colorSkyBot, colorGround, colorGrid, colorSun;
    if (m_cv2SunMode) {
        // Day Palette - Sunset Orange / Neon Pink
        colorSkyTop = {1.0f, 0.55f, 0.0f, 1.0f};    // Sunset Orange #FF8C00
        colorSkyBot = {1.0f, 0.0f, 0.5f, 1.0f};     // Neon Pink #FF0080
        colorGround = {0.1f, 0.0f, 0.1f, 1.0f};     // Dark purple ground
        colorGrid = {1.0f, 0.0f, 0.8f, 1.0f};       // Magenta #FF00CC
        colorSun = {1.0f, 1.0f, 0.0f, 1.0f};        // Yellow Sun
    } else {
        // Night Palette - Dark Indigo / Cyber Purple
        colorSkyTop = {0.0f, 0.0f, 0.2f, 1.0f};     // Dark Indigo #000033
        colorSkyBot = {0.5f, 0.0f, 0.8f, 1.0f};     // Cyber Purple #8000CC
        colorGround = {0.0f, 0.05f, 0.1f, 1.0f};    // Dark blue ground
        colorGrid = {0.0f, 1.0f, 1.0f, 1.0f};       // Cyan #00FFFF
        colorSun = {0.8f, 0.8f, 1.0f, 1.0f};        // Pale Moon
    }
    
    // Helper to add a quad (two triangles)
    auto AddQuad = [&](XMFLOAT3 tl, XMFLOAT3 tr, XMFLOAT3 bl, XMFLOAT3 br, XMFLOAT4 colTop, XMFLOAT4 colBot) {
        vertices.push_back({ tl, colTop, {-1.0f, -1.0f} });
        vertices.push_back({ tr, colTop, {-1.0f, -1.0f} });
        vertices.push_back({ bl, colBot, {-1.0f, -1.0f} });
        vertices.push_back({ tr, colTop, {-1.0f, -1.0f} });
        vertices.push_back({ br, colBot, {-1.0f, -1.0f} });
        vertices.push_back({ bl, colBot, {-1.0f, -1.0f} });
    };
    
    // Helper to add a line (thin quad)
    auto AddLine = [&](float x1, float y1, float x2, float y2, XMFLOAT4 col, float thickness = 0.003f) {
        // Calculate perpendicular offset for line thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 0.0001f) return;
        float nx = -dy / len * thickness;
        float ny = dx / len * thickness;
        
        vertices.push_back({ {x1 - nx, y1 - ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 + nx, y1 + ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - nx, y2 - ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 + nx, y1 + ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 + nx, y2 + ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - nx, y2 - ny, 0.5f}, col, {-1.0f, -1.0f} });
    };
    
    // 1. DRAW SKY GRADIENT (from top to horizon)
    AddQuad(
        {-1.0f, 1.0f, 0.99f}, {1.0f, 1.0f, 0.99f},      // Top corners
        {-1.0f, HORIZON_Y, 0.99f}, {1.0f, HORIZON_Y, 0.99f},  // Horizon corners
        colorSkyTop, colorSkyBot
    );
    
    // 2. DRAW GROUND (from horizon to bottom) - solid dark color
    AddQuad(
        {-1.0f, HORIZON_Y, 0.98f}, {1.0f, HORIZON_Y, 0.98f},
        {-1.0f, -1.0f, 0.98f}, {1.0f, -1.0f, 0.98f},
        colorGround, colorGround
    );
    
    // 2a. ATMOSPHERE EFFECTS
    if (m_cv2SunMode) {
        // Day: Vaporwave clouds (drifting horizontally)
        int numClouds = 5;
        for (int c = 0; c < numClouds; c++) {
            float cloudSeed = (float)c * 123.456f;
            float cloudX = fmodf(cloudSeed + m_cv2Time * 0.05f, 2.0f) - 1.0f;  // Drift slowly
            float cloudY = HORIZON_Y + 0.15f + sinf(cloudSeed) * 0.15f;
            float cloudWidth = 0.2f + sinf(cloudSeed * 0.5f) * 0.1f;
            float cloudHeight = 0.05f;
            
            XMFLOAT4 cloudColor = {1.0f, 0.85f, 0.95f, 0.3f};  // Semi-transparent pink/white
            
            // Draw cloud as rounded ellipse (8 segments)
            for (int i = 0; i < 8; i++) {
                float angle1 = (float)i / 8.0f * 6.28318f;
                float angle2 = (float)(i + 1) / 8.0f * 6.28318f;
                float cx1 = cloudX + cosf(angle1) * cloudWidth;
                float cy1 = cloudY + sinf(angle1) * cloudHeight;
                float cx2 = cloudX + cosf(angle2) * cloudWidth;
                float cy2 = cloudY + sinf(angle2) * cloudHeight;
                
                vertices.push_back({ {cloudX, cloudY, 0.92f}, cloudColor, {-1.0f, -1.0f} });
                vertices.push_back({ {cx1, cy1, 0.92f}, cloudColor, {-1.0f, -1.0f} });
                vertices.push_back({ {cx2, cy2, 0.92f}, cloudColor, {-1.0f, -1.0f} });
            }
        }
    } else {
        // Night: Starfield effect - stars moving toward user (like mountains/road)
        int numStars = 80;
        for (int s = 0; s < numStars; s++) {
            float starSeed = (float)s * 0.123f;
            
            // Calculate star depth (0 = horizon, 1 = near camera)
            // Use grid offset to make stars scroll
            float starDepth = fmodf(starSeed * 10.0f + m_cv2GridOffset * 2.0f, 1.0f);
            
            // Perspective: stars at horizon are small/converged, stars near camera are large/spread
            float perspScale = starDepth;  // 0 at horizon, 1 at camera
            
            // Base position in "world space"
            float baseX = (sinf(starSeed * 100.0f) * 2.0f - 1.0f);
            float baseY = (sinf(starSeed * 200.0f) * 0.5f + 0.5f);  // 0-1 range
            
            // Apply perspective
            float starX = baseX * perspScale;
            float starY = HORIZON_Y + baseY * (1.0f - HORIZON_Y) * perspScale;
            
            // Only draw stars above horizon
            if (starY > HORIZON_Y) {
                float twinkle = 0.3f + 0.7f * (sinf(m_cv2Time * 3.0f + starSeed * 50.0f) * 0.5f + 0.5f);
                float starSize = 0.002f + 0.003f * perspScale;  // Larger when closer
                
                XMFLOAT4 starColor = {1.0f, 1.0f, 1.0f, twinkle * (0.3f + 0.7f * perspScale)};  // Brighter when closer
                
                // Draw star as small diamond
                vertices.push_back({ {starX, starY + starSize, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX - starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX + starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
                
                vertices.push_back({ {starX, starY - starSize, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX - starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX + starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
            }
        }
        
        // Shooting stars - also moving toward user in starfield
        float shootingStarPhase = fmodf(m_cv2Time, 3.0f);
        if (shootingStarPhase < 0.5f) {  // Active for 0.5 seconds
            float progress = shootingStarPhase / 0.5f;
            
            // Shooting star moves from horizon toward camera (depth increases)
            float shootDepth = progress;  // 0 at horizon, 1 at camera
            float perspScale = shootDepth;
            
            // Start position (varies with time for randomness)
            float baseX = sinf(floorf(m_cv2Time / 3.0f) * 12.34f) * 0.8f;
            float baseY = 0.6f + sinf(floorf(m_cv2Time / 3.0f) * 23.45f) * 0.3f;
            
            // Current position
            float shootX = baseX * perspScale;
            float shootY = HORIZON_Y + baseY * (1.0f - HORIZON_Y) * perspScale;
            
            // Previous position (slightly back in depth for trail)
            float prevDepth = shootDepth - 0.15f;
            if (prevDepth < 0.0f) prevDepth = 0.0f;
            float prevPerspScale = prevDepth;
            float tailX = baseX * prevPerspScale;
            float tailY = HORIZON_Y + baseY * (1.0f - HORIZON_Y) * prevPerspScale;
            
            // Only draw above horizon
            if (shootY > HORIZON_Y) {
                XMFLOAT4 shootColor = {1.0f, 1.0f, 0.8f, 1.0f};
                XMFLOAT4 tailColor = {1.0f, 1.0f, 0.8f, 0.4f};
                
                // Draw shooting star with trail following movement (from tail to head)
                AddLine(tailX, tailY, shootX, shootY, shootColor, 0.002f + 0.003f * perspScale);
            }
        }
    }
    
    // 3. DRAW SUN/MOON (static, centered above horizon)
    float sunX = 0.0f;
    float sunY = HORIZON_Y + 0.35f;  // Above horizon
    float sunRadius = 0.15f;
    
    // Draw glow halo (larger, semi-transparent)
    int sunSegments = 48;
    float glowRadius = sunRadius * 1.8f;
    XMFLOAT4 glowColor = {colorSun.x, colorSun.y, colorSun.z, 0.15f};
    for (int i = 0; i < sunSegments; i++) {
        float theta1 = (float)i / sunSegments * 6.28318f;
        float theta2 = (float)(i + 1) / sunSegments * 6.28318f;
        
        vertices.push_back({ {sunX, sunY, 0.91f}, glowColor, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta1) * glowRadius, sunY + sinf(theta1) * glowRadius, 0.91f}, glowColor, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta2) * glowRadius, sunY + sinf(theta2) * glowRadius, 0.91f}, glowColor, {-1.0f, -1.0f} });
    }
    
    // Draw main orb with gradient (center brighter)
    XMFLOAT4 centerColor = {colorSun.x * 1.2f, colorSun.y * 1.2f, colorSun.z * 1.2f, 1.0f};
    if (centerColor.x > 1.0f) centerColor.x = 1.0f;
    if (centerColor.y > 1.0f) centerColor.y = 1.0f;
    if (centerColor.z > 1.0f) centerColor.z = 1.0f;
    
    for (int i = 0; i < sunSegments; i++) {
        float theta1 = (float)i / sunSegments * 6.28318f;
        float theta2 = (float)(i + 1) / sunSegments * 6.28318f;
        
        vertices.push_back({ {sunX, sunY, 0.9f}, centerColor, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta1) * sunRadius, sunY + sinf(theta1) * sunRadius, 0.9f}, colorSun, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta2) * sunRadius, sunY + sinf(theta2) * sunRadius, 0.9f}, colorSun, {-1.0f, -1.0f} });
    }
    
    // 4. DRAW MOUNTAINS (audio-reactive valley)
    // For each depth row, we draw a flat spectrum line that forms the mountain profile
    // Lines closer to camera are at bottom, lines at horizon are at top
    
    float roadWidth = 0.15f;  // Half-width of road (matches road grid)
    
    for (int row = 0; row < NUM_DEPTH_LINES; row++) {
        // Calculate depth factor (0 = at camera/bottom, 1 = at horizon)
        float rawZ = (float)row / NUM_DEPTH_LINES;
        
        // Apply scrolling offset to position (makes lines move toward horizon at same speed as road)
        float z = rawZ + m_cv2GridOffset;
        if (z >= 1.0f) z -= 1.0f;
        
        // FREEZE LOGIC: History lookup based on Z position, not row number
        // z=0 (bottom) shows most recent spectrum, z=1.0 (horizon) shows oldest
        // This ensures newest data always appears at bottom regardless of scrolling
        int histOffset = (int)(z * 59.0f);  // Map z (0-1) to history depth (0-59)
        if (histOffset < 0) histOffset = 0;
        if (histOffset >= 60) histOffset = 59;
        int histIdx = (m_cv2HistoryWriteIndex - 1 - histOffset + 60) % 60;
        
        // Perspective: closer rows are spread wider, distant rows converge
        float perspectiveScale = 1.0f - z * 0.7f;  // 1.0 at camera, 0.3 at horizon (less aggressive pinch)
        
        // Calculate brightness fade: 100% at viewer (z=0), 33% at horizon (z=1)
        float brightness = 1.0f - z * 0.67f;
        XMFLOAT4 fadedColor = {colorGrid.x * brightness, colorGrid.y * brightness, colorGrid.z * brightness, colorGrid.w};
        
        // Y position: interpolate from bottom (-1) to horizon (0.2)
        float baseY = -1.0f + z * (HORIZON_Y + 1.0f);
        
        // Road edge at this depth
        float roadEdge = roadWidth * perspectiveScale;
        
        // Extend drawing range off-screen for closest lines (wider mountains at bottom)
        float extendFactor = 1.0f + (1.0f - z) * 0.2f;  // 1.2x at camera, 1.0x at horizon
        
        // Draw left mountain (from left screen edge to left road edge, extended off-screen when close)
        float prevX = -1.0f * perspectiveScale * extendFactor;
        float prevY = baseY;
        
        for (int i = 0; i <= NUM_MOUNTAIN_POINTS / 2; i++) {
            // X position from left screen edge to left road edge (extended range)
            float t = (float)i / (NUM_MOUNTAIN_POINTS / 2);
            float xScreen = -1.0f * perspectiveScale * extendFactor + t * (1.0f * perspectiveScale * extendFactor - roadEdge);
            
            // Map to frequency bin: bass at left edge, highs toward center
            // Left side: bins 0-111 (first half of 224 usable bins)
            int bin = (int)(t * 111.0f);
            if (bin < 0) bin = 0;
            if (bin > 111) bin = 111;
            
            // Get spectrum value from our frozen history buffer
            float specVal = m_cv2MountainHistory[histIdx][bin];
            
            // Calculate height (flat line, only audio modulation)
            float audioHeight = specVal * MAX_HEIGHT;
            float totalHeight = audioHeight * perspectiveScale;
            
            // Final Y position (flat horizontal line at baseY + height)
            float yScreen = baseY + totalHeight;
            
            // Draw line segment from previous point
            if (i > 0) {
                AddLine(prevX, prevY, xScreen, yScreen, fadedColor, 0.002f * perspectiveScale + 0.001f);
            }
            
            prevX = xScreen;
            prevY = yScreen;
        }
        
        // Draw right mountain (from right road edge to right screen edge, extended off-screen when close)
        prevX = roadEdge;
        prevY = baseY;
        
        for (int i = 0; i <= NUM_MOUNTAIN_POINTS / 2; i++) {
            // X position from right road edge to right screen edge (extended range)
            float t = (float)i / (NUM_MOUNTAIN_POINTS / 2);
            float xScreen = roadEdge + t * (1.0f * perspectiveScale * extendFactor - roadEdge);
            
            // Map to frequency bin: mirror left side (highs at center, bass at right edge)
            // Right side: bins 111-0 (mirrored, creates valley shape)
            int bin = 111 - (int)(t * 111.0f);
            if (bin < 0) bin = 0;
            if (bin > 111) bin = 111;
            
            // Get spectrum value from our frozen history buffer
            float specVal = m_cv2MountainHistory[histIdx][bin];
            
            // Calculate height (flat line, only audio modulation)
            float audioHeight = specVal * MAX_HEIGHT;
            float totalHeight = audioHeight * perspectiveScale;
            
            // Final Y position (flat horizontal line at baseY + height)
            float yScreen = baseY + totalHeight;
            
            // Draw line segment from previous point
            if (i > 0) {
                AddLine(prevX, prevY, xScreen, yScreen, fadedColor, 0.002f * perspectiveScale + 0.001f);
            }
            
            prevX = xScreen;
            prevY = yScreen;
        }
    }
    
    // 5. DRAW ROAD GRID (center path)
    if (m_cv2ShowGrid) {
        float roadWidth = 0.15f;  // Half-width of road at camera
        
        // Draw vertical lines (converging to center at horizon)
        int numRoadLines = 5;
        for (int i = 0; i < numRoadLines; i++) {
            float xOffset = (float)i / (numRoadLines - 1) * 2.0f - 1.0f;  // -1 to 1
            xOffset *= roadWidth;
            
            float xBottom = xOffset;
            float xTop = xOffset * 0.3f;  // Converge at horizon (matches mountain perspective: 1.0 - 1.0 * 0.7 = 0.3)
            
            AddLine(xBottom, -1.0f, xTop, HORIZON_Y, colorGrid, 0.002f);
        }
        
        // Draw horizontal lines (scrolling toward horizon)
        int numHorizLines = NUM_DEPTH_LINES / 4;  // 25% of mountain lines (15 lines instead of 60)
        for (int i = 0; i < numHorizLines; i++) {
            float z = (float)i / (float)numHorizLines;
            z += m_cv2GridOffset;
            if (z >= 1.0f) z -= 1.0f;
            
            float y = -1.0f + z * (HORIZON_Y + 1.0f);
            float perspScale = 1.0f - z * 0.7f;  // Match mountain perspective (0.3 at horizon)
            float xLeft = -roadWidth * perspScale;
            float xRight = roadWidth * perspScale;
            
            AddLine(xLeft, y, xRight, y, colorGrid, 0.002f * perspScale + 0.0005f);
        }
    }
    
    // Submit all vertices
    if (!vertices.empty()) {
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
        
        m_context->Draw((UINT)vertices.size(), 0);
    }
}

void Renderer::UpdateLineFaderVis(float deltaTime) {
    const AudioData& data = m_audioEngine.GetData();
    
    // Initialize textures if needed
    if (!m_lfHistoryTexture) {
        D3D11_TEXTURE2D_DESC desc = {0};
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        
        m_device->CreateTexture2D(&desc, NULL, &m_lfHistoryTexture);
        m_device->CreateShaderResourceView(m_lfHistoryTexture, NULL, &m_lfHistorySRV);
        m_device->CreateRenderTargetView(m_lfHistoryTexture, NULL, &m_lfHistoryRTV);
        
        m_device->CreateTexture2D(&desc, NULL, &m_lfTempTexture);
        m_device->CreateShaderResourceView(m_lfTempTexture, NULL, &m_lfTempSRV);
        m_device->CreateRenderTargetView(m_lfTempTexture, NULL, &m_lfTempRTV);
        
        // Clear both textures to black
        float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_context->ClearRenderTargetView(m_lfHistoryRTV, clearColor);
        m_context->ClearRenderTargetView(m_lfTempRTV, clearColor);
    }
    
    std::vector<Vertex> vertices;
    D3D11_MAPPED_SUBRESOURCE ms;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    XMFLOAT4 white = {1.0f, 1.0f, 1.0f, 1.0f};
    
    // Step 1: Render to temp texture - shift history up and fade
    m_context->OMSetRenderTargets(1, &m_lfTempRTV, NULL);
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(m_lfTempRTV, clearColor);
    
    // Calculate scroll offset in NDC
    float scrollOffsetNDC = (float)m_lfScrollSpeed / (float)m_height * 2.0f;
    
    // Draw existing history, shifted up
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, white, {0.0f, 0.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {-1.0f, -1.0f + scrollOffsetNDC, 0.0f}, white, {0.0f, 1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {1.0f, -1.0f + scrollOffsetNDC, 0.0f}, white, {1.0f, 1.0f} });
    vertices.push_back({ {-1.0f, -1.0f + scrollOffsetNDC, 0.0f}, white, {0.0f, 1.0f} });
    
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);
    
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader, NULL, 0);
    m_context->PSSetShader(m_pixelShader, NULL, 0);
    m_context->PSSetShaderResources(0, 1, &m_lfHistorySRV);
    m_context->Draw(6, 0);
    
    // Apply fade with semi-transparent black overlay
    XMFLOAT4 fadeOverlay = {0.0f, 0.0f, 0.0f, m_lfFadeRate};
    
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {1.0f, -1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);
    
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
    m_context->Draw(6, 0);
    
    // Step 2: Add new spectrum line at the bottom
    // Get smoothed spectrum data
    float smoothedSpectrum[256];
    for (int i = 0; i < 256; i++) {
        float val = m_useNormalized ? data.SpectrumNormalized[i] : data.Spectrum[i];
        
        // Apply smoothing (rolling average of 3)
        float prev = (i > 0) ? (m_useNormalized ? data.SpectrumNormalized[i-1] : data.Spectrum[i-1]) : val;
        float next = (i < 255) ? (m_useNormalized ? data.SpectrumNormalized[i+1] : data.Spectrum[i+1]) : val;
        smoothedSpectrum[i] = (prev + val + next) / 3.0f;
    }
    
    // Helper lambda to draw a line segment with lightning bolt style
    auto DrawLineSegment = [&](float x1, float y1, float x2, float y2) {
        XMFLOAT4 lightBlue = {0.4f, 0.7f, 1.0f, 1.0f};  // Light blue outline
        XMFLOAT4 whiteCore = {1.0f, 1.0f, 1.0f, 1.0f};  // White center
        
        float outerThickness = 0.004f;  // Outer blue line thickness
        float innerThickness = 0.002f;  // Inner white line thickness
        
        // Calculate perpendicular offset for line thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.0001f) return;
        
        float perpX = -dy / len;
        float perpY = dx / len;
        
        // Draw outer (light blue) line
        vertices.push_back({ {x1 + perpX * outerThickness, y1 + perpY * outerThickness, 0.0f}, lightBlue, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 + perpX * outerThickness, y2 + perpY * outerThickness, 0.0f}, lightBlue, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * outerThickness, y1 - perpY * outerThickness, 0.0f}, lightBlue, {-1.0f, -1.0f} });
        
        vertices.push_back({ {x2 + perpX * outerThickness, y2 + perpY * outerThickness, 0.0f}, lightBlue, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - perpX * outerThickness, y2 - perpY * outerThickness, 0.0f}, lightBlue, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * outerThickness, y1 - perpY * outerThickness, 0.0f}, lightBlue, {-1.0f, -1.0f} });
        
        // Draw inner (white) line
        vertices.push_back({ {x1 + perpX * innerThickness, y1 + perpY * innerThickness, 0.0f}, whiteCore, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 + perpX * innerThickness, y2 + perpY * innerThickness, 0.0f}, whiteCore, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * innerThickness, y1 - perpY * innerThickness, 0.0f}, whiteCore, {-1.0f, -1.0f} });
        
        vertices.push_back({ {x2 + perpX * innerThickness, y2 + perpY * innerThickness, 0.0f}, whiteCore, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - perpX * innerThickness, y2 - perpY * innerThickness, 0.0f}, whiteCore, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * innerThickness, y1 - perpY * innerThickness, 0.0f}, whiteCore, {-1.0f, -1.0f} });
    };
    
    vertices.clear();
    
    // Calculate y position at bottom of screen
    float baseY = -1.0f;
    float scaleHeight = 0.8f;  // Use 80% of screen height for spectrum
    
    // Draw based on mirror mode
    if (m_lfMirrorMode == LFMirrorMode::None) {
        // No mirror - full spectrum across screen (using first 224 bins, dropping high end)
        for (int i = 0; i < 223; i++) {
            float x1 = -1.0f + (float)i / 223.0f * 2.0f;
            float x2 = -1.0f + (float)(i + 1) / 223.0f * 2.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
    } else if (m_lfMirrorMode == LFMirrorMode::BassEdges) {
        // Bass at edges - mirror at center
        // Left half: normal spectrum (using first 224 bins)
        for (int i = 0; i < 223; i++) {
            float x1 = -1.0f + (float)i / 223.0f;
            float x2 = -1.0f + (float)(i + 1) / 223.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
        // Right half: flipped spectrum (using first 224 bins)
        for (int i = 0; i < 223; i++) {
            float x1 = 1.0f - (float)i / 223.0f;
            float x2 = 1.0f - (float)(i + 1) / 223.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
    } else {  // BassCenter
        // Bass in center - mirror at edges
        // Left half: flipped spectrum (using first 224 bins)
        for (int i = 0; i < 223; i++) {
            float x1 = 0.0f - (float)i / 223.0f;
            float x2 = 0.0f - (float)(i + 1) / 223.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
        // Right half: normal spectrum (using first 224 bins)
        for (int i = 0; i < 223; i++) {
            float x1 = 0.0f + (float)i / 223.0f;
            float x2 = 0.0f + (float)(i + 1) / 223.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
    }
    
    // Draw all line segments
    if (!vertices.empty()) {
        m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
        m_context->Unmap(m_vertexBuffer, 0);
        
        m_context->Draw((UINT)vertices.size(), 0);
    }
    
    // Step 3: Copy temp back to history for next frame
    ID3D11Resource* srcResource = nullptr;
    ID3D11Resource* dstResource = nullptr;
    m_lfTempTexture->QueryInterface(__uuidof(ID3D11Resource), (void**)&srcResource);
    m_lfHistoryTexture->QueryInterface(__uuidof(ID3D11Resource), (void**)&dstResource);
    m_context->CopyResource(dstResource, srcResource);
    srcResource->Release();
    dstResource->Release();
    
    // Step 4: Render final result to screen
    m_context->OMSetRenderTargets(1, &m_renderTargetView, NULL);
    
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, white, {0.0f, 0.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, white, {0.0f, 1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {1.0f, -1.0f, 0.0f}, white, {1.0f, 1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, white, {0.0f, 1.0f} });
    
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);
    
    m_context->PSSetShaderResources(0, 1, &m_lfHistorySRV);
    m_context->Draw(6, 0);
    
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

void Renderer::UpdateSpectrum2Vis(float deltaTime) {
    const AudioData& data = m_audioEngine.GetData();
    
    std::vector<Vertex> vertices;

    // 28 bars, 48 segments per bar
    const int numBars = 28;
    const int segmentsPerBar = 48;
    
    // Helper to calculate bar positions based on mirror mode
    auto GetBarPositions = [&](int barIndex, float& xStart, float& barWidth) {
        float totalWidth = 2.0f; // NDC space -1 to 1
        
        if (m_s2MirrorMode == S2MirrorMode::None) {
            // No mirror - 28 bars across full width
            barWidth = totalWidth / numBars;
            xStart = -1.0f + barIndex * barWidth;
        } else {
            // Mirror modes - 28 bars displayed (14 per side, using 14 data sources)
            barWidth = totalWidth / numBars;
            
            if (m_s2MirrorMode == S2MirrorMode::BassEdges) {
                // Bass at edges: bar 0 at left edge, bar 13 at center-left, bar 14 at right edge, bar 27 at center-right
                if (barIndex < 14) {
                    // Left side: bass (index 0) on left edge
                    xStart = -1.0f + barIndex * barWidth;
                } else {
                    // Right side: bass (bar 14, data 0) at right edge
                    xStart = 1.0f - (barIndex - 13) * barWidth;
                }
            } else { // BassCenter
                // Bass in center: bar 0 at center-left, bar 13 at left edge, bar 14 at center-right, bar 27 at right edge
                if (barIndex < 14) {
                    // Left side: bass (index 0) toward center
                    xStart = 0.0f - (barIndex + 1) * barWidth;
                } else {
                    // Right side: bass (index 14, using data 0) toward center
                    int mirrorIndex = barIndex - 14; // 14->0, 15->1, ..., 27->13
                    xStart = 0.0f + mirrorIndex * barWidth;
                }
            }
        }
    };
    
    float gap = 0.005f;
    
    for (int i = 0; i < numBars; i++) {
        // Determine data source index for mirror modes
        int dataIndex = i;
        if (m_s2MirrorMode != S2MirrorMode::None && i >= 14) {
            // Mirror modes: bars 14-27 use same data as bars 0-13
            dataIndex = i - 14;
        }
        
        // Data curation: Use bins 0-223 (224 bins), divided into 28 buckets = 8 bins per bucket
        // In mirror mode, only use first 14 buckets (112 bins)
        float barValue = 0.0f;
        int binsPerBucket = (m_s2MirrorMode == S2MirrorMode::None) ? 8 : 16;
        int maxBinIndex = (m_s2MirrorMode == S2MirrorMode::None) ? 224 : 224;
        
        for (int j = 0; j < binsPerBucket; j++) {
            int binIndex = dataIndex * binsPerBucket + j;
            if (binIndex < maxBinIndex) {
                float val = data.SpectrumNormalized[binIndex];
                if (val > barValue) barValue = val;
            }
        }
        
        // Scale to 48 segments
        float currentHeightSegments = barValue * segmentsPerBar;
        int numSegments = (int)currentHeightSegments;
        
        // Update Peak
        if (currentHeightSegments > m_s2PeakLevels[i]) {
            m_s2PeakLevels[i] = currentHeightSegments;
        } else {
            m_s2PeakLevels[i] -= m_s2DecayRate * deltaTime;
            if (m_s2PeakLevels[i] < 0.0f) m_s2PeakLevels[i] = 0.0f;
        }

        // Calculate bar position
        float xStart, barWidth;
        GetBarPositions(i, xStart, barWidth);
        
        float w = barWidth - 2 * gap;
        float h = 2.0f / segmentsPerBar;
        float segGap = 0.003f;

        // Draw Segments
        for (int s = 0; s < numSegments && s < segmentsPerBar; s++) {
            float y = -1.0f + s * h + segGap;
            float segH = h - 2 * segGap;

            // Smooth color gradient with 65% transparency (alpha = 0.35)
            XMFLOAT4 color;
            if (s < 12) {
                // Green zone (0-12)
                color = {0.0f, 1.0f, 0.0f, 0.35f};
            } else if (s < 24) {
                // Green to Yellow gradient (12-24)
                float t = (float)(s - 12) / 12.0f;
                color = {t, 1.0f, 0.0f, 0.35f};
            } else if (s < 36) {
                // Yellow to Orange gradient (24-36)
                float t = (float)(s - 24) / 12.0f;
                color = {1.0f, 1.0f - t * 0.5f, 0.0f, 0.35f};
            } else {
                // Orange to Red gradient (36-48)
                float t = (float)(s - 36) / 12.0f;
                color = {1.0f, 0.5f - t * 0.5f, 0.0f, 0.35f};
            }

            // Main segment (slightly rounded via multiple quads)
            float x = xStart + gap;
            
            // Center quad (main body)
            vertices.push_back({ {x + 0.002f, y + segH - 0.001f, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w - 0.002f, y + segH - 0.001f, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x + 0.002f, y + 0.001f, 0.0f}, color, {-1.0f, -1.0f} });
            
            vertices.push_back({ {x + w - 0.002f, y + segH - 0.001f, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w - 0.002f, y + 0.001f, 0.0f}, color, {-1.0f, -1.0f} });
            vertices.push_back({ {x + 0.002f, y + 0.001f, 0.0f}, color, {-1.0f, -1.0f} });
            
            // Darker border
            XMFLOAT4 borderColor = {color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, color.w};
            float borderThickness = 0.0008f;
            
            // Top border
            vertices.push_back({ {x, y + segH, 0.0f}, borderColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w, y + segH, 0.0f}, borderColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x, y + segH - borderThickness, 0.0f}, borderColor, {-1.0f, -1.0f} });
            
            vertices.push_back({ {x + w, y + segH, 0.0f}, borderColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w, y + segH - borderThickness, 0.0f}, borderColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x, y + segH - borderThickness, 0.0f}, borderColor, {-1.0f, -1.0f} });
        }

        // Draw Peak with 50% transparency
        int peakSegment = (int)m_s2PeakLevels[i];
        if (peakSegment >= 0 && peakSegment < segmentsPerBar + 1) {
            float y = -1.0f + peakSegment * h + segGap;
            float segH = h - 2 * segGap;
            float x = xStart + gap;
            XMFLOAT4 peakColor = {1.0f, 0.0f, 0.0f, 0.5f}; // Red peak, 50% alpha

            vertices.push_back({ {x, y + segH, 0.0f}, peakColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w, y + segH, 0.0f}, peakColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x, y, 0.0f}, peakColor, {-1.0f, -1.0f} });

            vertices.push_back({ {x + w, y + segH, 0.0f}, peakColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x + w, y, 0.0f}, peakColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x, y, 0.0f}, peakColor, {-1.0f, -1.0f} });
        }
    }

    if (vertices.empty()) return;

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

    m_context->Draw((UINT)vertices.size(), 0);
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
    m_isFullscreen = m_config.isFullscreen;
    m_showBackground = m_config.showBackground;
    m_showClock = m_config.clockEnabled;
    m_currentBgIndex = m_config.currentBgIndex;
    m_currentBgPath = m_config.currentBgPath;
    m_currentVis = (Visualization)m_config.currentVis;
    
    m_decayRate = m_config.spectrumDecayRate;
    
    m_cv2Time = m_config.cv2Time;
    m_cv2Speed = m_config.cv2Speed;
    m_cv2SunMode = m_config.cv2SunMode;
    m_cv2ShowGrid = m_config.cv2ShowGrid;
    
    m_lfScrollSpeed = m_config.lfScrollSpeed;
    m_lfFadeRate = m_config.lfFadeRate;
    m_lfMirrorMode = (LFMirrorMode)m_config.lfMirrorMode;
    
    m_s2DecayRate = m_config.s2DecayRate;
    m_s2MirrorMode = (S2MirrorMode)m_config.s2MirrorMode;
    
    m_circleRotationSpeed = m_config.circleRotationSpeed;
    m_circleFadeRate = m_config.circleFadeRate;
    m_circleZoomRate = m_config.circleZoomRate;
    m_circleBlurRate = m_config.circleBlurRate;
    m_circlePeaksInside = m_config.circlePeaksInside;
    
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
    
    m_config.spectrumDecayRate = m_decayRate;
    
    m_config.cv2Time = m_cv2Time;
    m_config.cv2Speed = m_cv2Speed;
    m_config.cv2SunMode = m_cv2SunMode;
    m_config.cv2ShowGrid = m_cv2ShowGrid;
    
    m_config.lfScrollSpeed = m_lfScrollSpeed;
    m_config.lfFadeRate = m_lfFadeRate;
    m_config.lfMirrorMode = (int)m_lfMirrorMode;
    
    m_config.s2DecayRate = m_s2DecayRate;
    m_config.s2MirrorMode = (int)m_s2MirrorMode;
    
    m_config.circleRotationSpeed = m_circleRotationSpeed;
    m_config.circleFadeRate = m_circleFadeRate;
    m_config.circleZoomRate = m_circleZoomRate;
    m_config.circleBlurRate = m_circleBlurRate;
    m_config.circlePeaksInside = m_circlePeaksInside;
    
    m_config.isDirty = true;
}

void Renderer::ResetToDefaults() {
    m_config.Reset();
    LoadConfigIntoState();
    
    // Reset visual states
    for (int i = 0; i < 16; i++) m_peakLevels[i] = 0.0f;
    for (int i = 0; i < 28; i++) m_s2PeakLevels[i] = 0.0f;
    m_cv2GridOffset = 0.0f;
    m_cv2HistoryWriteIndex = 0;
    m_cv2TimeSinceLastLine = 0.0f;
    
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
    
    // Create separate texture for clock (smaller than OSD)
    UpdateTextTexture(clockText, true);

    // Render clock in top-right corner (same size as help/info)
    std::vector<Vertex> vertices;
    float w = 0.8f; // Same width as help/info
    float h = 0.8f; // Same height as help/info
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

    m_context->Draw(6, 0);
    
    // Unbind texture
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

void Renderer::UpdateCircleVis(float deltaTime) {
    const AudioData& data = m_audioEngine.GetData();
    
    // Initialize textures if needed
    if (!m_circleHistoryTexture) {
        D3D11_TEXTURE2D_DESC desc = {0};
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        
        m_device->CreateTexture2D(&desc, NULL, &m_circleHistoryTexture);
        m_device->CreateShaderResourceView(m_circleHistoryTexture, NULL, &m_circleHistorySRV);
        m_device->CreateRenderTargetView(m_circleHistoryTexture, NULL, &m_circleHistoryRTV);
        
        m_device->CreateTexture2D(&desc, NULL, &m_circleTempTexture);
        m_device->CreateShaderResourceView(m_circleTempTexture, NULL, &m_circleTempSRV);
        m_device->CreateRenderTargetView(m_circleTempTexture, NULL, &m_circleTempRTV);
        
        // Clear ONLY on first frame - never again for tunnel effect
        float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_context->ClearRenderTargetView(m_circleHistoryRTV, clearColor);
        m_context->ClearRenderTargetView(m_circleTempRTV, clearColor);
    }
    
    std::vector<Vertex> vertices;
    D3D11_MAPPED_SUBRESOURCE ms;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    XMFLOAT4 white = {1.0f, 1.0f, 1.0f, 1.0f};
    
    // Step 1: Render previous frame to temp with zoom-out effect (RECURSIVE FEEDBACK)
    // DO NOT CLEAR - this is key for tunnel effect
    m_context->OMSetRenderTargets(1, &m_circleTempRTV, NULL);
    
    // Calculate zoom scale - values > 1.0 create outward tunnel, < 1.0 create inward tunnel
    // The zoom-out percentage expands the previous frame, pushing it toward edges
    float zoomScale = 1.0f + (m_circleZoomRate / 100.0f);
    
    // Calculate fade alpha - this dims the previous frame
    float fadeAlpha = 1.0f - (m_circleFadeRate / 100.0f);
    if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
    if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;
    
    XMFLOAT4 fadeColor = {fadeAlpha, fadeAlpha, fadeAlpha, 1.0f};
    
    // Draw previous frame zoomed out (tunnel effect)
    vertices.clear();
    vertices.push_back({ {-zoomScale, zoomScale, 0.0f}, fadeColor, {0.0f, 0.0f} });
    vertices.push_back({ {zoomScale, zoomScale, 0.0f}, fadeColor, {1.0f, 0.0f} });
    vertices.push_back({ {-zoomScale, -zoomScale, 0.0f}, fadeColor, {0.0f, 1.0f} });
    
    vertices.push_back({ {zoomScale, zoomScale, 0.0f}, fadeColor, {1.0f, 0.0f} });
    vertices.push_back({ {zoomScale, -zoomScale, 0.0f}, fadeColor, {1.0f, 1.0f} });
    vertices.push_back({ {-zoomScale, -zoomScale, 0.0f}, fadeColor, {0.0f, 1.0f} });
    
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);
    
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader, NULL, 0);
    m_context->PSSetShader(m_pixelShader, NULL, 0);
    m_context->PSSetShaderResources(0, 1, &m_circleHistorySRV);
    m_context->Draw(6, 0);
    
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
    
    // Step 2: Get smoothed spectrum data
    float smoothedSpectrum[256];
    for (int i = 0; i < 256; i++) {
        float val = m_useNormalized ? data.SpectrumNormalized[i] : data.Spectrum[i];
        
        // Apply smoothing (rolling average of 3)
        float prev = (i > 0) ? (m_useNormalized ? data.SpectrumNormalized[i-1] : data.Spectrum[i-1]) : val;
        float next = (i < 255) ? (m_useNormalized ? data.SpectrumNormalized[i+1] : data.Spectrum[i+1]) : val;
        smoothedSpectrum[i] = (prev + val + next) / 3.0f;
    }
    
    // Step 3: Update rotation
    m_circleRotation += m_circleRotationSpeed;
    if (m_circleRotation >= 360.0f) m_circleRotation -= 360.0f;
    if (m_circleRotation < 0.0f) m_circleRotation += 360.0f;
    
    // Step 4: Update hue for rainbow color cycling
    m_circleHue += 0.5f;  // Cycle through hues slowly
    if (m_circleHue >= 360.0f) m_circleHue -= 360.0f;
    
    // Convert HSV to RGB for current hue
    auto HSVtoRGB = [](float h, float s, float v) -> XMFLOAT4 {
        float c = v * s;
        float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float r, g, b;
        if (h < 60.0f) { r = c; g = x; b = 0; }
        else if (h < 120.0f) { r = x; g = c; b = 0; }
        else if (h < 180.0f) { r = 0; g = c; b = x; }
        else if (h < 240.0f) { r = 0; g = x; b = c; }
        else if (h < 300.0f) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        
        return XMFLOAT4{r + m, g + m, b + m, 1.0f};
    };
    
    XMFLOAT4 circleColor = HSVtoRGB(m_circleHue, 0.8f, 1.0f);
    
    // Step 5: Draw new circle
    vertices.clear();
    
    float centerX = 0.0f;
    float centerY = 0.0f;
    float baseRadius = 0.3f;  // Base radius of circle
    float maxAmplitude = 0.4f;  // Maximum amplitude for spectrum visualization
    
    // Create mirrored spectrum (ABCCBA pattern)
    // We'll use 128 samples, mirrored to create 256 total points
    int numSamples = 128;
    float angularStep = 360.0f / (numSamples * 2);  // Total 256 points around circle
    
    for (int i = 0; i < numSamples; i++) {
        // First half (A)
        float angle1 = m_circleRotation + i * angularStep * 2.0f;
        float angle2 = m_circleRotation + (i + 1) * angularStep * 2.0f;
        
        float amplitude1 = smoothedSpectrum[i * 2] * maxAmplitude;
        float amplitude2 = smoothedSpectrum[(i + 1) * 2] * maxAmplitude;
        
        float radius1, radius2;
        if (m_circlePeaksInside) {
            radius1 = baseRadius - amplitude1;
            radius2 = baseRadius - amplitude2;
        } else {
            radius1 = baseRadius + amplitude1;
            radius2 = baseRadius + amplitude2;
        }
        
        float radian1 = angle1 * 3.14159f / 180.0f;
        float radian2 = angle2 * 3.14159f / 180.0f;
        
        float x1 = centerX + cosf(radian1) * radius1;
        float y1 = centerY + sinf(radian1) * radius1;
        float x2 = centerX + cosf(radian2) * radius2;
        float y2 = centerY + sinf(radian2) * radius2;
        
        float x1b = centerX + cosf(radian1) * baseRadius;
        float y1b = centerY + sinf(radian1) * baseRadius;
        float x2b = centerX + cosf(radian2) * baseRadius;
        float y2b = centerY + sinf(radian2) * baseRadius;
        
        // Draw segment as triangle strip
        vertices.push_back({ {x1, y1, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
        
        vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
    }
    
    // Second half - mirrored (CCB)
    for (int i = numSamples - 1; i >= 0; i--) {
        float angle1 = m_circleRotation + (numSamples + (numSamples - 1 - i)) * angularStep * 2.0f;
        float angle2 = m_circleRotation + (numSamples + (numSamples - i)) * angularStep * 2.0f;
        
        float amplitude1 = smoothedSpectrum[i * 2] * maxAmplitude;
        float amplitude2 = smoothedSpectrum[((i > 0) ? i - 1 : 0) * 2] * maxAmplitude;
        
        float radius1, radius2;
        if (m_circlePeaksInside) {
            radius1 = baseRadius - amplitude1;
            radius2 = baseRadius - amplitude2;
        } else {
            radius1 = baseRadius + amplitude1;
            radius2 = baseRadius + amplitude2;
        }
        
        float radian1 = angle1 * 3.14159f / 180.0f;
        float radian2 = angle2 * 3.14159f / 180.0f;
        
        float x1 = centerX + cosf(radian1) * radius1;
        float y1 = centerY + sinf(radian1) * radius1;
        float x2 = centerX + cosf(radian2) * radius2;
        float y2 = centerY + sinf(radian2) * radius2;
        
        float x1b = centerX + cosf(radian1) * baseRadius;
        float y1b = centerY + sinf(radian1) * baseRadius;
        float x2b = centerX + cosf(radian2) * baseRadius;
        float y2b = centerY + sinf(radian2) * baseRadius;
        
        // Draw segment as triangle strip
        vertices.push_back({ {x1, y1, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
        
        vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
        vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
    }
    
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);
    
    m_context->Draw(vertices.size(), 0);
    
    // Step 6: Copy temp texture back to history texture for next frame (feedback loop!)
    m_context->CopyResource(m_circleHistoryTexture, m_circleTempTexture);
    
    // Step 7: Render final result to back buffer
    m_context->OMSetRenderTargets(1, &m_renderTargetView, NULL);
    
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, white, {0.0f, 0.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, white, {0.0f, 1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {1.0f, -1.0f, 0.0f}, white, {1.0f, 1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, white, {0.0f, 1.0f} });
    
    m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(m_vertexBuffer, 0);
    
    // Display the current temp texture (which has the new circle + zoomed history)
    m_context->PSSetShaderResources(0, 1, &m_circleTempSRV);
    m_context->Draw(6, 0);
    
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}
