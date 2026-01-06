#include "Spectrum2Vis.h"
#include "../Config.h"
#include <algorithm>

bool Spectrum2Vis::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) {
    m_device = device;
    m_context = context;
    m_width = width;
    m_height = height;
    return true;
}

void Spectrum2Vis::Cleanup() {
    // No resources to clean up for Spectrum2
}

void Spectrum2Vis::Update(float deltaTime, const AudioData& audioData, bool useNormalized,
                         ID3D11Buffer* vertexBuffer, ID3D11InputLayout* inputLayout,
                         ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader) {
    std::vector<Vertex> vertices;

    // 28 bars, 48 segments per bar
    const int numBars = 28;
    const int segmentsPerBar = 48;
    
    // Helper to calculate bar positions based on mirror mode
    auto GetBarPositions = [&](int barIndex, float& xStart, float& barWidth) {
        float totalWidth = 2.0f; // NDC space -1 to 1
        
        if (m_mirrorMode == MirrorMode::None) {
            // No mirror - 28 bars across full width
            barWidth = totalWidth / numBars;
            xStart = -1.0f + barIndex * barWidth;
        } else {
            // Mirror modes - 28 bars displayed (14 per side, using 14 data sources)
            barWidth = totalWidth / numBars;
            
            if (m_mirrorMode == MirrorMode::BassEdges) {
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
        if (m_mirrorMode != MirrorMode::None && i >= 14) {
            // Mirror modes: bars 14-27 use same data as bars 0-13
            dataIndex = i - 14;
        }
        
        // Data curation: Use bins 0-223 (224 bins), divided into 28 buckets = 8 bins per bucket
        // In mirror mode, only use first 14 buckets (112 bins)
        float barValue = 0.0f;
        int binsPerBucket = (m_mirrorMode == MirrorMode::None) ? 8 : 16;
        int maxBinIndex = (m_mirrorMode == MirrorMode::None) ? 224 : 224;
        
        for (int j = 0; j < binsPerBucket; j++) {
            int binIndex = dataIndex * binsPerBucket + j;
            if (binIndex < maxBinIndex) {
                float val = audioData.SpectrumNormalized[binIndex];
                if (val > barValue) barValue = val;
            }
        }
        
        // Scale to 48 segments
        float currentHeightSegments = barValue * segmentsPerBar;
        int numSegments = (int)currentHeightSegments;
        
        // Update Peak
        if (currentHeightSegments > m_peakLevels[i]) {
            m_peakLevels[i] = currentHeightSegments;
        } else {
            m_peakLevels[i] -= m_decayRate * deltaTime;
            if (m_peakLevels[i] < 0.0f) m_peakLevels[i] = 0.0f;
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
        int peakSegment = (int)m_peakLevels[i];
        if (peakSegment >= 0 && peakSegment < segmentsPerBar + 1) {
            float y = -1.0f + peakSegment * h + segGap;
            float segH = h - 2 * segGap;
            float x = xStart + gap;
            float w = barWidth - 2 * gap;
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

    m_context->Draw((UINT)vertices.size(), 0);
}

void Spectrum2Vis::HandleInput(WPARAM key) {
    if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        m_decayRate = std::max(0.1f, m_decayRate - 0.5f);
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        m_decayRate = std::min(20.0f, m_decayRate + 0.5f);
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

std::string Spectrum2Vis::GetHelpText() const {
    return "-/=: Adjust Decay\n"
           "M: Cycle Mirror Mode";
}

void Spectrum2Vis::ResetToDefaults() {
    m_decayRate = 5.0f;
    m_mirrorMode = MirrorMode::BassEdges;
    for (int i = 0; i < 28; i++) {
        m_peakLevels[i] = 0.0f;
    }
}

void Spectrum2Vis::SaveState(Config& config, int visIndex) {
    config.s2DecayRate = m_decayRate;
    config.s2MirrorMode = (int)m_mirrorMode;
}

void Spectrum2Vis::LoadState(Config& config, int visIndex) {
    m_decayRate = config.s2DecayRate;
    m_mirrorMode = (MirrorMode)config.s2MirrorMode;
}
