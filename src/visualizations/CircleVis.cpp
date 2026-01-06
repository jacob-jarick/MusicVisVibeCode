#include "CircleVis.h"
#include "../Config.h"
#include <algorithm>
#include <cmath>

bool CircleVis::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) {
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
    
    // Clear to transparent black so background shows through
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(m_historyRTV, clearColor);
    context->ClearRenderTargetView(m_tempRTV, clearColor);
    
    return true;
}

void CircleVis::Cleanup() {
    if (m_historySRV) { m_historySRV->Release(); m_historySRV = nullptr; }
    if (m_historyRTV) { m_historyRTV->Release(); m_historyRTV = nullptr; }
    if (m_historyTexture) { m_historyTexture->Release(); m_historyTexture = nullptr; }
    if (m_tempSRV) { m_tempSRV->Release(); m_tempSRV = nullptr; }
    if (m_tempRTV) { m_tempRTV->Release(); m_tempRTV = nullptr; }
    if (m_tempTexture) { m_tempTexture->Release(); m_tempTexture = nullptr; }
}

void CircleVis::Update(float deltaTime, const AudioData& audioData, bool useNormalized,
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
    
    // Step 1: Render previous frame to temp with zoom effect (RECURSIVE FEEDBACK)
    // DO NOT CLEAR - this is key for tunnel effect
    m_context->OMSetRenderTargets(1, &m_tempRTV, NULL);
    
    // Calculate zoom scale based on zoom direction
    // Zoom IN (default): scale < 1.0, shrinks previous frame toward center (tunnel moving forward)
    // Zoom OUT: scale > 1.0, expands previous frame toward edges (tunnel coming at you)
    float zoomScale;
    if (m_zoomOut) {
        // Zoom out: expand the frame
        zoomScale = 1.0f + (m_zoomRate / 100.0f);
    } else {
        // Zoom in (default): shrink the frame
        zoomScale = 1.0f - (m_zoomRate / 100.0f);
        if (zoomScale < 0.01f) zoomScale = 0.01f;  // Prevent negative/zero
    }
    
    // Calculate fade alpha - this dims the previous frame
    float fadeAlpha = 1.0f - (m_fadeRate / 100.0f);
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
    
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
    
    // Step 2: Get smoothed spectrum data
    float smoothedSpectrum[256];
    for (int i = 0; i < 256; i++) {
        float val = useNormalized ? audioData.SpectrumNormalized[i] : audioData.Spectrum[i];
        
        // Apply smoothing (rolling average of 3)
        float prev = (i > 0) ? (useNormalized ? audioData.SpectrumNormalized[i-1] : audioData.Spectrum[i-1]) : val;
        float next = (i < 255) ? (useNormalized ? audioData.SpectrumNormalized[i+1] : audioData.Spectrum[i+1]) : val;
        smoothedSpectrum[i] = (prev + val + next) / 3.0f;
    }
    
    // Step 3: Update rotation
    m_rotation += m_rotationSpeed;
    if (m_rotation >= 360.0f) m_rotation -= 360.0f;
    if (m_rotation < 0.0f) m_rotation += 360.0f;
    
    // Step 4: Update hue for rainbow color cycling
    m_hue += 0.5f;  // Cycle through hues slowly
    if (m_hue >= 360.0f) m_hue -= 360.0f;
    
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
    
    XMFLOAT4 circleColor = HSVtoRGB(m_hue, 0.8f, 1.0f);
    
    // Helper lambda to draw a line segment (like LineFader)
    auto DrawLineSegment = [&](float x1, float y1, float x2, float y2, XMFLOAT4 color) {
        float outerThickness = 0.004f;  // Outer line thickness
        float innerThickness = 0.002f;  // Inner white core thickness
        
        // Calculate perpendicular offset for line thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.0001f) return;
        
        float perpX = -dy / len;
        float perpY = dx / len;
        
        // Draw outer colored line
        vertices.push_back({ {x1 + perpX * outerThickness, y1 + perpY * outerThickness, 0.0f}, color, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 + perpX * outerThickness, y2 + perpY * outerThickness, 0.0f}, color, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * outerThickness, y1 - perpY * outerThickness, 0.0f}, color, {-1.0f, -1.0f} });
        
        vertices.push_back({ {x2 + perpX * outerThickness, y2 + perpY * outerThickness, 0.0f}, color, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - perpX * outerThickness, y2 - perpY * outerThickness, 0.0f}, color, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * outerThickness, y1 - perpY * outerThickness, 0.0f}, color, {-1.0f, -1.0f} });
        
        // Draw inner white core
        XMFLOAT4 white = {1.0f, 1.0f, 1.0f, 1.0f};
        vertices.push_back({ {x1 + perpX * innerThickness, y1 + perpY * innerThickness, 0.0f}, white, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 + perpX * innerThickness, y2 + perpY * innerThickness, 0.0f}, white, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * innerThickness, y1 - perpY * innerThickness, 0.0f}, white, {-1.0f, -1.0f} });
        
        vertices.push_back({ {x2 + perpX * innerThickness, y2 + perpY * innerThickness, 0.0f}, white, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - perpX * innerThickness, y2 - perpY * innerThickness, 0.0f}, white, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 - perpX * innerThickness, y1 - perpY * innerThickness, 0.0f}, white, {-1.0f, -1.0f} });
    };
    
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
        float angle1 = m_rotation + i * angularStep * 2.0f;
        float angle2 = m_rotation + (i + 1) * angularStep * 2.0f;
        
        float amplitude1 = smoothedSpectrum[i * 2] * maxAmplitude;
        float amplitude2 = smoothedSpectrum[(i + 1) * 2] * maxAmplitude;
        
        float radius1, radius2;
        if (m_peakMode == PeakMode::Inside) {
            radius1 = baseRadius - amplitude1;
            radius2 = baseRadius - amplitude2;
        } else if (m_peakMode == PeakMode::Outside) {
            radius1 = baseRadius + amplitude1;
            radius2 = baseRadius + amplitude2;
        } else {  // Both
            radius1 = baseRadius;
            radius2 = baseRadius;
        }
        
        float radian1 = angle1 * 3.14159f / 180.0f;
        float radian2 = angle2 * 3.14159f / 180.0f;
        
        float x1 = centerX + cosf(radian1) * radius1;
        float y1 = centerY + sinf(radian1) * radius1;
        float x2 = centerX + cosf(radian2) * radius2;
        float y2 = centerY + sinf(radian2) * radius2;
        
        if (m_peakMode == PeakMode::Both) {
            // Draw both inside and outside
            float x1in = centerX + cosf(radian1) * (baseRadius - amplitude1);
            float y1in = centerY + sinf(radian1) * (baseRadius - amplitude1);
            float x2in = centerX + cosf(radian2) * (baseRadius - amplitude2);
            float y2in = centerY + sinf(radian2) * (baseRadius - amplitude2);
            
            float x1out = centerX + cosf(radian1) * (baseRadius + amplitude1);
            float y1out = centerY + sinf(radian1) * (baseRadius + amplitude1);
            float x2out = centerX + cosf(radian2) * (baseRadius + amplitude2);
            float y2out = centerY + sinf(radian2) * (baseRadius + amplitude2);
            
            if (m_fillMode) {
                // Filled mode - draw triangles
                float x1b = centerX + cosf(radian1) * baseRadius;
                float y1b = centerY + sinf(radian1) * baseRadius;
                float x2b = centerX + cosf(radian2) * baseRadius;
                float y2b = centerY + sinf(radian2) * baseRadius;
                
                // Inside triangle
                vertices.push_back({ {x1in, y1in, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2in, y2in, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2in, y2in, 0.0f}, circleColor, {-1.0f, -1.0f} });
                
                // Outside triangle
                vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1out, y1out, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1out, y1out, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2out, y2out, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            } else {
                // Line mode - draw both lines
                DrawLineSegment(x1in, y1in, x2in, y2in, circleColor);
                DrawLineSegment(x1out, y1out, x2out, y2out, circleColor);
            }
        } else if (m_fillMode) {
            // Filled mode - draw triangles from base to peaks
            float x1b = centerX + cosf(radian1) * baseRadius;
            float y1b = centerY + sinf(radian1) * baseRadius;
            float x2b = centerX + cosf(radian2) * baseRadius;
            float y2b = centerY + sinf(radian2) * baseRadius;
            
            vertices.push_back({ {x1, y1, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
            
            vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
        } else {
            // Line mode - draw just the outline
            DrawLineSegment(x1, y1, x2, y2, circleColor);
        }
    }
    
    // Second half - mirrored (CCB)
    for (int i = numSamples - 1; i >= 0; i--) {
        float angle1 = m_rotation + (numSamples + (numSamples - 1 - i)) * angularStep * 2.0f;
        float angle2 = m_rotation + (numSamples + (numSamples - i)) * angularStep * 2.0f;
        
        float amplitude1 = smoothedSpectrum[i * 2] * maxAmplitude;
        float amplitude2 = smoothedSpectrum[((i > 0) ? i - 1 : 0) * 2] * maxAmplitude;
        
        float radius1, radius2;
        if (m_peakMode == PeakMode::Inside) {
            radius1 = baseRadius - amplitude1;
            radius2 = baseRadius - amplitude2;
        } else if (m_peakMode == PeakMode::Outside) {
            radius1 = baseRadius + amplitude1;
            radius2 = baseRadius + amplitude2;
        } else {  // Both
            radius1 = baseRadius;
            radius2 = baseRadius;
        }
        
        float radian1 = angle1 * 3.14159f / 180.0f;
        float radian2 = angle2 * 3.14159f / 180.0f;
        
        float x1 = centerX + cosf(radian1) * radius1;
        float y1 = centerY + sinf(radian1) * radius1;
        float x2 = centerX + cosf(radian2) * radius2;
        float y2 = centerY + sinf(radian2) * radius2;
        
        if (m_peakMode == PeakMode::Both) {
            // Draw both inside and outside
            float x1in = centerX + cosf(radian1) * (baseRadius - amplitude1);
            float y1in = centerY + sinf(radian1) * (baseRadius - amplitude1);
            float x2in = centerX + cosf(radian2) * (baseRadius - amplitude2);
            float y2in = centerY + sinf(radian2) * (baseRadius - amplitude2);
            
            float x1out = centerX + cosf(radian1) * (baseRadius + amplitude1);
            float y1out = centerY + sinf(radian1) * (baseRadius + amplitude1);
            float x2out = centerX + cosf(radian2) * (baseRadius + amplitude2);
            float y2out = centerY + sinf(radian2) * (baseRadius + amplitude2);
            
            if (m_fillMode) {
                // Filled mode - draw triangles
                float x1b = centerX + cosf(radian1) * baseRadius;
                float y1b = centerY + sinf(radian1) * baseRadius;
                float x2b = centerX + cosf(radian2) * baseRadius;
                float y2b = centerY + sinf(radian2) * baseRadius;
                
                // Inside triangle
                vertices.push_back({ {x1in, y1in, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2in, y2in, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2in, y2in, 0.0f}, circleColor, {-1.0f, -1.0f} });
                
                // Outside triangle
                vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1out, y1out, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x1out, y1out, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2out, y2out, 0.0f}, circleColor, {-1.0f, -1.0f} });
                vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            } else {
                // Line mode - draw both lines
                DrawLineSegment(x1in, y1in, x2in, y2in, circleColor);
                DrawLineSegment(x1out, y1out, x2out, y2out, circleColor);
            }
        } else if (m_fillMode) {
            // Filled mode - draw triangles from base to peaks
            float x1b = centerX + cosf(radian1) * baseRadius;
            float y1b = centerY + sinf(radian1) * baseRadius;
            float x2b = centerX + cosf(radian2) * baseRadius;
            float y2b = centerY + sinf(radian2) * baseRadius;
            
            vertices.push_back({ {x1, y1, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
            
            vertices.push_back({ {x1b, y1b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x2b, y2b, 0.0f}, circleColor, {-1.0f, -1.0f} });
            vertices.push_back({ {x2, y2, 0.0f}, circleColor, {-1.0f, -1.0f} });
        } else {
            // Line mode - draw just the outline
            DrawLineSegment(x1, y1, x2, y2, circleColor);
        }
    }
    
    m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(vertexBuffer, 0);
    
    m_context->Draw(vertices.size(), 0);
    
    // Step 6: Copy temp texture back to history texture for next frame (feedback loop!)
    m_context->CopyResource(m_historyTexture, m_tempTexture);
    
    // Step 7: Render final result to back buffer (restore original render target)
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
    
    // Display the current temp texture (which has the new circle + zoomed history)
    m_context->PSSetShaderResources(0, 1, &m_tempSRV);
    m_context->Draw(6, 0);
    
    m_context->PSSetShaderResources(0, 1, &nullSRV);
    
    // Release the saved render target reference
    if (originalRenderTarget) {
        originalRenderTarget->Release();
    }
}

void CircleVis::HandleInput(WPARAM key) {
    if (key == VK_OEM_COMMA) {  // ',' Key
        m_fadeRate = std::max(0.0f, m_fadeRate - 0.05f);  // Min 0%
    } else if (key == VK_OEM_PERIOD) {  // '.' Key
        m_fadeRate = std::min(5.0f, m_fadeRate + 0.05f);  // Max 5%
    } else if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        m_zoomRate = std::max(0.0f, m_zoomRate - 0.05f);  // Min 0%
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        m_zoomRate = std::min(5.0f, m_zoomRate + 0.05f);  // Max 5%
    } else if (key == VK_OEM_1) {  // ';' Key
        m_blurRate = std::max(0.0f, m_blurRate - 0.05f);  // Min 0%
    } else if (key == VK_OEM_7) {  // '\'' Key
        m_blurRate = std::min(10.0f, m_blurRate + 0.05f);  // Max 10%
    } else if (key == 'K') {
        m_rotationSpeed = std::max(-1.5f, m_rotationSpeed - 0.1f);  // Min -1.5 degrees
    } else if (key == 'L') {
        m_rotationSpeed = std::min(1.5f, m_rotationSpeed + 0.1f);  // Max 1.5 degrees
    } else if (key == 'M') {
        // Cycle through peak modes: Inside -> Outside -> Both -> Inside
        if (m_peakMode == PeakMode::Inside) {
            m_peakMode = PeakMode::Outside;
        } else if (m_peakMode == PeakMode::Outside) {
            m_peakMode = PeakMode::Both;
        } else {
            m_peakMode = PeakMode::Inside;
        }
    } else if (key == 'Z') {
        m_zoomOut = !m_zoomOut;
    } else if (key == 'P') {
        m_fillMode = !m_fillMode;
    }
}

