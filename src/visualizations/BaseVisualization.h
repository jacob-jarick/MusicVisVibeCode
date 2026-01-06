#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "../audio/AudioEngine.h"

using namespace DirectX;

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
    XMFLOAT2 texCoord;
};

class BaseVisualization {
public:
    virtual ~BaseVisualization() = default;
    
    // Initialize visualization-specific resources
    virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) = 0;
    
    // Cleanup visualization-specific resources
    virtual void Cleanup() = 0;
    
    // Update and render the visualization
    virtual void Update(float deltaTime, const AudioData& audioData, bool useNormalized, 
                       ID3D11Buffer* vertexBuffer, ID3D11InputLayout* inputLayout,
                       ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader) = 0;
    
    // Handle keyboard input
    virtual void HandleInput(WPARAM key) = 0;
    
    // Get help text for this visualization
    virtual std::string GetHelpText() const = 0;
    
    // Reset to default settings
    virtual void ResetToDefaults() = 0;
    
    // Save/load state to/from config
    virtual void SaveState(class Config& config, int visIndex) = 0;
    virtual void LoadState(class Config& config, int visIndex) = 0;

protected:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    int m_width = 0;
    int m_height = 0;
};
