#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <shlobj.h>

class Config {
public:
    Config();
    
    // Main settings
    bool useNormalized = true;
    bool isFullscreen = false;
    bool showBackground = false;
    bool clockEnabled = false;
    int currentBgIndex = -1;
    std::wstring currentBgPath;
    
    // Visualization states
    int currentVis = 0;  // 0=Spectrum, 1=CyberValley2, 2=LineFader, 3=Spectrum2, 4=Circle
    std::vector<bool> visEnabled;  // Track which visualizations are enabled
    
    // Spectrum settings
    float spectrumDecayRate = 5.0f;
    
    // CyberValley2 settings
    float cv2Time = 0.0f;
    float cv2Speed = 50.0f;
    bool cv2SunMode = false;
    bool cv2ShowGrid = true;
    
    // LineFader settings
    int lfScrollSpeed = 5;
    float lfFadeRate = 0.005f;
    int lfMirrorMode = 0;  // 0=None, 1=BassEdges, 2=BassCenter
    
    // Spectrum2 settings
    float s2DecayRate = 5.0f;
    int s2MirrorMode = 0;  // 0=None, 1=BassEdges, 2=BassCenter
    
    // Circle settings
    float circleRotationSpeed = 0.1f;  // Rotation speed in degrees per frame
    float circleFadeRate = 1.0f;       // Fade percentage (0-5%)
    float circleZoomRate = 1.0f;       // Zoom percentage (0-5%)
    float circleBlurRate = 1.0f;       // Blur percentage (0-10%)
    int circlePeakMode = 0;            // 0=Inside, 1=Outside, 2=Both
    bool circleZoomOut = false;        // false = zoom in, true = zoom out
    bool circleFillMode = false;       // false = line only, true = filled
    
    // Config management
    bool Load();
    bool Save();
    void Reset();
    std::wstring GetConfigPath();
    
    // Track if config has changed
    bool isDirty = false;
    float timeSinceLastSave = 0.0f;
    
private:
    std::wstring m_configPath;
    void EnsureConfigDirectory();
};
