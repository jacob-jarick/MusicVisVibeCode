#pragma once
#include "BaseVisualization.h"

class CyberValley2Vis : public BaseVisualization {
public:
    CyberValley2Vis() = default;
    ~CyberValley2Vis() override { Cleanup(); }
    
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) override;
    void Cleanup() override;
    void Update(float deltaTime, const AudioData& audioData, bool useNormalized,
               ID3D11Buffer* vertexBuffer, ID3D11InputLayout* inputLayout,
               ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader) override;
    void HandleInput(WPARAM key) override;
    std::string GetHelpText() const override;
    void ResetToDefaults() override;
    void SaveState(class Config& config, int visIndex) override;
    void LoadState(class Config& config, int visIndex) override;

private:
    float m_time = 0.0f;           // Day/night cycle timer (0-600 seconds)
    float m_speed = 50.0f;         // Scroll speed percentage (5% to 200%), 50% = ~2s to horizon
    float m_gridOffset = 0.0f;     // Grid scroll position (0-1)
    bool m_sunMode = false;        // true = Day, false = Night (default night mode)
    bool m_showGrid = true;        // Grid visibility toggle
    float m_mountainHistory[60][256] = {{0}};  // Frozen snapshots of spectrum for mountains
    int m_historyWriteIndex = 0;   // Current write position in history buffer
    float m_timeSinceLastLine = 0.0f;  // Time accumulator for line drawing (2 lines/sec)
};
