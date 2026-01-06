#pragma once
#include "BaseVisualization.h"

class LineFaderVis : public BaseVisualization {
public:
    LineFaderVis() = default;
    ~LineFaderVis() override { Cleanup(); }
    
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
    
    int m_scrollSpeed = 5;          // Scroll speed in pixels per frame (1-50)
    float m_fadeRate = 0.005f;      // Fade rate per frame (0.0005 - 0.005, i.e., 0.05% - 0.50%)
    MirrorMode m_mirrorMode = MirrorMode::BassEdges;
    
    ID3D11Texture2D* m_historyTexture = nullptr;
    ID3D11ShaderResourceView* m_historySRV = nullptr;
    ID3D11RenderTargetView* m_historyRTV = nullptr;
    ID3D11Texture2D* m_tempTexture = nullptr;
    ID3D11ShaderResourceView* m_tempSRV = nullptr;
    ID3D11RenderTargetView* m_tempRTV = nullptr;
};
