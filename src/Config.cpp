#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

Config::Config() {
    // Initialize with 4 visualizations, all enabled by default
    visEnabled = {true, true, true, true};
    
    // Get config path
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        m_configPath = std::wstring(path) + L"\\.musicvibecode\\config.txt";
    }
}

void Config::EnsureConfigDirectory() {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        std::wstring dirPath = std::wstring(path) + L"\\.musicvibecode";
        CreateDirectoryW(dirPath.c_str(), NULL);
    }
}

std::wstring Config::GetConfigPath() {
    return m_configPath;
}

bool Config::Load() {
    // Convert wstring to string for file operations
    std::string configPathStr(m_configPath.begin(), m_configPath.end());
    
    std::ifstream file(configPathStr);
    if (!file.is_open()) {
        std::wcout << L"Config file not found at: " << m_configPath << L", using defaults" << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Parse key=value
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse settings
        if (key == "useNormalized") useNormalized = (value == "1" || value == "true");
        else if (key == "isFullscreen") isFullscreen = (value == "1" || value == "true");
        else if (key == "showBackground") showBackground = (value == "1" || value == "true");
        else if (key == "clockEnabled") clockEnabled = (value == "1" || value == "true");
        else if (key == "currentBgIndex") currentBgIndex = std::stoi(value);
        else if (key == "currentVis") currentVis = std::stoi(value);
        else if (key == "spectrumDecayRate") spectrumDecayRate = std::stof(value);
        else if (key == "cv2Time") cv2Time = std::stof(value);
        else if (key == "cv2Speed") cv2Speed = std::stof(value);
        else if (key == "cv2SunMode") cv2SunMode = (value == "1" || value == "true");
        else if (key == "cv2ShowGrid") cv2ShowGrid = (value == "1" || value == "true");
        else if (key == "lfScrollSpeed") lfScrollSpeed = std::stoi(value);
        else if (key == "lfFadeRate") lfFadeRate = std::stof(value);
        else if (key == "lfMirrorMode") lfMirrorMode = std::stoi(value);
        else if (key == "s2DecayRate") s2DecayRate = std::stof(value);
        else if (key == "s2MirrorMode") s2MirrorMode = std::stoi(value);
        else if (key == "visEnabled") {
            // Parse comma-separated list of 0/1
            visEnabled.clear();
            std::stringstream ss(value);
            std::string item;
            while (std::getline(ss, item, ',')) {
                visEnabled.push_back(item == "1" || item == "true");
            }
        }
    }
    
    file.close();
    isDirty = false;
    std::cout << "Config loaded successfully" << std::endl;
    return true;
}

bool Config::Save() {
    EnsureConfigDirectory();
    
    // Convert wstring to string for file operations
    std::string configPathStr(m_configPath.begin(), m_configPath.end());
    
    std::ofstream file(configPathStr);
    if (!file.is_open()) {
        std::wcerr << L"Failed to save config to: " << m_configPath << std::endl;
        return false;
    }
    
    file << "# MusicVisVibeCode Configuration\n\n";
    
    file << "# Main Settings\n";
    file << "useNormalized=" << (useNormalized ? "1" : "0") << "\n";
    file << "isFullscreen=" << (isFullscreen ? "1" : "0") << "\n";
    file << "showBackground=" << (showBackground ? "1" : "0") << "\n";
    file << "clockEnabled=" << (clockEnabled ? "1" : "0") << "\n";
    file << "currentBgIndex=" << currentBgIndex << "\n";
    file << "\n";
    
    file << "# Visualization Settings\n";
    file << "currentVis=" << currentVis << "\n";
    file << "visEnabled=";
    for (size_t i = 0; i < visEnabled.size(); i++) {
        if (i > 0) file << ",";
        file << (visEnabled[i] ? "1" : "0");
    }
    file << "\n\n";
    
    file << "# Spectrum Settings\n";
    file << "spectrumDecayRate=" << spectrumDecayRate << "\n\n";
    
    file << "# CyberValley2 Settings\n";
    file << "cv2Time=" << cv2Time << "\n";
    file << "cv2Speed=" << cv2Speed << "\n";
    file << "cv2SunMode=" << (cv2SunMode ? "1" : "0") << "\n";
    file << "cv2ShowGrid=" << (cv2ShowGrid ? "1" : "0") << "\n\n";
    
    file << "# LineFader Settings\n";
    file << "lfScrollSpeed=" << lfScrollSpeed << "\n";
    file << "lfFadeRate=" << lfFadeRate << "\n";
    file << "lfMirrorMode=" << lfMirrorMode << "\n\n";
    
    file << "# Spectrum2 Settings\n";
    file << "s2DecayRate=" << s2DecayRate << "\n";
    file << "s2MirrorMode=" << s2MirrorMode << "\n";
    
    file.close();
    isDirty = false;
    std::cout << "Config saved successfully" << std::endl;
    return true;
}

void Config::Reset() {
    // Reset to defaults
    useNormalized = true;
    isFullscreen = false;
    showBackground = false;
    clockEnabled = false;
    currentBgIndex = -1;
    currentBgPath = L"";
    
    currentVis = 0;
    visEnabled = {true, true, true, true};
    
    spectrumDecayRate = 5.0f;
    
    cv2Time = 0.0f;
    cv2Speed = 50.0f;
    cv2SunMode = false;
    cv2ShowGrid = true;
    
    lfScrollSpeed = 5;
    lfFadeRate = 0.005f;
    lfMirrorMode = 0;
    
    s2DecayRate = 5.0f;
    s2MirrorMode = 0;
    
    isDirty = true;
    std::cout << "Config reset to defaults" << std::endl;
}
