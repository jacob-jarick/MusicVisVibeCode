#pragma once
#include "BaseVisualization.h"

class Spectrum2Vis : public BaseVisualization {
public:
    Spectrum2Vis() = default;
    ~Spectrum2Vis() override { Cleanup(); }
    
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
    enum class MirrorMode { None, BassEdges, BassCenter };
    
    float m_peakLevels[28] = {0};
    float m_decayRate = 5.0f;       // Segments per second (default: 1 segment every 0.2s)
    MirrorMode m_mirrorMode = MirrorMode::BassEdges;
};
