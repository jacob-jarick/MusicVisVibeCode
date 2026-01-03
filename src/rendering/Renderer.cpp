#include "Renderer.h"
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
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

bool Renderer::Initialize(HINSTANCE hInstance, int width, int height) {
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
    bd.ByteWidth = sizeof(Vertex) * 10000; // Enough for many quads
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

void Renderer::Run() {
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Render();
        }
    }
}

void Renderer::Render() {
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(m_renderTargetView, clearColor);

    // Set Common States
    m_context->OMSetBlendState(m_blendState, NULL, 0xffffffff);
    m_context->PSSetSamplers(0, 1, &m_samplerState);

    // Draw Background
    if (m_showBackground && m_backgroundSRV) {
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

    UpdateSpectrumVis();
    RenderOSD();

    m_swapChain->Present(1, 0);
}

void Renderer::CreateTextResources() {
    // Create a texture for text rendering (GDI compatible)
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = 512;
    desc.Height = 256;
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
        RECT fullRect = {0, 0, 512, 256};
        HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &fullRect, hBlackBrush);
        DeleteObject(hBlackBrush);

        // 2. Setup Font
        HFONT hFont = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        SelectObject(hdc, hFont);

        // 3. Measure Text
        RECT textRect = {0, 0, 512, 256};
        DrawText(hdc, text.c_str(), -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
        
        // 4. Calculate Box Size (20% wider and taller)
        int textWidth = textRect.right - textRect.left;
        int textHeight = textRect.bottom - textRect.top;
        int boxWidth = (int)(textWidth * 1.2f);
        int boxHeight = (int)(textHeight * 1.2f);
        
        // 5. Position Box
        RECT boxRect;
        if (rightAlign) {
            boxRect.right = 512 - 10; // 10px padding from edge
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
        osdText = "HELP MENU:\n\n"
                  "H: Toggle Help\n"
                  "I: Toggle Info\n"
                  "C: Toggle Clock\n"
                  "N: Toggle Normalized\n"
                  "F: Toggle Fullscreen\n"
                  "B: Random Background\n"
                  "[/]: Prev/Next Background\n"
                  "-/=: Adjust Decay\n"
                  "ESC: Quit";
    } else if (m_showInfo) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "INFO OVERLAY:\n\n";
        ss << "FPS: " << m_fps << "\n";
        ss << "Decay Rate: " << m_decayRate << "\n";
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
    float w = 0.6f; // Width relative to screen
    float h = 0.6f; // Height relative to screen
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
        m_decayRate = std::max(0.1f, m_decayRate - 0.5f);
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        m_decayRate = std::min(20.0f, m_decayRate + 0.5f);
    } else if (key == 'H') {
        m_showHelp = !m_showHelp;
        if (m_showHelp) { m_showInfo = false; m_showClock = false; }
    } else if (key == 'I') {
        m_showInfo = !m_showInfo;
        if (m_showInfo) { m_showHelp = false; m_showClock = false; }
    } else if (key == 'C') {
        m_showClock = !m_showClock;
        if (m_showClock) { m_showHelp = false; m_showInfo = false; }
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
    } else if (key == VK_LEFT || key == VK_RIGHT) {
        // TODO: Switch visualization (Next/Prev)
    } else if (key == 'R') {
        // TODO: Random visualization
    } else if (key >= '0' && key <= '9') {
        // TODO: Switch to visualization index
    } else if (key == VK_ESCAPE) {
        PostQuitMessage(0);
    }
}

void Renderer::UpdateSpectrumVis() {
    const AudioData& data = m_audioEngine.GetData();
    
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    float deltaTime = (float)(currentTime.QuadPart - m_lastTime.QuadPart) / m_frequency.QuadPart;
    m_lastTime = currentTime;

    // FPS Calculation
    m_frameCount++;
    m_timeElapsed += deltaTime;
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
