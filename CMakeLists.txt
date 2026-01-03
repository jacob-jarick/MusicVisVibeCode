cmake_minimum_required(VERSION 3.10)

project(MusicVisVibeCode VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(MusicVisVibeCode ${SOURCES})

# Link libraries
if(WIN32)
    target_link_libraries(MusicVisVibeCode PRIVATE 
        d3d11 
        d3dcompiler 
        dxguid 
        ole32 
        user32 
        mmdevapi 
        dxgi
        gdi32
        gdiplus
    )
endif()
