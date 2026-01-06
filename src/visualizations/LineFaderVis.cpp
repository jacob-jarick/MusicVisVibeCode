#include "LineFaderVis.h"
#include "../Config.h"
#include <algorithm>
#include <cmath>

bool LineFaderVis::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) {
    m_device = device;
    m_context = context;
    m_width = width;
    m_height = height;
    
    // Initialize textures
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    
    device->CreateTexture2D(&desc, NULL, &m_historyTexture);
    device->CreateShaderResourceView(m_historyTexture, NULL, &m_historySRV);
    device->CreateRenderTargetView(m_historyTexture, NULL, &m_historyRTV);
    
    device->CreateTexture2D(&desc, NULL, &m_tempTexture);
    device->CreateShaderResourceView(m_tempTexture, NULL, &m_tempSRV);
    device->CreateRenderTargetView(m_tempTexture, NULL, &m_tempRTV);
    
    // Clear both textures to black
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(m_historyRTV, clearColor);
    context->ClearRenderTargetView(m_tempRTV, clearColor);
    
    return true;
}

void LineFaderVis::Cleanup() {
    if (m_historySRV) { m_historySRV->Release(); m_historySRV = nullptr; }
    if (m_historyRTV) { m_historyRTV->Release(); m_historyRTV = nullptr; }
    if (m_historyTexture) { m_historyTexture->Release(); m_historyTexture = nullptr; }
    if (m_tempSRV) { m_tempSRV->Release(); m_tempSRV = nullptr; }
    if (m_tempRTV) { m_tempRTV->Release(); m_tempRTV = nullptr; }
    if (m_tempTexture) { m_tempTexture->Release(); m_tempTexture = nullptr; }
}

void LineFaderVis::Update(float deltaTime, const AudioData& audioData, bool useNormalized,
                         ID3D11Buffer* vertexBuffer, ID3D11InputLayout* inputLayout,
                         ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader) {
    std::vector<Vertex> vertices;
    D3D11_MAPPED_SUBRESOURCE ms;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    XMFLOAT4 white = {1.0f, 1.0f, 1.0f, 1.0f};
    
    // Save the current render target so we can restore it later
    ID3D11RenderTargetView* originalRenderTarget = nullptr;
    m_context->OMGetRenderTargets(1, &originalRenderTarget, nullptr);
    
    // Step 1: Render to temp texture - shift history up and fade
    m_context->OMSetRenderTargets(1, &m_tempRTV, NULL);
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(m_tempRTV, clearColor);
    
    // Calculate scroll offset in NDC
    float scrollOffsetNDC = (float)m_scrollSpeed / (float)m_height * 2.0f;
    
    // Draw existing history, shifted up
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, white, {0.0f, 0.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {-1.0f, -1.0f + scrollOffsetNDC, 0.0f}, white, {0.0f, 1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {1.0f, -1.0f + scrollOffsetNDC, 0.0f}, white, {1.0f, 1.0f} });
    vertices.push_back({ {-1.0f, -1.0f + scrollOffsetNDC, 0.0f}, white, {0.0f, 1.0f} });
    
    m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(vertexBuffer, 0);
    
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(vertexShader, NULL, 0);
    m_context->PSSetShader(pixelShader, NULL, 0);
    m_context->PSSetShaderResources(0, 1, &m_historySRV);
    m_context->Draw(6, 0);
    
    // Apply fade with semi-transparent black overlay
    XMFLOAT4 fadeOverlay = {0.0f, 0.0f, 0.0f, m_fadeRate};
    
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {1.0f, -1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, fadeOverlay, {-1.0f, -1.0f} });
    
    m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(vertexBuffer, 0);
    
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
    m_context->Draw(6, 0);
    
    // Step 2: Add new spectrum line at the bottom
    // Get smoothed spectrum data
    float smoothedSpectrum[256];
    for (int i = 0; i < 256; i++) {
        float val = useNormalized ? audioData.SpectrumNormalized[i] : audioData.Spectrum[i];
        
        // Apply smoothing (rolling average of 3)
        float prev = (i > 0) ? (useNormalized ? audioData.SpectrumNormalized[i-1] : audioData.Spectrum[i-1]) : val;
        float next = (i < 255) ? (useNormalized ? audioData.SpectrumNormalized[i+1] : audioData.Spectrum[i+1]) : val;
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
    if (m_mirrorMode == MirrorMode::None) {
        // No mirror - full spectrum across screen (using first 224 bins, dropping high end)
        for (int i = 0; i < 223; i++) {
            float x1 = -1.0f + (float)i / 223.0f * 2.0f;
            float x2 = -1.0f + (float)(i + 1) / 223.0f * 2.0f;
            float y1 = baseY + smoothedSpectrum[i] * scaleHeight;
            float y2 = baseY + smoothedSpectrum[i + 1] * scaleHeight;
            
            DrawLineSegment(x1, y1, x2, y2);
        }
    } else if (m_mirrorMode == MirrorMode::BassEdges) {
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
        m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
        m_context->Unmap(vertexBuffer, 0);
        
        m_context->Draw((UINT)vertices.size(), 0);
    }
    
    // Step 3: Copy temp back to history for next frame
    ID3D11Resource* srcResource = nullptr;
    ID3D11Resource* dstResource = nullptr;
    m_tempTexture->QueryInterface(__uuidof(ID3D11Resource), (void**)&srcResource);
    m_historyTexture->QueryInterface(__uuidof(ID3D11Resource), (void**)&dstResource);
    m_context->CopyResource(dstResource, srcResource);
    srcResource->Release();
    dstResource->Release();
    
    // Step 4: Render final result to screen (restore original render target)
    if (originalRenderTarget) {
        m_context->OMSetRenderTargets(1, &originalRenderTarget, NULL);
    }
    
    vertices.clear();
    vertices.push_back({ {-1.0f, 1.0f, 0.0f}, white, {0.0f, 0.0f} });
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, white, {0.0f, 1.0f} });
    
    vertices.push_back({ {1.0f, 1.0f, 0.0f}, white, {1.0f, 0.0f} });
    vertices.push_back({ {1.0f, -1.0f, 0.0f}, white, {1.0f, 1.0f} });
    vertices.push_back({ {-1.0f, -1.0f, 0.0f}, white, {0.0f, 1.0f} });
    
    m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(vertexBuffer, 0);
    
    m_context->PSSetShaderResources(0, 1, &m_historySRV);
    m_context->Draw(6, 0);
    
    m_context->PSSetShaderResources(0, 1, &nullSRV);
    
    // Release the saved render target reference
    if (originalRenderTarget) {
        originalRenderTarget->Release();
    }
}

