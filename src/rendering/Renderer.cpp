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
    if (startVis >= 0 && startVis <= 2) {
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

    // Start with a random background
    m_showBackground = true;
    LoadRandomBackground();

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
            
            // Check timeout
            if (m_timeoutSeconds > 0.0f && m_runningTime >= m_timeoutSeconds) {
                std::cout << "Timeout reached (" << m_timeoutSeconds << "s), exiting..." << std::endl;
                PostQuitMessage(0);
            }
        }
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
    if (m_showBackground && m_backgroundSRV && (m_currentVis == Visualization::Spectrum || m_currentVis == Visualization::LineFader)) {
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
        HFONT hFont = CreateFont(56, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
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
    if (!m_showHelp && !m_showInfo && !m_showClock) return;

    std::string osdText;
    bool rightAlign = false;

    if (m_showHelp) {
        if (m_currentVis == Visualization::Spectrum) {
            osdText = "HELP (SPECTRUM):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "N: Toggle Normalized\n"
                      "F: Toggle Fullscreen\n"
                      "B: Random Background\n"
                      "[/]: Prev/Next Background\n"
                      "-/=: Adjust Decay\n"
                      "Left/Right: Change Vis\n"
                      "ESC: Quit";
        } else if (m_currentVis == Visualization::CyberValley2) {
            osdText = "HELP (CYBER VALLEY 2):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "F: Toggle Fullscreen\n"
                      "Left/Right: Change Vis\n"
                      "V: Toggle Sun/Moon\n"
                      "G: Toggle Grid\n"
                      "-/=: Adjust Speed\n"
                      "ESC: Quit";
        } else if (m_currentVis == Visualization::LineFader) {
            osdText = "HELP (LINE FADER):\n\n"
                      "H: Toggle Help\n"
                      "I: Toggle Info\n"
                      "C: Toggle Clock\n"
                      "N: Toggle Normalized\n"
                      "F: Toggle Fullscreen\n"
                      "B: Random Background\n"
                      "[/]: Prev/Next Background\n"
                      "-/=: Adjust Scroll Speed\n"
                      "M: Cycle Mirror Mode\n"
                      "Left/Right: Change Vis\n"
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
            if (m_lfMirrorMode == LFMirrorMode::None) {
                ss << "Mirror: None\n";
            } else if (m_lfMirrorMode == LFMirrorMode::BassEdges) {
                ss << "Mirror: Bass at Edges\n";
            } else {
                ss << "Mirror: Bass in Center\n";
            }
        }
        ss << "Audio Scale: " << m_audioEngine.GetData().Scale << "\n";
        ss << "Playing: " << (m_audioEngine.GetData().playing ? "Yes" : "No") << "\n";
        ss << "Normalized: " << (m_useNormalized ? "Yes" : "No");
        osdText = ss.str();
    } else if (m_showClock) {
        rightAlign = true;
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
    if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        if (m_currentVis == Visualization::Spectrum) {
            m_decayRate = std::max(0.1f, m_decayRate - 0.5f);
        } else if (m_currentVis == Visualization::CyberValley2) {
            m_cv2Speed = std::max(0.25f, m_cv2Speed - 0.25f);  // Minimum 0.25 lines/sec (4x slower)
        } else if (m_currentVis == Visualization::LineFader) {
            m_lfScrollSpeed = std::max(1, m_lfScrollSpeed - 1);  // Minimum 1 pixel
        }
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        if (m_currentVis == Visualization::Spectrum) {
            m_decayRate = std::min(20.0f, m_decayRate + 0.5f);
        } else if (m_currentVis == Visualization::CyberValley2) {
            m_cv2Speed = std::min(2.5f, m_cv2Speed + 0.25f);  // Maximum 2.5 lines/sec
        } else if (m_currentVis == Visualization::LineFader) {
            m_lfScrollSpeed = std::min(20, m_lfScrollSpeed + 1);  // Maximum 20 pixels
        }
    } else if (key == 'H') {
        m_showHelp = !m_showHelp;
        if (m_showHelp) {
            m_showInfo = false;
            m_showClock = false;
        }
    } else if (key == 'I') {
        m_showInfo = !m_showInfo;
        if (m_showInfo) { m_showHelp = false; m_showClock = false; }
    } else if (key == 'C') {
        m_showClock = !m_showClock;
        if (m_showClock) { m_showHelp = false; m_showInfo = false; }
    } else if (key == 'V') {
        if (m_currentVis == Visualization::CyberValley2) {
            m_cv2SunMode = !m_cv2SunMode;
        }
    } else if (key == 'G') {
        if (m_currentVis == Visualization::CyberValley2) {
            m_cv2ShowGrid = !m_cv2ShowGrid;
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
        }
    } else if (key == 'N') {
        m_useNormalized = !m_useNormalized;
    } else if (key == 'F') {
        m_isFullscreen = !m_isFullscreen;
        m_swapChain->SetFullscreenState(m_isFullscreen, NULL);
    } else if (key == 'B') {
        // Always load new background, ensure it's shown
        m_showBackground = true;
        LoadRandomBackground();
    } else if (key == VK_OEM_4) { // '[' Key
        m_showBackground = true;
        if (m_backgroundFiles.empty()) ScanBackgrounds();
        if (!m_backgroundFiles.empty()) {
            m_currentBgIndex--;
            if (m_currentBgIndex < 0) m_currentBgIndex = m_backgroundFiles.size() - 1;
            LoadBackground(m_currentBgIndex);
        }
    } else if (key == VK_OEM_6) { // ']' Key
        m_showBackground = true;
        if (m_backgroundFiles.empty()) ScanBackgrounds();
        if (!m_backgroundFiles.empty()) {
            m_currentBgIndex++;
            if (m_currentBgIndex >= m_backgroundFiles.size()) m_currentBgIndex = 0;
            LoadBackground(m_currentBgIndex);
        }
    } else if (key == VK_LEFT) {
        int vis = (int)m_currentVis - 1;
        if (vis < 0) vis = 2; // Wrap to last (3 visualizations: 0, 1, 2)
        m_currentVis = (Visualization)vis;
    } else if (key == VK_RIGHT) {
        int vis = (int)m_currentVis + 1;
        if (vis > 2) vis = 0; // Wrap to first
        m_currentVis = (Visualization)vis;
    } else if (key == '1') {
        m_currentVis = Visualization::Spectrum;
    } else if (key == '2') {
        m_currentVis = Visualization::CyberValley2;
    } else if (key == '3') {
        m_currentVis = Visualization::LineFader;
    } else if (key == 'R') {
        // Random visualization
        m_currentVis = (Visualization)(rand() % 3);
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
        // Map 16 bars to 256 bins (approx 16 bins per bar)
        // Use MAX instead of AVG to ensure peaks are visible and bars reach the top
        float barValue = 0.0f;
        for (int j = 0; j < 16; j++) {
            float val = m_useNormalized ? data.SpectrumNormalized[i * 16 + j] : data.Spectrum[i * 16 + j];
            if (val > barValue) barValue = val;
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
             XMFLOAT4 color = {1.0f, 0.0f, 0.0f, 1.0f}; // Red peak

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
    const int NUM_MOUNTAIN_POINTS = 32;  // Points per side of mountain (reduced for performance)
    const int NUM_DEPTH_LINES = 30;      // Number of lines going toward horizon (reduced for performance)
    const float MAX_HEIGHT = 1.2f;       // Maximum mountain height (doubled for more dramatic effect)
    const int NUM_FREQ_BINS = 32;        // Number of frequency bins to use (not grouped, individual)
    
    // Update timers
    m_cv2Time += deltaTime;
    if (m_cv2Time > 600.0f) m_cv2Time -= 600.0f;  // 10 minute cycle
    
    // Speed: controls how fast lines move toward horizon
    // Higher values = more lines drawn per second
    float linesPerSecond = m_cv2Speed;  // speed directly controls lines per second
    m_cv2GridOffset += linesPerSecond * deltaTime;
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
        
        // Get audio data based on ROW NUMBER, not scrolled position
        // This makes each row always look at the same point in history
        // Row 0 = current audio, Row 1 = 1 frame ago, Row 2 = 2 frames ago, etc.
        // Lines FREEZE their audio snapshot and only zoom toward horizon
        int histOffset = row;  // Direct row-to-history mapping
        int histIdx = (data.historyIndex - histOffset + 60) % 60;
        
        // Perspective: closer rows are spread wider, distant rows converge
        float perspectiveScale = 1.0f - z * 0.9f;  // 1.0 at camera, 0.1 at horizon
        
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
            
            // Map to frequency bin: use 32 DIRECT samples from spectrum (not grouped)
            // Left side: direct spectrum bins 0-31 (bass at outer edge)
            int bin = (int)(t * 31.0f);  // Direct mapping to first 32 bins
            if (bin < 0) bin = 0;
            if (bin > 31) bin = 31;
            
            // Get spectrum value from history
            float specVal = m_useNormalized ? data.HistoryNormalized[histIdx][bin] : data.History[histIdx][bin];
            
            // Calculate height (flat line, only audio modulation)
            float audioHeight = specVal * MAX_HEIGHT;
            float totalHeight = audioHeight * perspectiveScale;
            
            // Final Y position (flat horizontal line at baseY + height)
            float yScreen = baseY + totalHeight;
            
            // Draw line segment from previous point
            if (i > 0) {
                AddLine(prevX, prevY, xScreen, yScreen, colorGrid, 0.002f * perspectiveScale + 0.001f);
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
            
            // Map to frequency bin: use 32 DIRECT samples from spectrum (mirrored)
            // Right side: direct spectrum bins 31-0 (mirrored, bass at outer edge)
            int bin = 31 - (int)(t * 31.0f);  // Direct mapping, mirrored
            if (bin < 0) bin = 0;
            if (bin > 31) bin = 31;
            
            // Get spectrum value from history
            float specVal = m_useNormalized ? data.HistoryNormalized[histIdx][bin] : data.History[histIdx][bin];
            
            // Calculate height (flat line, only audio modulation)
            float audioHeight = specVal * MAX_HEIGHT;
            float totalHeight = audioHeight * perspectiveScale;
            
            // Final Y position (flat horizontal line at baseY + height)
            float yScreen = baseY + totalHeight;
            
            // Draw line segment from previous point
            if (i > 0) {
                AddLine(prevX, prevY, xScreen, yScreen, colorGrid, 0.002f * perspectiveScale + 0.001f);
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
            float xTop = xOffset * 0.1f;  // Converge at horizon
            
            AddLine(xBottom, -1.0f, xTop, HORIZON_Y, colorGrid, 0.002f);
        }
        
        // Draw horizontal lines (scrolling toward horizon)
        int numHorizLines = NUM_DEPTH_LINES;  // Match mountain line count for synchronized movement
        for (int i = 0; i < numHorizLines; i++) {
            float z = (float)i / numHorizLines;
            z += m_cv2GridOffset;
            if (z >= 1.0f) z -= 1.0f;
            
            float y = -1.0f + z * (HORIZON_Y + 1.0f);
            float perspScale = 1.0f - z * 0.9f;
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
    float fadeAmount = 0.015f;  // 1.5% fade per frame for slower fade
    XMFLOAT4 fadeOverlay = {0.0f, 0.0f, 0.0f, fadeAmount};
    
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
        // No mirror - full spectrum across screen
        for (int i = 0; i < 255; i++) {
            float x1 = -1.0f + (float)i / 255.0f * 2.0f;
            float x2 = -1.0f + (float)(i + 1) / 255.0f * 2.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
    } else if (m_lfMirrorMode == LFMirrorMode::BassEdges) {
        // Bass at edges - mirror at center
        // Left half: normal spectrum
        for (int i = 0; i < 255; i++) {
            float x1 = -1.0f + (float)i / 255.0f;
            float x2 = -1.0f + (float)(i + 1) / 255.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
        // Right half: flipped spectrum
        for (int i = 0; i < 255; i++) {
            float x1 = 1.0f - (float)i / 255.0f;
            float x2 = 1.0f - (float)(i + 1) / 255.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
    } else {  // BassCenter
        // Bass in center - mirror at edges
        // Left half: flipped spectrum
        for (int i = 0; i < 255; i++) {
            float x1 = 0.0f - (float)i / 255.0f;
            float x2 = 0.0f - (float)(i + 1) / 255.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
        // Right half: normal spectrum
        for (int i = 0; i < 255; i++) {
            float x1 = 0.0f + (float)i / 255.0f;
            float x2 = 0.0f + (float)(i + 1) / 255.0f;
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
