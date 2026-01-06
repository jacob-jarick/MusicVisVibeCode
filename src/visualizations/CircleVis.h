#pragma once
#include "BaseVisualization.h"

class CircleVis : public BaseVisualization {
public:
    CircleVis() = default;
    ~CircleVis() override { Cleanup(); }
    
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
    enum class PeakMode { Inside, Outside, Both };
    
    float m_rotation = 0.0f;        // Current rotation angle in degrees
    float m_rotationSpeed = 0.1f;   // Rotation speed in degrees per frame (default 0.1, range -1.5 to 1.5)
    float m_fadeRate = 1.0f;        // Fade percentage (0-5%, default 1%)
    float m_zoomRate = 1.0f;        // Zoom percentage (0-5%, default 1%)
    float m_blurRate = 1.0f;        // Blur percentage (0-10%, default 1%)
    PeakMode m_peakMode = PeakMode::Inside;  // Where peaks appear
    bool m_zoomOut = false;         // false = zoom in (default), true = zoom out
    bool m_fillMode = false;        // false = line only (default), true = filled
    float m_hue = 0.0f;             // Current hue for rainbow color cycling (0-360)
    
    ID3D11Texture2D* m_historyTexture = nullptr;
    ID3D11ShaderResourceView* m_historySRV = nullptr;
    ID3D11RenderTargetView* m_historyRTV = nullptr;
    ID3D11Texture2D* m_tempTexture = nullptr;
    ID3D11ShaderResourceView* m_tempSRV = nullptr;
    ID3D11RenderTargetView* m_tempRTV = nullptr;
};