void LineFaderVis::HandleInput(WPARAM key) {
    if (key == VK_OEM_COMMA) {  // ',' Key
        m_fadeRate = std::max(0.0005f, m_fadeRate - 0.0005f);  // Min 0.05%
    } else if (key == VK_OEM_PERIOD) {  // '.' Key
        m_fadeRate = std::min(0.005f, m_fadeRate + 0.0005f);  // Max 0.50%
    } else if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        m_scrollSpeed = std::max(1, m_scrollSpeed - 1);  // Minimum 1 pixel
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        m_scrollSpeed = std::min(50, m_scrollSpeed + 1);  // Maximum 50 pixels
    } else if (key == 'M') {
        // Cycle through mirror modes
        if (m_mirrorMode == MirrorMode::None) {
            m_mirrorMode = MirrorMode::BassEdges;
        } else if (m_mirrorMode == MirrorMode::BassEdges) {
            m_mirrorMode = MirrorMode::BassCenter;
        } else {
            m_mirrorMode = MirrorMode::None;
        }
    }
}

std::string LineFaderVis::GetHelpText() const {
    return ",/.: Adjust Fade Rate\n"
           "-/=: Adjust Scroll Speed\n"
           "M: Cycle Mirror Mode";
}

void LineFaderVis::ResetToDefaults() {
    m_scrollSpeed = 5;
    m_fadeRate = 0.005f;
    m_mirrorMode = MirrorMode::BassEdges;
}

void LineFaderVis::SaveState(Config& config, int visIndex) {
    config.lfScrollSpeed = m_scrollSpeed;
    config.lfFadeRate = m_fadeRate;
    config.lfMirrorMode = (int)m_mirrorMode;
}

void LineFaderVis::LoadState(Config& config, int visIndex) {
    m_scrollSpeed = config.lfScrollSpeed;
    m_fadeRate = config.lfFadeRate;
    m_mirrorMode = (MirrorMode)config.lfMirrorMode;
}