std::string CircleVis::GetHelpText() const {
    return ",/.: Adjust Fade %\n"
           "-/=: Adjust Zoom %\n"
           ";/': Adjust Blur %\n"
           "K/L: Adjust Rotation Speed\n"
           "M: Toggle Peaks Inside/Outside\n"
           "Z: Toggle Zoom In/Out\n"
           "P: Toggle Fill/Line Mode";
}

void CircleVis::ResetToDefaults() {
    m_rotation = 0.0f;
    m_rotationSpeed = 0.1f;
    m_fadeRate = 1.0f;
    m_zoomRate = 1.0f;
    m_blurRate = 1.0f;
    m_peakMode = PeakMode::Inside;
    m_zoomOut = false;
    m_fillMode = false;
    m_hue = 0.0f;
}

void CircleVis::SaveState(Config& config, int visIndex) {
    config.circleRotationSpeed = m_rotationSpeed;
    config.circleFadeRate = m_fadeRate;
    config.circleZoomRate = m_zoomRate;
    config.circleBlurRate = m_blurRate;
    config.circlePeakMode = (int)m_peakMode;
    config.circleZoomOut = m_zoomOut;
    config.circleFillMode = m_fillMode;
}

void CircleVis::LoadState(Config& config, int visIndex) {
    m_rotationSpeed = config.circleRotationSpeed;
    m_fadeRate = config.circleFadeRate;
    m_zoomRate = config.circleZoomRate;
    m_blurRate = config.circleBlurRate;
    m_peakMode = (PeakMode)config.circlePeakMode;
    m_zoomOut = config.circleZoomOut;
    m_fillMode = config.circleFillMode;
}
