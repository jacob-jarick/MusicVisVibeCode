#include "SpectrumVis.h"
#include "../Config.h"
#include <algorithm>

bool SpectrumVis::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) {
    m_device = device;
    m_context = context;
    m_width = width;
    m_height = height;
    return true;
}

void SpectrumVis::Cleanup() {
    // No resources to clean up for Spectrum
}

void SpectrumVis::Update(float deltaTime, const AudioData& audioData, bool useNormalized,
                        ID3D11Buffer* vertexBuffer, ID3D11InputLayout* inputLayout,
                        ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader) {
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
                float val = useNormalized ? audioData.SpectrumNormalized[binIndex] : audioData.Spectrum[binIndex];
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
    m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    m_context->Unmap(vertexBuffer, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(vertexShader, NULL, 0);
    m_context->PSSetShader(pixelShader, NULL, 0);

    m_context->Draw(vertices.size(), 0);
}

void SpectrumVis::HandleInput(WPARAM key) {
    // No specific controls for Spectrum visualization
}

std::string SpectrumVis::GetHelpText() const {
    return ""; // No specific controls
}

void SpectrumVis::ResetToDefaults() {
    m_decayRate = 5.0f;
    for (int i = 0; i < 16; i++) {
        m_peakLevels[i] = 0.0f;
    }
}

void SpectrumVis::SaveState(Config& config, int visIndex) {
    config.spectrumDecayRate = m_decayRate;
}

void SpectrumVis::LoadState(Config& config, int visIndex) {
    m_decayRate = config.spectrumDecayRate;
}
