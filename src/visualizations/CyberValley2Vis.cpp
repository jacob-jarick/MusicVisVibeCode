#include "CyberValley2Vis.h"
#include "../Config.h"
#include <algorithm>
#include <cmath>

bool CyberValley2Vis::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) {
    m_device = device;
    m_context = context;
    m_width = width;
    m_height = height;
    return true;
}

void CyberValley2Vis::Cleanup() {
    // No resources to clean up for CyberValley2
}

void CyberValley2Vis::Update(float deltaTime, const AudioData& audioData, bool useNormalized,
                            ID3D11Buffer* vertexBuffer, ID3D11InputLayout* inputLayout,
                            ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader) {
    std::vector<Vertex> vertices;
    
    // Constants
    const float HORIZON_Y = 0.2f;  // 40% from top (NDC: 1.0 is top, -1.0 is bottom, so 0.2 is 40% down)
    const int NUM_MOUNTAIN_POINTS = 112;  // Points per side of mountain (higher resolution)
    const int NUM_DEPTH_LINES = 60;      // Number of lines going toward horizon (more lines for smoother scrolling)
    const float MAX_HEIGHT = 0.9f;       // Maximum mountain height (increased 50% for more dramatic peaks)
    const int NUM_FREQ_BINS = 224;       // Number of frequency bins to use (256 - 32 high-end bins)
    
    // CRITICAL: Capture spectrum peaks into our frozen history buffer at controlled rate
    // Draw 30 mountain lines per second, using highest values since last draw
    m_timeSinceLastLine += deltaTime;
    const float LINE_DRAW_INTERVAL = 0.0333333f;  // ~0.033 seconds = 30 lines per second
    
    if (m_timeSinceLastLine >= LINE_DRAW_INTERVAL) {
        m_timeSinceLastLine -= LINE_DRAW_INTERVAL;
        
        // Use SpectrumHighestSample which already tracks peak values over last 6 frames
        for (int i = 0; i < 256; i++) {
            m_mountainHistory[m_historyWriteIndex][i] = audioData.SpectrumHighestSample[i];
        }
        m_historyWriteIndex = (m_historyWriteIndex + 1) % 60;
    }
    
    // Update timers
    m_time += deltaTime;
    if (m_time > 600.0f) m_time -= 600.0f;  // 10 minute cycle
    
    // Speed: controls how fast lines move toward horizon
    // Speed is percentage (50% = 2 seconds to horizon, 100% = 1 second)
    // Convert percentage to scroll speed: distance/second = speed / 100
    float scrollSpeed = m_speed / 100.0f;  // 50% -> 0.5 units/sec -> 2 seconds to horizon
    m_gridOffset += scrollSpeed * deltaTime;
    if (m_gridOffset > 1.0f) m_gridOffset -= 1.0f;
    
    // Day/Night colors based on V key toggle
    XMFLOAT4 colorSkyTop, colorSkyBot, colorGround, colorGrid, colorSun;
    if (m_sunMode) {
        // Day Palette - Sunset Orange / Neon Pink
        colorSkyTop = {1.0f, 0.55f, 0.0f, 1.0f};    // Sunset Orange #FF8C00
        colorSkyBot = {1.0f, 0.0f, 0.5f, 1.0f};     // Neon Pink #FF0080
        colorGround = {0.1f, 0.0f, 0.1f, 1.0f};     // Dark purple ground
        colorGrid = {1.0f, 0.0f, 0.8f, 1.0f};       // Magenta #FF00CC
        colorSun = {1.0f, 1.0f, 0.0f, 1.0f};        // Yellow Sun
    } else {
        // Night Palette - Dark Indigo / Cyber Purple
        colorSkyTop = {0.0f, 0.0f, 0.2f, 1.0f};     // Dark Indigo #000033
        colorSkyBot = {0.5f, 0.0f, 0.8f, 1.0f};     // Cyber Purple #8000CC
        colorGround = {0.0f, 0.05f, 0.1f, 1.0f};    // Dark blue ground
        colorGrid = {0.0f, 1.0f, 1.0f, 1.0f};       // Cyan #00FFFF
        colorSun = {0.8f, 0.8f, 1.0f, 1.0f};        // Pale Moon
    }
    
    // Helper to add a quad (two triangles)
    auto AddQuad = [&](XMFLOAT3 tl, XMFLOAT3 tr, XMFLOAT3 bl, XMFLOAT3 br, XMFLOAT4 colTop, XMFLOAT4 colBot) {
        vertices.push_back({ tl, colTop, {-1.0f, -1.0f} });
        vertices.push_back({ tr, colTop, {-1.0f, -1.0f} });
        vertices.push_back({ bl, colBot, {-1.0f, -1.0f} });
        vertices.push_back({ tr, colTop, {-1.0f, -1.0f} });
        vertices.push_back({ br, colBot, {-1.0f, -1.0f} });
        vertices.push_back({ bl, colBot, {-1.0f, -1.0f} });
    };
    
    // Helper to add a line (thin quad)
    auto AddLine = [&](float x1, float y1, float x2, float y2, XMFLOAT4 col, float thickness = 0.003f) {
        // Calculate perpendicular offset for line thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 0.0001f) return;
        float nx = -dy / len * thickness;
        float ny = dx / len * thickness;
        
        vertices.push_back({ {x1 - nx, y1 - ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 + nx, y1 + ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - nx, y2 - ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x1 + nx, y1 + ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 + nx, y2 + ny, 0.5f}, col, {-1.0f, -1.0f} });
        vertices.push_back({ {x2 - nx, y2 - ny, 0.5f}, col, {-1.0f, -1.0f} });
    };
    
    // 1. DRAW SKY GRADIENT (from top to horizon)
    AddQuad(
        {-1.0f, 1.0f, 0.99f}, {1.0f, 1.0f, 0.99f},      // Top corners
        {-1.0f, HORIZON_Y, 0.99f}, {1.0f, HORIZON_Y, 0.99f},  // Horizon corners
        colorSkyTop, colorSkyBot
    );
    
    // 2. DRAW GROUND (from horizon to bottom) - dark asphalt road surface
    XMFLOAT4 roadSurfaceColor = {0.08f, 0.08f, 0.10f, 1.0f};  // Dark blue-gray asphalt
    AddQuad(
        {-1.0f, HORIZON_Y, 0.98f}, {1.0f, HORIZON_Y, 0.98f},
        {-1.0f, -1.0f, 0.98f}, {1.0f, -1.0f, 0.98f},
        roadSurfaceColor, roadSurfaceColor
    );
    
    // 2a. ATMOSPHERE EFFECTS
    if (m_sunMode) {
        // Day: Vaporwave clouds (drifting horizontally)
        int numClouds = 5;
        for (int c = 0; c < numClouds; c++) {
            float cloudSeed = (float)c * 123.456f;
            float cloudX = fmodf(cloudSeed + m_time * 0.05f, 2.0f) - 1.0f;  // Drift slowly
            float cloudY = HORIZON_Y + 0.15f + sinf(cloudSeed) * 0.15f;
            float cloudWidth = 0.2f + sinf(cloudSeed * 0.5f) * 0.1f;
            float cloudHeight = 0.05f;
            
            XMFLOAT4 cloudColor = {1.0f, 0.85f, 0.95f, 0.3f};  // Semi-transparent pink/white
            
            // Draw cloud as rounded ellipse (8 segments)
            for (int i = 0; i < 8; i++) {
                float angle1 = (float)i / 8.0f * 6.28318f;
                float angle2 = (float)(i + 1) / 8.0f * 6.28318f;
                float cx1 = cloudX + cosf(angle1) * cloudWidth;
                float cy1 = cloudY + sinf(angle1) * cloudHeight;
                float cx2 = cloudX + cosf(angle2) * cloudWidth;
                float cy2 = cloudY + sinf(angle2) * cloudHeight;
                
                vertices.push_back({ {cloudX, cloudY, 0.92f}, cloudColor, {-1.0f, -1.0f} });
                vertices.push_back({ {cx1, cy1, 0.92f}, cloudColor, {-1.0f, -1.0f} });
                vertices.push_back({ {cx2, cy2, 0.92f}, cloudColor, {-1.0f, -1.0f} });
            }
        }
    } else {
        // Night: Starfield effect - stars moving toward user (like mountains/road)
        int numStars = 80;
        for (int s = 0; s < numStars; s++) {
            float starSeed = (float)s * 0.123f;
            
            // Calculate star depth (0 = horizon, 1 = near camera)
            // Use grid offset to make stars scroll
            float starDepth = fmodf(starSeed * 10.0f + m_gridOffset * 2.0f, 1.0f);
            
            // Perspective: stars at horizon are small/converged, stars near camera are large/spread
            float perspScale = starDepth;  // 0 at horizon, 1 at camera
            
            // Base position in "world space"
            float baseX = (sinf(starSeed * 100.0f) * 2.0f - 1.0f);
            float baseY = (sinf(starSeed * 200.0f) * 0.5f + 0.5f);  // 0-1 range
            
            // Apply perspective
            float starX = baseX * perspScale;
            float starY = HORIZON_Y + baseY * (1.0f - HORIZON_Y) * perspScale;
            
            // Only draw stars above horizon
            if (starY > HORIZON_Y) {
                float twinkle = 0.3f + 0.7f * (sinf(m_time * 3.0f + starSeed * 50.0f) * 0.5f + 0.5f);
                float starSize = 0.002f + 0.003f * perspScale;  // Larger when closer
                
                XMFLOAT4 starColor = {1.0f, 1.0f, 1.0f, twinkle * (0.3f + 0.7f * perspScale)};  // Brighter when closer
                
                // Draw star as small diamond
                vertices.push_back({ {starX, starY + starSize, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX - starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX + starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
                
                vertices.push_back({ {starX, starY - starSize, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX - starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
                vertices.push_back({ {starX + starSize, starY, 0.92f}, starColor, {-1.0f, -1.0f} });
            }
        }
        
        // Shooting stars - also moving toward user in starfield
        float shootingStarPhase = fmodf(m_time, 3.0f);
        if (shootingStarPhase < 0.5f) {  // Active for 0.5 seconds
            float progress = shootingStarPhase / 0.5f;
            
            // Shooting star moves from horizon toward camera (depth increases)
            float shootDepth = progress;  // 0 at horizon, 1 at camera
            float perspScale = shootDepth;
            
            // Start position (varies with time for randomness)
            float baseX = sinf(floorf(m_time / 3.0f) * 12.34f) * 0.8f;
            float baseY = 0.6f + sinf(floorf(m_time / 3.0f) * 23.45f) * 0.3f;
            
            // Current position
            float shootX = baseX * perspScale;
            float shootY = HORIZON_Y + baseY * (1.0f - HORIZON_Y) * perspScale;
            
            // Previous position (slightly back in depth for trail)
            float prevDepth = shootDepth - 0.15f;
            if (prevDepth < 0.0f) prevDepth = 0.0f;
            float prevPerspScale = prevDepth;
            float tailX = baseX * prevPerspScale;
            float tailY = HORIZON_Y + baseY * (1.0f - HORIZON_Y) * prevPerspScale;
            
            // Only draw above horizon
            if (shootY > HORIZON_Y) {
                XMFLOAT4 shootColor = {1.0f, 1.0f, 0.8f, 1.0f};
                XMFLOAT4 tailColor = {1.0f, 1.0f, 0.8f, 0.4f};
                
                // Draw shooting star with trail following movement (from tail to head)
                AddLine(tailX, tailY, shootX, shootY, shootColor, 0.002f + 0.003f * perspScale);
            }
        }
    }
    
    // 3. DRAW SUN/MOON (static, centered on horizon with bottom 1/5th below horizon)
    float sunX = 0.0f;
    float sunRadius = 0.30f;  // Doubled from 0.15f
    float sunY = HORIZON_Y + sunRadius * 0.8f;  // Position so bottom 1/5th (0.2*radius) is below horizon
    float aspectRatio = (float)m_width / (float)m_height;  // Aspect ratio correction

    // Draw glow halo (larger, semi-transparent)
    int sunSegments = 48;
    float glowRadius = sunRadius * 1.8f;
    XMFLOAT4 glowColor = {colorSun.x, colorSun.y, colorSun.z, 0.15f};
    for (int i = 0; i < sunSegments; i++) {
        float theta1 = (float)i / sunSegments * 6.28318f;
        float theta2 = (float)(i + 1) / sunSegments * 6.28318f;

        vertices.push_back({ {sunX, sunY, 0.91f}, glowColor, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta1) * glowRadius / aspectRatio, sunY + sinf(theta1) * glowRadius, 0.91f}, glowColor, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta2) * glowRadius / aspectRatio, sunY + sinf(theta2) * glowRadius, 0.91f}, glowColor, {-1.0f, -1.0f} });
    }

    // Draw main orb with gradient (center brighter)
    XMFLOAT4 centerColor = {colorSun.x * 1.2f, colorSun.y * 1.2f, colorSun.z * 1.2f, 1.0f};
    if (centerColor.x > 1.0f) centerColor.x = 1.0f;
    if (centerColor.y > 1.0f) centerColor.y = 1.0f;
    if (centerColor.z > 1.0f) centerColor.z = 1.0f;

    for (int i = 0; i < sunSegments; i++) {
        float theta1 = (float)i / sunSegments * 6.28318f;
        float theta2 = (float)(i + 1) / sunSegments * 6.28318f;

        vertices.push_back({ {sunX, sunY, 0.9f}, centerColor, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta1) * sunRadius / aspectRatio, sunY + sinf(theta1) * sunRadius, 0.9f}, colorSun, {-1.0f, -1.0f} });
        vertices.push_back({ {sunX + cosf(theta2) * sunRadius / aspectRatio, sunY + sinf(theta2) * sunRadius, 0.9f}, colorSun, {-1.0f, -1.0f} });
    }

    // Vaporwave horizontal stripes (transparent gaps showing sky through)
    // Draw sky-colored stripes over the sun/moon to create "cut-out" effect
    int numStripes = 5;
    for (int s = 0; s < numStripes; s++) {
        // Position stripes in lower 40% of the circle (0.6 to 1.0 from center)
        float normalizedY = 0.6f + (s / (float)(numStripes - 1)) * 0.4f;
        float stripeY = sunY - sunRadius * normalizedY;

        // Calculate horizontal extent at this Y using circle equation
        // x² + y² = r², so x = sqrt(r² - y²)
        float dy = stripeY - sunY;
        if (fabsf(dy) < sunRadius) {
            float xExtent = sqrtf(sunRadius * sunRadius - dy * dy);

            // Use sky bottom color for transparent stripe effect (shows sky through gaps)
            XMFLOAT4 stripeColor = colorSkyBot;

            // Draw stripe as horizontal quad (thicker for visibility)
            float halfThickness = 0.008f;
            float xLeft = sunX - xExtent / aspectRatio;
            float xRight = sunX + xExtent / aspectRatio;

            // Draw as filled quad for better opacity
            vertices.push_back({ {xLeft, stripeY + halfThickness, 0.89f}, stripeColor, {-1.0f, -1.0f} });
            vertices.push_back({ {xRight, stripeY + halfThickness, 0.89f}, stripeColor, {-1.0f, -1.0f} });
            vertices.push_back({ {xLeft, stripeY - halfThickness, 0.89f}, stripeColor, {-1.0f, -1.0f} });

            vertices.push_back({ {xRight, stripeY + halfThickness, 0.89f}, stripeColor, {-1.0f, -1.0f} });
            vertices.push_back({ {xRight, stripeY - halfThickness, 0.89f}, stripeColor, {-1.0f, -1.0f} });
            vertices.push_back({ {xLeft, stripeY - halfThickness, 0.89f}, stripeColor, {-1.0f, -1.0f} });
        }
    }

    // 4. DRAW MOUNTAINS (audio-reactive valley)
    // For each depth row, we draw a flat spectrum line that forms the mountain profile
    // Lines closer to camera are at bottom, lines at horizon are at top
    
    float roadWidth = 0.15f;  // Half-width of road (matches road grid)
    
    for (int row = 0; row < NUM_DEPTH_LINES; row++) {
        // Calculate depth factor (0 = at camera/bottom, 1 = at horizon)
        float rawZ = (float)row / NUM_DEPTH_LINES;
        
        // Apply scrolling offset to position (makes lines move toward horizon at same speed as road)
        float z = rawZ + m_gridOffset;
        if (z >= 1.0f) z -= 1.0f;
        
        // FREEZE LOGIC: History lookup based on Z position, not row number
        // z=0 (bottom) shows most recent spectrum, z=1.0 (horizon) shows oldest
        // This ensures newest data always appears at bottom regardless of scrolling
        int histOffset = (int)(z * 59.0f);  // Map z (0-1) to history depth (0-59)
        if (histOffset < 0) histOffset = 0;
        if (histOffset >= 60) histOffset = 59;
        int histIdx = (m_historyWriteIndex - 1 - histOffset + 60) % 60;
        
        // Perspective: closer rows are spread wider, distant rows converge
        float perspectiveScale = 1.0f - z * 0.7f;  // 1.0 at camera, 0.3 at horizon (less aggressive pinch)
        
        // Calculate brightness fade: 100% at viewer (z=0), 33% at horizon (z=1)
        float brightness = 1.0f - z * 0.67f;
        XMFLOAT4 fadedColor = {colorGrid.x * brightness, colorGrid.y * brightness, colorGrid.z * brightness, colorGrid.w};
        
        // Y position: interpolate from bottom (-1) to horizon (0.2)
        float baseY = -1.0f + z * (HORIZON_Y + 1.0f);
        
        // Road edge at this depth
        float roadEdge = roadWidth * perspectiveScale;
        
        // Extend drawing range off-screen for closest lines (wider mountains at bottom)
        float extendFactor = 1.0f + (1.0f - z) * 0.2f;  // 1.2x at camera, 1.0x at horizon
        
        // Draw left mountain (from left screen edge to left road edge, extended off-screen when close)
        float prevX = -1.0f * perspectiveScale * extendFactor;
        float prevY = baseY;
        
        for (int i = 0; i <= NUM_MOUNTAIN_POINTS / 2; i++) {
            // X position from left screen edge to left road edge (extended range)
            float t = (float)i / (NUM_MOUNTAIN_POINTS / 2);
            float xScreen = -1.0f * perspectiveScale * extendFactor + t * (1.0f * perspectiveScale * extendFactor - roadEdge);
            
            // Map to frequency bin: bass at left edge, highs toward center
            // Left side: bins 0-111 (first half of 224 usable bins)
            int bin = (int)(t * 111.0f);
            if (bin < 0) bin = 0;
            if (bin > 111) bin = 111;
            
            // Get spectrum value from our frozen history buffer
            float specVal = m_mountainHistory[histIdx][bin];
            
            // Calculate height (flat line, only audio modulation)
            float audioHeight = specVal * MAX_HEIGHT;
            float totalHeight = audioHeight * perspectiveScale;
            
            // Final Y position (flat horizontal line at baseY + height)
            float yScreen = baseY + totalHeight;
            
            // Draw line segment from previous point
            if (i > 0) {
                AddLine(prevX, prevY, xScreen, yScreen, fadedColor, 0.002f * perspectiveScale + 0.001f);
            }
            
            prevX = xScreen;
            prevY = yScreen;
        }
        
        // Draw right mountain (from right road edge to right screen edge, extended off-screen when close)
        prevX = roadEdge;
        prevY = baseY;
        
        for (int i = 0; i <= NUM_MOUNTAIN_POINTS / 2; i++) {
            // X position from right road edge to right screen edge (extended range)
            float t = (float)i / (NUM_MOUNTAIN_POINTS / 2);
            float xScreen = roadEdge + t * (1.0f * perspectiveScale * extendFactor - roadEdge);
            
            // Map to frequency bin: mirror left side (highs at center, bass at right edge)
            // Right side: bins 111-0 (mirrored, creates valley shape)
            int bin = 111 - (int)(t * 111.0f);
            if (bin < 0) bin = 0;
            if (bin > 111) bin = 111;
            
            // Get spectrum value from our frozen history buffer
            float specVal = m_mountainHistory[histIdx][bin];
            
            // Calculate height (flat line, only audio modulation)
            float audioHeight = specVal * MAX_HEIGHT;
            float totalHeight = audioHeight * perspectiveScale;
            
            // Final Y position (flat horizontal line at baseY + height)
            float yScreen = baseY + totalHeight;
            
            // Draw line segment from previous point
            if (i > 0) {
                AddLine(prevX, prevY, xScreen, yScreen, fadedColor, 0.002f * perspectiveScale + 0.001f);
            }
            
            prevX = xScreen;
            prevY = yScreen;
        }
    }

    // ROAD EDGE LINES AND DUAL WHITE CENTER LINES (before grid rendering)
    float laneLineSpacing = 0.01f;  // Small gap between dual lines
    // roadWidth already declared earlier (0.15f)
    XMFLOAT4 whiteColor = {1.0f, 1.0f, 1.0f, 1.0f};

    int numLaneSegments = NUM_DEPTH_LINES / 2;  // 30 segments
    for (int i = 0; i < numLaneSegments; i++) {
        // Calculate depth with scrolling
        float z = (float)i / (float)numLaneSegments;
        z += m_gridOffset;
        if (z >= 1.0f) z -= 1.0f;

        // Y position and perspective
        float y = -1.0f + z * (HORIZON_Y + 1.0f);
        float perspScale = 1.0f - z * 0.7f;

        // Left center line (slightly left of center)
        float xLeft = -laneLineSpacing * perspScale;

        // Right center line (slightly right of center)
        float xRight = laneLineSpacing * perspScale;

        // Left edge line (at road boundary)
        float xEdgeLeft = -roadWidth * perspScale;

        // Right edge line (at road boundary)
        float xEdgeRight = roadWidth * perspScale;

        // Next segment position for line drawing
        float nextZ = (float)(i + 1) / (float)numLaneSegments + m_gridOffset;
        if (nextZ >= 1.0f) nextZ -= 1.0f;
        float nextY = -1.0f + nextZ * (HORIZON_Y + 1.0f);
        float nextPerspScale = 1.0f - nextZ * 0.7f;
        float nextXLeft = -laneLineSpacing * nextPerspScale;
        float nextXRight = laneLineSpacing * nextPerspScale;
        float nextXEdgeLeft = -roadWidth * nextPerspScale;
        float nextXEdgeRight = roadWidth * nextPerspScale;

        if (i > 0) {
            // Draw left lane line segment
            AddLine(xLeft, y, nextXLeft, nextY, whiteColor, 0.003f * perspScale);

            // Draw right lane line segment
            AddLine(xRight, y, nextXRight, nextY, whiteColor, 0.003f * perspScale);

            // Draw left edge line segment
            AddLine(xEdgeLeft, y, nextXEdgeLeft, nextY, whiteColor, 0.004f * perspScale);

            // Draw right edge line segment
            AddLine(xEdgeRight, y, nextXEdgeRight, nextY, whiteColor, 0.004f * perspScale);
        }

        // CATS EYES - Australian style dual pairs on each white line (every 3rd segment)
        if (i % 3 == 0) {
            float dotSize = 0.012f * perspScale;  // Bigger size for visibility

            XMFLOAT4 catsEyeGlow = {1.0f, 0.9f, 0.3f, 0.4f};  // Yellow glow (semi-transparent)
            XMFLOAT4 catsEyeCore = {1.0f, 1.0f, 0.8f, 1.0f};  // Bright yellowish white core

            // Draw pair on left white line
            float leftX = xLeft;
            // Outer glow layer (larger, transparent)
            float glowSize = dotSize * 2.5f;
            vertices.push_back({ {leftX, y + glowSize, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX - glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX + glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });

            vertices.push_back({ {leftX, y - glowSize, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX - glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX + glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });

            // Core reflector (smaller, bright)
            vertices.push_back({ {leftX, y + dotSize, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX - dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX + dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });

            vertices.push_back({ {leftX, y - dotSize, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX - dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {leftX + dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });

            // Draw pair on right white line
            float rightX = xRight;
            // Outer glow layer
            vertices.push_back({ {rightX, y + glowSize, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX - glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX + glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });

            vertices.push_back({ {rightX, y - glowSize, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX - glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX + glowSize, y, 0.47f}, catsEyeGlow, {-1.0f, -1.0f} });

            // Core reflector
            vertices.push_back({ {rightX, y + dotSize, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX - dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX + dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });

            vertices.push_back({ {rightX, y - dotSize, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX - dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
            vertices.push_back({ {rightX + dotSize, y, 0.46f}, catsEyeCore, {-1.0f, -1.0f} });
        }
    }

    // 5. DRAW ROAD GRID (center path) - soft overlay on asphalt
    if (m_showGrid) {
        float roadWidth = 0.15f;  // Half-width of road at camera

        // Soften grid color (30% opacity for subtle wireframe overlay)
        XMFLOAT4 softGridColor = {colorGrid.x * 0.3f, colorGrid.y * 0.3f, colorGrid.z * 0.3f, 1.0f};

        // Draw vertical lines (converging to center at horizon)
        int numRoadLines = 5;
        for (int i = 0; i < numRoadLines; i++) {
            float xOffset = (float)i / (numRoadLines - 1) * 2.0f - 1.0f;  // -1 to 1
            xOffset *= roadWidth;

            float xBottom = xOffset;
            float xTop = xOffset * 0.3f;  // Converge at horizon (matches mountain perspective: 1.0 - 1.0 * 0.7 = 0.3)

            AddLine(xBottom, -1.0f, xTop, HORIZON_Y, softGridColor, 0.002f);
        }

        // Draw horizontal lines (scrolling toward horizon)
        int numHorizLines = NUM_DEPTH_LINES / 4;  // 25% of mountain lines (15 lines instead of 60)
        for (int i = 0; i < numHorizLines; i++) {
            float z = (float)i / (float)numHorizLines;
            z += m_gridOffset;
            if (z >= 1.0f) z -= 1.0f;

            float y = -1.0f + z * (HORIZON_Y + 1.0f);
            float perspScale = 1.0f - z * 0.7f;  // Match mountain perspective (0.3 at horizon)
            float xLeft = -roadWidth * perspScale;
            float xRight = roadWidth * perspScale;

            AddLine(xLeft, y, xRight, y, softGridColor, 0.002f * perspScale + 0.0005f);
        }
    }
    
    // Submit all vertices
    if (!vertices.empty()) {
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
}

void CyberValley2Vis::HandleInput(WPARAM key) {
    if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        m_speed = std::max(5.0f, m_speed - 5.0f);  // Minimum 5% (very slow, 20s to horizon)
    } else if (key == VK_OEM_PLUS || key == VK_ADD) {
        m_speed = std::min(200.0f, m_speed + 5.0f);  // Maximum 200% (very fast, 0.5s to horizon)
    } else if (key == 'V') {
        m_sunMode = !m_sunMode;
    } else if (key == 'G') {
        m_showGrid = !m_showGrid;
    }
}

std::string CyberValley2Vis::GetHelpText() const {
    return "V: Toggle Sun/Moon\n"
           "G: Toggle Grid\n"
           "-/=: Adjust Speed";
}

void CyberValley2Vis::ResetToDefaults() {
    m_time = 0.0f;
    m_speed = 50.0f;
    m_gridOffset = 0.0f;
    m_sunMode = false;
    m_showGrid = true;
    m_historyWriteIndex = 0;
    m_timeSinceLastLine = 0.0f;
    for (int i = 0; i < 60; i++) {
        for (int j = 0; j < 256; j++) {
            m_mountainHistory[i][j] = 0.0f;
        }
    }
}

void CyberValley2Vis::SaveState(Config& config, int visIndex) {
    config.cv2Time = m_time;
    config.cv2Speed = m_speed;
    config.cv2SunMode = m_sunMode;
    config.cv2ShowGrid = m_showGrid;
}

void CyberValley2Vis::LoadState(Config& config, int visIndex) {
    m_time = config.cv2Time;
    m_speed = config.cv2Speed;
    m_sunMode = config.cv2SunMode;
    m_showGrid = config.cv2ShowGrid;
}
