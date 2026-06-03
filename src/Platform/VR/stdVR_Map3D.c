#include "stdVR_Map3D.h"

#ifdef PLATFORM_VR

#include "stdPlatform.h"
#include "World/sithSector.h"
#include "World/sithSurface.h"
#include "World/sithWorld.h"
#include "World/sithThing.h"
#include "AI/sithAIClass.h"
#include "Gameplay/sithPlayer.h"
#include "Engine/sithRender.h"
#include "Engine/sithCamera.h"
#include "Engine/rdCamera.h"
#include "Primitives/rdMath.h"
#include "Primitives/rdMatrix.h"
#include "stdVR.h"

#include <math.h>

// VR debug logging
extern void VR_Log(const char* fmt, ...);

// GLES3 for direct line rendering
#ifdef TARGET_ANDROID_NATIVE_GLES
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#endif

// Simple line shader for VR map rendering
static GLuint stdVR_map3DShaderProgram = 0;
static GLuint stdVR_map3DVertexBuffer = 0;
static GLuint stdVR_map3DVertexArray = 0;  // VAO required for GLES3
static GLint stdVR_map3DUniformMVP = -1;
static GLint stdVR_map3DUniformEyeIndex = -1;  // For PC VR per-eye rendering
static int stdVR_map3DShaderInitted = 0;

// Vertex data for line rendering (position + color)
typedef struct {
    float x, y, z;
    float r, g, b, a;
} stdVR_Map3DVertex;

#define STDVR_MAP3D_MAX_VERTICES (STDVR_MAP3D_MAX_LINES * 2)
static stdVR_Map3DVertex stdVR_map3DVertices[STDVR_MAP3D_MAX_VERTICES];

// Map state
static stdVR_Map3DState stdVR_map3DState = {0};

// Line buffer
static stdVR_Map3DLine stdVR_map3DLines[STDVR_MAP3D_MAX_LINES];
static int stdVR_map3DNumLines = 0;

// Cached geometry (level edges - static, collected once when map opens)
static stdVR_Map3DLine stdVR_map3DCachedLines[STDVR_MAP3D_MAX_LINES];
static int stdVR_map3DNumCachedLines = 0;
static int stdVR_map3DCacheValid = 0;
static int stdVR_map3DLastCacheSectorId = -1;  // Track sector for cache invalidation

// Cached activatable surface positions (collected once during cache update)
#define STDVR_MAP3D_MAX_ACTIVATABLES 256
static rdVector3 stdVR_map3DActivatables[STDVR_MAP3D_MAX_ACTIVATABLES];
static int stdVR_map3DNumActivatables = 0;

// Cached visited sector IDs (to avoid conflict with game's renderTick usage)
#define STDVR_MAP3D_MAX_VISITED_SECTORS 512
static int stdVR_map3DVisitedSectors[STDVR_MAP3D_MAX_VISITED_SECTORS];
static int stdVR_map3DNumVisitedSectors = 0;

// Depth-based color gradient settings (game units - levels use small coordinates)
static float stdVR_map3DGradientNear = 0.0f;     // Distance where green is brightest
static float stdVR_map3DGradientFar = 10.0f;     // Distance where green reaches minimum
static uint8_t stdVR_map3DColorNear = 0xFF;      // Green channel value at near distance (255)
static uint8_t stdVR_map3DColorFar = 0x60;       // Green channel value at far distance (80)

// Grid floor settings
static int stdVR_map3DShowGrid = 1;             // Show grid floor
static float stdVR_map3DGridSize = 10.0f;       // Grid cell size in game units
static float stdVR_map3DGridExtent = 50.0f;     // Grid extends this far from player
static uint32_t stdVR_map3DGridColor = 0x40008000;  // Dark green, 25% alpha (AABBGGRR)

// Visual frame settings
static int stdVR_map3DShowFrame = 1;            // Show frame around map
static uint32_t stdVR_map3DFrameColor = 0xFF00FF80;  // Cyan-ish glow color

// Holographic transparency (0-255, lower = more transparent)
static uint8_t stdVR_map3DFloorAlpha = 0xCC;    // Floor lines (80% opaque)
static uint8_t stdVR_map3DWallAlpha = 0x99;     // Wall lines (60% opaque)

// Wall color (different from floor for visual distinction)
static uint8_t stdVR_map3DWallColorNear = 0x80; // Blue channel for walls (near)
static uint8_t stdVR_map3DWallColorFar = 0x40;  // Blue channel for walls (far)

// Zoom animation settings
static float stdVR_map3DTargetZoom = 1.4f;      // Target zoom level
static float stdVR_map3DZoomAnimDuration = 0.5f; // Animation duration in seconds
static uint32_t stdVR_map3DAnimStartTime = 0;   // Timestamp when animation started
static int stdVR_map3DZoomAnimating = 0;        // Is zoom-in animation in progress
static int stdVR_map3DClosing = 0;              // Is map closing (zoom-out animation)
static float stdVR_map3DCloseStartZoom = 1.0f;  // Zoom level when close started

// 3D marker rotation (for enemy/pickup diamonds)
static float stdVR_map3DMarkerRotation = 0.0f;  // Current rotation angle in radians
static float stdVR_map3DMarkerRotationSpeed = 3.14159f; // Radians per second (PI = 0.5 rotation/sec)

// Configuration (in game units)
// Note: Level coordinates are typically small (single digits), player height ~0.12 units
static float stdVR_map3DDistance = 0.5f;    // Distance from player (game units)
static float stdVR_map3DHeight = -0.1f;     // Height below eye level (game units)
static float stdVR_map3DDefaultScale = 0.3f; // Scale factor - keep map reasonably sized

// Player look direction for positioning map in front of player
static rdVector3 stdVR_map3DLookDir = {0};

// Map rotation (controlled by left thumbstick when map is visible)
static float stdVR_map3DRotation = 0.0f;  // Rotation angle in degrees

// Map zoom (controlled by right thumbstick Y when map is visible)
static float stdVR_map3DZoom = 1.0f;  // Zoom multiplier (1.0 = default)
#define STDVR_MAP3D_ZOOM_MIN 0.1f
#define STDVR_MAP3D_ZOOM_MAX 8.0f

// Two-handed gesture state for map manipulation
static int stdVR_map3DGestureActive = 0;
static float stdVR_map3DGestureStartDist = 0.0f;      // Initial distance between controllers
static float stdVR_map3DGestureStartAngle = 0.0f;     // Initial angle between controllers (horizontal)
static float stdVR_map3DGestureStartZoom = 1.0f;      // Zoom when gesture started
static float stdVR_map3DGestureStartRotation = 0.0f;  // Rotation when gesture started
static rdVector3 stdVR_map3DGestureStartMidpoint = {0}; // Midpoint when gesture started
static rdVector3 stdVR_map3DGestureStartAnchor = {0}; // Anchor position when gesture started

// Map anchor position in JKDF2 tracking space (world-fixed)
static rdVector3 stdVR_map3DAnchorPos = {0};  // Where the map center is in JKDF2 coords
static int stdVR_map3DAnchorSet = 0;          // Has anchor been initialized?

// Tick counter for sector visit tracking
static int stdVR_map3DRenderTick = 0;

// Forward declarations
static void stdVR_Map3D_CollectSectorEdges(sithSector* pSector, int depth);
static void stdVR_Map3D_CollectSurfaceEdges(sithSector* pSector, sithSurface* pSurface);
static void stdVR_Map3D_AddLine(rdVector3* v1, rdVector3* v2, uint32_t color);
static void stdVR_Map3D_AddCachedLine(rdVector3* v1, rdVector3* v2, uint32_t color);
static uint32_t stdVR_Map3D_GetDepthColor(float depth, int bIsFloor);
static void stdVR_Map3D_TransformToWorld(rdVector3* pOut, rdVector3* pIn);
static void stdVR_Map3D_InitShader(void);
static void stdVR_Map3D_ShutdownShader(void);
static void stdVR_Map3D_CollectEntityMarkers(void);
static void stdVR_Map3D_AddGridAndFrame(void);
static void stdVR_Map3D_UpdateCachedGeometry(void);
static void stdVR_Map3D_Draw3DDiamond(rdVector3* pCenter, float size, uint32_t color, float rotationRad);
static int stdVR_Map3D_IsSectorVisited(sithSector* pSector);

// Vertex shader for colored lines - single-pass MultiView (renders both eyes via
// gl_ViewID_OVR into the multiview array FBO). Used on Quest and desktop PCVR alike; only
// the GLSL #version (and GLES precision qualifier) differ by platform.
static const char* stdVR_map3DVertexShaderSrc =
#ifdef TARGET_ANDROID_NATIVE_GLES
    "#version 300 es\n"
#else
    "#version 330 core\n"
#endif
    "#extension GL_OVR_multiview2 : enable\n"
    "#define NUM_VIEWS 2\n"
    "layout(num_views = NUM_VIEWS) in;\n"
#ifdef TARGET_ANDROID_NATIVE_GLES
    "precision highp float;\n"
#endif
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "uniform mat4 uMVP[2];\n"  // Per-eye MVP matrices, indexed by gl_ViewID_OVR
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = uMVP[gl_ViewID_OVR] * vec4(aPos, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

// Fragment shader for colored lines
static const char* stdVR_map3DFragmentShaderSrc =
#ifdef TARGET_ANDROID_NATIVE_GLES
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vColor;\n"
    "}\n";
#else
    "#version 330 core\n"
    "in vec4 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vColor;\n"
    "}\n";
#endif

static void stdVR_Map3D_InitShader(void)
{
    if (stdVR_map3DShaderInitted) return;

    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &stdVR_map3DVertexShaderSrc, NULL);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        stdPlatform_Printf("stdVR_Map3D: Vertex shader compile error: %s\n", infoLog);
        return;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &stdVR_map3DFragmentShaderSrc, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        stdPlatform_Printf("stdVR_Map3D: Fragment shader compile error: %s\n", infoLog);
        glDeleteShader(vertexShader);
        return;
    }

    // Link program
    stdVR_map3DShaderProgram = glCreateProgram();
    glAttachShader(stdVR_map3DShaderProgram, vertexShader);
    glAttachShader(stdVR_map3DShaderProgram, fragmentShader);
    glLinkProgram(stdVR_map3DShaderProgram);

    glGetProgramiv(stdVR_map3DShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(stdVR_map3DShaderProgram, 512, NULL, infoLog);
        stdPlatform_Printf("stdVR_Map3D: Shader program link error: %s\n", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        stdVR_map3DShaderProgram = 0;
        return;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Get uniform location for MVP array
    stdVR_map3DUniformMVP = glGetUniformLocation(stdVR_map3DShaderProgram, "uMVP");

#ifndef TARGET_ANDROID_NATIVE_GLES
    // PC VR: Get uniform location for eye index (used for per-eye rendering)
    stdVR_map3DUniformEyeIndex = glGetUniformLocation(stdVR_map3DShaderProgram, "uEyeIndex");
#endif

    // Clear any pending errors
    while (glGetError() != GL_NO_ERROR) {}

    // Save current VAO binding (critical: main renderer uses VAO 1, not 0)
    GLint prevVAO;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

    // Create VAO for our line rendering
    glGenVertexArrays(1, &stdVR_map3DVertexArray);
    glBindVertexArray(stdVR_map3DVertexArray);

    // Create vertex buffer and set up attributes in the VAO
    glGenBuffers(1, &stdVR_map3DVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, stdVR_map3DVertexBuffer);
    glEnableVertexAttribArray(0);  // aPos
    glEnableVertexAttribArray(1);  // aColor
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(stdVR_Map3DVertex), (void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(stdVR_Map3DVertex), (void*)(3 * sizeof(float)));

    // Restore previous VAO (critical for main renderer)
    glBindVertexArray(prevVAO);

    stdVR_map3DShaderInitted = 1;
    stdPlatform_Printf("stdVR_Map3D: Shader initialized (VAO=%u)\n", stdVR_map3DVertexArray);
}

static void stdVR_Map3D_ShutdownShader(void)
{
    if (stdVR_map3DVertexArray) {
        glDeleteVertexArrays(1, &stdVR_map3DVertexArray);
        stdVR_map3DVertexArray = 0;
    }
    if (stdVR_map3DVertexBuffer) {
        glDeleteBuffers(1, &stdVR_map3DVertexBuffer);
        stdVR_map3DVertexBuffer = 0;
    }
    if (stdVR_map3DShaderProgram) {
        glDeleteProgram(stdVR_map3DShaderProgram);
        stdVR_map3DShaderProgram = 0;
    }
    stdVR_map3DShaderInitted = 0;
}

// ============================================================================
// Public API
// ============================================================================

void stdVR_Map3D_Startup(void)
{
    memset(&stdVR_map3DState, 0, sizeof(stdVR_map3DState));
    stdVR_map3DState.scale = stdVR_map3DDefaultScale;
    stdVR_map3DState.position.x = 0.0f;
    stdVR_map3DState.position.y = stdVR_map3DHeight;
    stdVR_map3DState.position.z = 0.0f;
    stdVR_map3DState.bInitialized = 1;

    stdPlatform_Printf("stdVR_Map3D: Initialized (scale=%.3f, dist=%.1f, height=%.1f)\n",
        stdVR_map3DState.scale, stdVR_map3DDistance, stdVR_map3DHeight);
}

void stdVR_Map3D_Shutdown(void)
{
    stdVR_Map3D_ShutdownShader();
    stdVR_map3DState.bInitialized = 0;
    stdVR_map3DState.bEnabled = 0;
    stdVR_map3DNumLines = 0;
}

void stdVR_Map3D_Toggle(void)
{
    // If currently closing, ignore toggle (let animation finish)
    if (stdVR_map3DClosing) {
        return;
    }

    if (!stdVR_map3DState.bEnabled) {
        // Opening the map
        stdVR_map3DState.bEnabled = 1;
        // Initialize anchor position in front of the user when map is enabled
        // Get current head pose to place map relative to where user is looking
        rdMatrix34 eyePose;
        stdVR_GetEyeViewMatrix(0, &eyePose);

        // Place anchor in front of the head along the look direction
        // eyePose is in JKDF2 space: scale=position, lvec=forward direction
        float mapDistance = 0.55f;   // Distance from head (closer)
        float mapDropHeight = 0.25f; // How much lower than eye level
        stdVR_map3DAnchorPos.x = eyePose.scale.x + eyePose.lvec.x * mapDistance;
        stdVR_map3DAnchorPos.y = eyePose.scale.y + eyePose.lvec.y * mapDistance;
        stdVR_map3DAnchorPos.z = eyePose.scale.z + eyePose.lvec.z * mapDistance - mapDropHeight;

        stdVR_map3DAnchorSet = 1;

        // Orient map so player arrow points forward (same direction user is facing)
        // Calculate player's current yaw and set map rotation to compensate
        if (sithPlayer_pLocalPlayerThing) {
            rdMatrix34* pOrient = &sithPlayer_pLocalPlayerThing->lookOrientation;
            float playerYaw = atan2f(pOrient->lvec.x, pOrient->lvec.y) * (180.0f / 3.14159265f);
            stdVR_map3DRotation = playerYaw;  // Arrow points away from user
        } else {
            stdVR_map3DRotation = 0.0f;
        }

        // Start zoom-in animation from 0 to target
        stdVR_map3DZoom = 0.01f;     // Start nearly invisible
        stdVR_map3DAnimStartTime = stdPlatform_GetTimeMsec();
        stdVR_map3DZoomAnimating = 1;
        stdVR_map3DClosing = 0;

        // Invalidate cache so geometry is recollected fresh
        stdVR_map3DCacheValid = 0;
        stdVR_map3DLastCacheSectorId = -1;

        stdPlatform_Printf("stdVR_Map3D: Opening\n");
    } else {
        // Closing the map - start zoom-out animation
        stdVR_map3DCloseStartZoom = stdVR_map3DZoom;
        stdVR_map3DAnimStartTime = stdPlatform_GetTimeMsec();
        stdVR_map3DClosing = 1;
        stdVR_map3DZoomAnimating = 0;
        stdPlatform_Printf("stdVR_Map3D: Closing\n");
    }
}

int stdVR_Map3D_IsVisible(void)
{
    // Visible if enabled OR if closing animation is in progress
    return stdVR_map3DState.bEnabled || stdVR_map3DClosing;
}

void stdVR_Map3D_SetScale(float scale)
{
    stdVR_map3DState.scale = scale;
}

void stdVR_Map3D_SetDistance(float distance)
{
    stdVR_map3DDistance = distance;
}

void stdVR_Map3D_SetHeight(float height)
{
    stdVR_map3DHeight = height;
    stdVR_map3DState.position.y = height;
}

void stdVR_Map3D_Rotate(float deltaAngle)
{
    stdVR_map3DRotation += deltaAngle;
    // Keep in 0-360 range
    while (stdVR_map3DRotation >= 360.0f) stdVR_map3DRotation -= 360.0f;
    while (stdVR_map3DRotation < 0.0f) stdVR_map3DRotation += 360.0f;
}

void stdVR_Map3D_Zoom(float deltaZoom)
{
    stdVR_map3DZoom *= (1.0f + deltaZoom);
    // Clamp to valid range
    if (stdVR_map3DZoom < STDVR_MAP3D_ZOOM_MIN) stdVR_map3DZoom = STDVR_MAP3D_ZOOM_MIN;
    if (stdVR_map3DZoom > STDVR_MAP3D_ZOOM_MAX) stdVR_map3DZoom = STDVR_MAP3D_ZOOM_MAX;
}

void stdVR_Map3D_ProcessGestures(int bothGripsHeld, rdVector3* pLeftPos, rdVector3* pRightPos)
{
    if (!stdVR_map3DState.bEnabled) {
        stdVR_map3DGestureActive = 0;
        return;
    }

    if (bothGripsHeld && pLeftPos && pRightPos) {
        // Controller positions are in JKDF2 space: X=right, Y=forward, Z=up
        // Calculate horizontal distance between controllers (XY plane, ignore Z/vertical)
        float dx = pRightPos->x - pLeftPos->x;
        float dy = pRightPos->y - pLeftPos->y;
        float horizontalDist = sqrtf(dx*dx + dy*dy);

        // Also track full 3D distance for scaling
        float dz = pRightPos->z - pLeftPos->z;
        float fullDist = sqrtf(dx*dx + dy*dy + dz*dz);

        // Calculate current angle between controllers in horizontal plane only (XY)
        // This makes rotation only respond to twisting hands, not lifting
        float currentAngle = atan2f(dx, dy) * (180.0f / 3.14159265f);

        // Calculate current midpoint (in JKDF2 space)
        rdVector3 currentMidpoint;
        currentMidpoint.x = (pLeftPos->x + pRightPos->x) * 0.5f;
        currentMidpoint.y = (pLeftPos->y + pRightPos->y) * 0.5f;
        currentMidpoint.z = (pLeftPos->z + pRightPos->z) * 0.5f;

        if (!stdVR_map3DGestureActive) {
            // Gesture just started - store initial state
            stdVR_map3DGestureActive = 1;
            stdVR_map3DGestureStartDist = fullDist;
            stdVR_map3DGestureStartAngle = currentAngle;
            stdVR_map3DGestureStartZoom = stdVR_map3DZoom;
            stdVR_map3DGestureStartRotation = stdVR_map3DRotation;
            stdVR_map3DGestureStartMidpoint = currentMidpoint;
            stdVR_map3DGestureStartAnchor = stdVR_map3DAnchorPos;
        } else {
            // Gesture ongoing - apply transformations

            // Scale: ratio of current distance to start distance
            if (stdVR_map3DGestureStartDist > 0.01f) {
                float scaleRatio = fullDist / stdVR_map3DGestureStartDist;
                stdVR_map3DZoom = stdVR_map3DGestureStartZoom * scaleRatio;
                // Clamp zoom
                if (stdVR_map3DZoom < STDVR_MAP3D_ZOOM_MIN) stdVR_map3DZoom = STDVR_MAP3D_ZOOM_MIN;
                if (stdVR_map3DZoom > STDVR_MAP3D_ZOOM_MAX) stdVR_map3DZoom = STDVR_MAP3D_ZOOM_MAX;
            }

            // Rotation: difference in horizontal angle only
            // Only apply rotation if controllers are reasonably separated horizontally
            if (horizontalDist > 0.05f) {
                float angleDelta = currentAngle - stdVR_map3DGestureStartAngle;
                // Handle angle wraparound
                while (angleDelta > 180.0f) angleDelta -= 360.0f;
                while (angleDelta < -180.0f) angleDelta += 360.0f;

                stdVR_map3DRotation = stdVR_map3DGestureStartRotation - angleDelta;
                // Keep in 0-360 range
                while (stdVR_map3DRotation >= 360.0f) stdVR_map3DRotation -= 360.0f;
                while (stdVR_map3DRotation < 0.0f) stdVR_map3DRotation += 360.0f;
            }

            // Translation: move anchor position in JKDF2 space
            // Movement is 1:1 with controller movement
            stdVR_map3DAnchorPos.x = stdVR_map3DGestureStartAnchor.x +
                (currentMidpoint.x - stdVR_map3DGestureStartMidpoint.x);
            stdVR_map3DAnchorPos.y = stdVR_map3DGestureStartAnchor.y +
                (currentMidpoint.y - stdVR_map3DGestureStartMidpoint.y);
            stdVR_map3DAnchorPos.z = stdVR_map3DGestureStartAnchor.z +
                (currentMidpoint.z - stdVR_map3DGestureStartMidpoint.z);
        }
    } else {
        // Grips released - anchor position persists in JKDF2 space
        stdVR_map3DGestureActive = 0;
    }
}

int stdVR_Map3D_ShouldPauseGame(void)
{
    // Pause game logic when map is visible (including during close animation)
    return (stdVR_map3DState.bEnabled || stdVR_map3DClosing) && stdVR_map3DState.bInitialized;
}

// ============================================================================
// Update - Collect geometry from level (with caching for performance)
// ============================================================================

// Update cached static geometry (level edges and activatable surfaces)
static void stdVR_Map3D_UpdateCachedGeometry(void)
{
    if (!sithPlayer_pLocalPlayerThing || !sithWorld_pCurrentWorld) return;

    sithSector* pPlayerSector = sithPlayer_pLocalPlayerThing->sector;
    if (!pPlayerSector) return;

    // Clear cached buffers
    stdVR_map3DNumCachedLines = 0;
    stdVR_map3DNumActivatables = 0;
    stdVR_map3DNumVisitedSectors = 0;

    // Increment tick for visit tracking during collection
    stdVR_map3DRenderTick++;

    // Recursively collect edges from sectors
    stdVR_Map3D_CollectSectorEdges(pPlayerSector, 0);

    // Collect activatable surface positions from visited sectors
    rdVector3* pVertices = sithWorld_pCurrentWorld->vertices;
    if (pVertices) {
        for (int s = 0; s < sithWorld_pCurrentWorld->numSectors; s++) {
            sithSector* pSector = &sithWorld_pCurrentWorld->sectors[s];

            // Skip sectors not visited during geometry collection
            if (!stdVR_Map3D_IsSectorVisited(pSector)) continue;

            for (int i = 0; i < pSector->numSurfaces; i++) {
                sithSurface* pSurface = &pSector->surfaces[i];

                // Only collect COG-linked surfaces (activatable switches, panels, etc.)
                // Skip scrolling surfaces - those are animated screens, not activatables
                if (!(pSurface->surfaceFlags & SITH_SURFACE_COG_LINKED)) continue;
                if (pSurface->surfaceFlags & SITH_SURFACE_SCROLLING) continue;

                // Calculate centroid and bounding box of surface vertices
                int numVerts = pSurface->surfaceInfo.face.numVertices;
                int* pVertexIdxs = pSurface->surfaceInfo.face.vertexPosIdx;
                if (numVerts < 3 || !pVertexIdxs) continue;

                rdVector3 centroid = {0, 0, 0};
                rdVector3 minBounds = {1e30f, 1e30f, 1e30f};
                rdVector3 maxBounds = {-1e30f, -1e30f, -1e30f};

                for (int v = 0; v < numVerts; v++) {
                    int idx = pVertexIdxs[v];
                    rdVector3* pVert = &pVertices[idx];
                    centroid.x += pVert->x;
                    centroid.y += pVert->y;
                    centroid.z += pVert->z;

                    if (pVert->x < minBounds.x) minBounds.x = pVert->x;
                    if (pVert->y < minBounds.y) minBounds.y = pVert->y;
                    if (pVert->z < minBounds.z) minBounds.z = pVert->z;
                    if (pVert->x > maxBounds.x) maxBounds.x = pVert->x;
                    if (pVert->y > maxBounds.y) maxBounds.y = pVert->y;
                    if (pVert->z > maxBounds.z) maxBounds.z = pVert->z;
                }
                centroid.x /= numVerts;
                centroid.y /= numVerts;
                centroid.z /= numVerts;

                // Filter by size - switches are very small (< 0.15 game units in any dimension)
                // Large surfaces like screens, doors, or decorative panels are skipped
                float sizeX = maxBounds.x - minBounds.x;
                float sizeY = maxBounds.y - minBounds.y;
                float sizeZ = maxBounds.z - minBounds.z;
                float maxSize = sizeX > sizeY ? sizeX : sizeY;
                maxSize = maxSize > sizeZ ? maxSize : sizeZ;

                if (maxSize > 0.15f) continue;  // Skip large surfaces

                // Filter by orientation - switches are on walls (vertical surfaces)
                // Vertical surfaces have mostly horizontal normals (small Z component)
                rdVector3* pNormal = &pSurface->surfaceInfo.face.normal;
                float absNormalZ = pNormal->z > 0 ? pNormal->z : -pNormal->z;
                if (absNormalZ > 0.15f) continue;  // Skip floors/ceilings

                // Store in cache
                if (stdVR_map3DNumActivatables < STDVR_MAP3D_MAX_ACTIVATABLES) {
                    stdVR_map3DActivatables[stdVR_map3DNumActivatables++] = centroid;
                }
            }
        }
    }

    // Mark cache as valid
    stdVR_map3DCacheValid = 1;
    stdVR_map3DLastCacheSectorId = (int)(pPlayerSector - sithWorld_pCurrentWorld->sectors);

    stdPlatform_Printf("stdVR_Map3D: Cached %d level lines, %d activatables (sector %d)\n",
        stdVR_map3DNumCachedLines, stdVR_map3DNumActivatables, stdVR_map3DLastCacheSectorId);
}

// Collect entity markers (player, enemies, items) - called every frame
static void stdVR_Map3D_CollectEntityMarkers(void)
{
    // Add player position marker (bright yellow cross + direction arrow)
    {
        rdVector3 playerPos = stdVR_map3DState.playerPos;
        float markerSize = 0.5f;  // Size in game units
        uint32_t markerColor = 0xFF00FFFF;  // Bright yellow (AABBGGRR)
        uint32_t arrowColor = 0xFF0000FF;   // Red for direction

        // Cross at player position
        rdVector3 v1, v2;

        // Horizontal line (X axis)
        v1.x = playerPos.x - markerSize;
        v1.y = playerPos.y;
        v1.z = playerPos.z;
        v2.x = playerPos.x + markerSize;
        v2.y = playerPos.y;
        v2.z = playerPos.z;
        stdVR_Map3D_AddLine(&v1, &v2, markerColor);

        // Vertical line (Y axis in game = forward)
        v1.x = playerPos.x;
        v1.y = playerPos.y - markerSize;
        v1.z = playerPos.z;
        v2.x = playerPos.x;
        v2.y = playerPos.y + markerSize;
        v2.z = playerPos.z;
        stdVR_Map3D_AddLine(&v1, &v2, markerColor);

        // Direction arrow (combining game character orientation + physical HMD rotation)
        // Game character yaw (from snap turns, etc.)
        float characterYawRad = stdVR_map3DState.playerYaw * (3.14159265f / 180.0f);
        // HMD physical rotation (from tracking)
        rdMatrix34 headPose;
        stdVR_GetEyeViewMatrix(0, &headPose);
        float hmdYawRad = atan2f(headPose.lvec.x, headPose.lvec.y);
        // Combined total yaw
        float totalYawRad = characterYawRad + hmdYawRad;
        float arrowLen = markerSize * 1.5f;
        float arrowDirX = sinf(totalYawRad) * arrowLen;
        float arrowDirY = cosf(totalYawRad) * arrowLen;

        v1 = playerPos;
        v2.x = playerPos.x + arrowDirX;
        v2.y = playerPos.y + arrowDirY;
        v2.z = playerPos.z;
        stdVR_Map3D_AddLine(&v1, &v2, arrowColor);

        // Arrow head - lines pointing back from the tip at ~150 degrees from forward
        float headSize = markerSize * 0.4f;
        float headAngle = 0.5f;  // ~30 degrees from the reverse direction
        float headAngle1 = totalYawRad + 3.14159265f - headAngle;  // back-left
        float headAngle2 = totalYawRad + 3.14159265f + headAngle;  // back-right

        v1 = v2;  // Arrow tip
        rdVector3 v3;
        v3.x = v2.x + sinf(headAngle1) * headSize;
        v3.y = v2.y + cosf(headAngle1) * headSize;
        v3.z = v2.z;
        stdVR_Map3D_AddLine(&v1, &v3, arrowColor);

        v3.x = v2.x + sinf(headAngle2) * headSize;
        v3.y = v2.y + cosf(headAngle2) * headSize;
        v3.z = v2.z;
        stdVR_Map3D_AddLine(&v1, &v3, arrowColor);
    }

    // Draw enemy markers (rotating 3D red diamonds for hostile living actors)
    if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->things) {
        uint32_t enemyColor = 0xFF0000FF;  // Bright red (AABBGGRR)
        float enemyMarkerSize = 0.12f;      // Size in game units

        for (int i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded; i++) {
            sithThing* pThing = &sithWorld_pCurrentWorld->things[i];

            // Skip non-actors and dead/disabled things
            if (pThing->type != SITH_THING_ACTOR) continue;
            if (pThing->thingflags & (SITH_TF_DEAD | SITH_TF_DISABLED)) continue;
            if (!pThing->sector) continue;  // Not in world

            // Skip things in sectors not visited during cache collection
            if (!stdVR_Map3D_IsSectorVisited(pThing->sector)) continue;

            // Skip friendly/neutral NPCs (no AIClass or alignment >= 0)
            // Hostile enemies have negative alignment
            if (!pThing->pAIClass || pThing->pAIClass->alignment >= 0.0f) continue;

            // Draw rotating 3D diamond marker at enemy position
            rdVector3 pos = pThing->position;
            stdVR_Map3D_Draw3DDiamond(&pos, enemyMarkerSize, enemyColor, stdVR_map3DMarkerRotation);
        }
    }

    // Draw pickup markers (rotating 3D blue diamonds for uncollected items)
    if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->things) {
        uint32_t pickupColor = 0xFFFF8000;  // Light blue (AABBGGRR)
        float pickupMarkerSize = 0.08f;      // Smaller than enemy markers

        for (int i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded; i++) {
            sithThing* pThing = &sithWorld_pCurrentWorld->things[i];

            // Skip non-items and disabled things
            if (pThing->type != SITH_THING_ITEM) continue;
            if (pThing->thingflags & SITH_TF_DISABLED) continue;
            if (!pThing->sector) continue;  // Not in world

            // Skip things in sectors not visited during cache collection
            if (!stdVR_Map3D_IsSectorVisited(pThing->sector)) continue;

            // Draw rotating 3D diamond marker at pickup position
            // Offset up so bottom point sits at floor level
            rdVector3 pos = pThing->position;
            pos.z += pickupMarkerSize;
            stdVR_Map3D_Draw3DDiamond(&pos, pickupMarkerSize, pickupColor, stdVR_map3DMarkerRotation);
        }
    }

    // Draw activatable thing markers (yellow diamonds for COG-type things like switches)
    if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->things) {
        uint32_t activateColor = 0xFF00FFFF;  // Bright yellow (AABBGGRR)
        float activateMarkerSize = 0.06f;      // Smaller than pickups

        for (int i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded; i++) {
            sithThing* pThing = &sithWorld_pCurrentWorld->things[i];

            // Only show COG-type things (switches, buttons, intractable objects)
            if (pThing->type != SITH_THING_COG) continue;
            if (pThing->thingflags & SITH_TF_DISABLED) continue;
            if (!pThing->sector) continue;  // Not in world

            // Skip things in sectors not visited during cache collection
            if (!stdVR_Map3D_IsSectorVisited(pThing->sector)) continue;

            // Draw rotating 3D diamond marker at activatable position
            rdVector3 pos = pThing->position;
            pos.z += activateMarkerSize;  // Offset up so bottom point sits at thing's base
            stdVR_Map3D_Draw3DDiamond(&pos, activateMarkerSize, activateColor, stdVR_map3DMarkerRotation);
        }
    }

    // Draw activatable surface markers from cached positions (yellow diamonds)
    {
        uint32_t activateColor = 0xFF00FFFF;  // Bright yellow (AABBGGRR)
        float activateMarkerSize = 0.06f;      // Smaller than pickups

        for (int i = 0; i < stdVR_map3DNumActivatables; i++) {
            rdVector3 pos = stdVR_map3DActivatables[i];
            stdVR_Map3D_Draw3DDiamond(&pos, activateMarkerSize, activateColor, stdVR_map3DMarkerRotation);
        }
    }
}

// Add grid floor and frame around map
static void stdVR_Map3D_AddGridAndFrame(void)
{
    rdVector3 playerPos = stdVR_map3DState.playerPos;
    rdVector3 v1, v2;

    // Grid floor at player Z level
    if (stdVR_map3DShowGrid) {
        float gridZ = playerPos.z;
        float gridExtent = stdVR_map3DGridExtent;
        float gridSize = stdVR_map3DGridSize;
        uint32_t gridColor = stdVR_map3DGridColor;

        // Calculate grid boundaries centered on player
        float minX = playerPos.x - gridExtent;
        float maxX = playerPos.x + gridExtent;
        float minY = playerPos.y - gridExtent;
        float maxY = playerPos.y + gridExtent;

        // Snap to grid
        minX = floorf(minX / gridSize) * gridSize;
        maxX = ceilf(maxX / gridSize) * gridSize;
        minY = floorf(minY / gridSize) * gridSize;
        maxY = ceilf(maxY / gridSize) * gridSize;

        // Draw X-parallel lines (running along X axis)
        for (float y = minY; y <= maxY; y += gridSize) {
            v1.x = minX; v1.y = y; v1.z = gridZ;
            v2.x = maxX; v2.y = y; v2.z = gridZ;
            stdVR_Map3D_AddLine(&v1, &v2, gridColor);
        }

        // Draw Y-parallel lines (running along Y axis)
        for (float x = minX; x <= maxX; x += gridSize) {
            v1.x = x; v1.y = minY; v1.z = gridZ;
            v2.x = x; v2.y = maxY; v2.z = gridZ;
            stdVR_Map3D_AddLine(&v1, &v2, gridColor);
        }
    }

    // Holographic frame around the map bounds
    if (stdVR_map3DShowFrame && stdVR_map3DCacheValid && stdVR_map3DNumCachedLines > 0) {
        // Calculate bounds from cached geometry
        float minX = 1e30f, maxX = -1e30f;
        float minY = 1e30f, maxY = -1e30f;
        float minZ = 1e30f, maxZ = -1e30f;

        for (int i = 0; i < stdVR_map3DNumCachedLines; i++) {
            stdVR_Map3DLine* pLine = &stdVR_map3DCachedLines[i];

            if (pLine->start.x < minX) minX = pLine->start.x;
            if (pLine->start.x > maxX) maxX = pLine->start.x;
            if (pLine->end.x < minX) minX = pLine->end.x;
            if (pLine->end.x > maxX) maxX = pLine->end.x;

            if (pLine->start.y < minY) minY = pLine->start.y;
            if (pLine->start.y > maxY) maxY = pLine->start.y;
            if (pLine->end.y < minY) minY = pLine->end.y;
            if (pLine->end.y > maxY) maxY = pLine->end.y;

            if (pLine->start.z < minZ) minZ = pLine->start.z;
            if (pLine->start.z > maxZ) maxZ = pLine->start.z;
            if (pLine->end.z < minZ) minZ = pLine->end.z;
            if (pLine->end.z > maxZ) maxZ = pLine->end.z;
        }

        // Add small padding to bounds
        float pad = 1.0f;
        minX -= pad; maxX += pad;
        minY -= pad; maxY += pad;
        minZ -= pad; maxZ += pad;

        uint32_t frameColor = stdVR_map3DFrameColor;

        // Draw 12 edges of a bounding box
        // Bottom face (z = minZ)
        v1.x = minX; v1.y = minY; v1.z = minZ; v2.x = maxX; v2.y = minY; v2.z = minZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = maxX; v1.y = minY; v1.z = minZ; v2.x = maxX; v2.y = maxY; v2.z = minZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = maxX; v1.y = maxY; v1.z = minZ; v2.x = minX; v2.y = maxY; v2.z = minZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = minX; v1.y = maxY; v1.z = minZ; v2.x = minX; v2.y = minY; v2.z = minZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);

        // Top face (z = maxZ)
        v1.x = minX; v1.y = minY; v1.z = maxZ; v2.x = maxX; v2.y = minY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = maxX; v1.y = minY; v1.z = maxZ; v2.x = maxX; v2.y = maxY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = maxX; v1.y = maxY; v1.z = maxZ; v2.x = minX; v2.y = maxY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = minX; v1.y = maxY; v1.z = maxZ; v2.x = minX; v2.y = minY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);

        // Vertical edges
        v1.x = minX; v1.y = minY; v1.z = minZ; v2.x = minX; v2.y = minY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = maxX; v1.y = minY; v1.z = minZ; v2.x = maxX; v2.y = minY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = maxX; v1.y = maxY; v1.z = minZ; v2.x = maxX; v2.y = maxY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
        v1.x = minX; v1.y = maxY; v1.z = minZ; v2.x = minX; v2.y = maxY; v2.z = maxZ;
        stdVR_Map3D_AddLine(&v1, &v2, frameColor);
    }
}

void stdVR_Map3D_Update(void)
{
    static int updateDebugCount = 0;

    // Allow updates if enabled OR if closing animation is in progress
    if ((!stdVR_map3DState.bEnabled && !stdVR_map3DClosing) || !stdVR_map3DState.bInitialized) {
        return;
    }

    // Update zoom-in animation (opening)
    if (stdVR_map3DZoomAnimating) {
        uint32_t currentTime = stdPlatform_GetTimeMsec();
        float elapsed = (currentTime - stdVR_map3DAnimStartTime) / 1000.0f;
        float t = elapsed / stdVR_map3DZoomAnimDuration;

        if (t >= 1.0f) {
            // Animation complete
            stdVR_map3DZoom = stdVR_map3DTargetZoom;
            stdVR_map3DZoomAnimating = 0;
        } else {
            // Smooth ease-out interpolation: 1 - (1-t)^2
            float easeT = 1.0f - (1.0f - t) * (1.0f - t);
            stdVR_map3DZoom = 0.01f + easeT * (stdVR_map3DTargetZoom - 0.01f);
        }
    }

    // Update zoom-out animation (closing)
    if (stdVR_map3DClosing) {
        uint32_t currentTime = stdPlatform_GetTimeMsec();
        float elapsed = (currentTime - stdVR_map3DAnimStartTime) / 1000.0f;
        float t = elapsed / stdVR_map3DZoomAnimDuration;

        if (t >= 1.0f) {
            // Animation complete - actually close the map
            stdVR_map3DZoom = 0.01f;
            stdVR_map3DClosing = 0;
            stdVR_map3DState.bEnabled = 0;
            stdPlatform_Printf("stdVR_Map3D: Closed\n");
            return;  // Don't process further this frame
        } else {
            // Smooth ease-in interpolation: t^2 (starts slow, ends fast)
            float easeT = t * t;
            stdVR_map3DZoom = stdVR_map3DCloseStartZoom * (1.0f - easeT);
            if (stdVR_map3DZoom < 0.01f) stdVR_map3DZoom = 0.01f;
        }
    }

    // Update 3D marker rotation (continuous spin)
    {
        static uint32_t lastMarkerUpdateTime = 0;
        uint32_t currentTime = stdPlatform_GetTimeMsec();
        if (lastMarkerUpdateTime == 0) lastMarkerUpdateTime = currentTime;
        float deltaTime = (currentTime - lastMarkerUpdateTime) / 1000.0f;
        lastMarkerUpdateTime = currentTime;

        stdVR_map3DMarkerRotation += stdVR_map3DMarkerRotationSpeed * deltaTime;
        // Keep in 0 to 2*PI range
        while (stdVR_map3DMarkerRotation >= 6.28318f) stdVR_map3DMarkerRotation -= 6.28318f;
    }

    // Need a valid player and world
    if (!sithPlayer_pLocalPlayerThing || !sithWorld_pCurrentWorld) {
        if (updateDebugCount < 5) {
            updateDebugCount++;
            stdPlatform_Printf("stdVR_Map3D_Update: No player (%p) or world (%p)\n",
                sithPlayer_pLocalPlayerThing, sithWorld_pCurrentWorld);
        }
        return;
    }

    sithSector* pPlayerSector = sithPlayer_pLocalPlayerThing->sector;
    if (!pPlayerSector) {
        if (updateDebugCount < 5) {
            updateDebugCount++;
            stdPlatform_Printf("stdVR_Map3D_Update: No player sector\n");
        }
        return;
    }

    // Store player position for centering
    stdVR_map3DState.playerPos = sithPlayer_pLocalPlayerThing->position;

    // Get player look direction from orientation matrix (lvec is forward)
    rdMatrix34* pOrient = &sithPlayer_pLocalPlayerThing->lookOrientation;
    stdVR_map3DLookDir = pOrient->lvec;

    // Get player yaw from look orientation
    stdVR_map3DState.playerYaw = atan2f(pOrient->lvec.x, pOrient->lvec.y) * (180.0f / 3.14159265f);

    // Check if cache needs update (first open or sector changed)
    int currentSectorId = (int)(pPlayerSector - sithWorld_pCurrentWorld->sectors);
    if (!stdVR_map3DCacheValid || currentSectorId != stdVR_map3DLastCacheSectorId) {
        if (updateDebugCount < 10) {
            updateDebugCount++;
            stdPlatform_Printf("stdVR_Map3D_Update: Updating cache (sector %d -> %d)\n",
                stdVR_map3DLastCacheSectorId, currentSectorId);
        }
        stdVR_Map3D_UpdateCachedGeometry();
    }

    // Clear dynamic line buffer (for this frame)
    stdVR_map3DNumLines = 0;

    // Copy cached geometry to render buffer
    for (int i = 0; i < stdVR_map3DNumCachedLines; i++) {
        stdVR_map3DLines[stdVR_map3DNumLines++] = stdVR_map3DCachedLines[i];
    }

    // Add entity markers (player, enemies, items) - these update every frame
    stdVR_Map3D_CollectEntityMarkers();

    // Add grid floor and frame
    stdVR_Map3D_AddGridAndFrame();

    // Debug: log how many lines collected (only first few frames)
    static int collectDebugCount = 0;
    if (collectDebugCount < 5) {
        collectDebugCount++;
        stdPlatform_Printf("stdVR_Map3D_Update: %d cached + %d dynamic = %d total lines\n",
            stdVR_map3DNumCachedLines, stdVR_map3DNumLines - stdVR_map3DNumCachedLines,
            stdVR_map3DNumLines);
    }
}

// ============================================================================
// Geometry Collection
// ============================================================================

static void stdVR_Map3D_CollectSectorEdges(sithSector* pSector, int depth)
{
    if (!pSector || depth > STDVR_MAP3D_MAX_DEPTH) {
        return;
    }

    // Skip if already visited this frame
    if (pSector->renderTick == stdVR_map3DRenderTick) {
        return;
    }
    pSector->renderTick = stdVR_map3DRenderTick;

    // Store sector ID in our visited list (for entity visibility checks later)
    if (stdVR_map3DNumVisitedSectors < STDVR_MAP3D_MAX_VISITED_SECTORS) {
        stdVR_map3DVisitedSectors[stdVR_map3DNumVisitedSectors++] = pSector->id;
    }

    // Check visibility flags (unless showing all for debug)
    if (!(pSector->flags & SITH_SECTOR_AUTOMAPVISIBLE)) {
        // Still recurse through adjoins to find visible sectors
        for (int i = 0; i < pSector->numSurfaces; i++) {
            sithSurface* pSurface = &pSector->surfaces[i];
            if (pSurface->adjoin && pSurface->adjoin->sector) {
                stdVR_Map3D_CollectSectorEdges(pSurface->adjoin->sector, depth + 1);
            }
        }
        return;
    }

    // Skip hidden sectors
    if (pSector->flags & SITH_SECTOR_AUTOMAPHIDE) {
        return;
    }

    // Collect edges from all non-adjoin surfaces (walls, floors, ceilings)
    // Adjoin surfaces are portals to other sectors - skip them to avoid duplicate edges
    for (int i = 0; i < pSector->numSurfaces; i++) {
        sithSurface* pSurface = &pSector->surfaces[i];

        // Skip adjoin surfaces (portals) - they don't have visible geometry
        if (pSurface->adjoin) {
            continue;
        }

        // Draw all solid surfaces (walls, floors, ceilings)
        stdVR_Map3D_CollectSurfaceEdges(pSector, pSurface);
    }

    // Recurse to adjacent sectors
    for (int i = 0; i < pSector->numSurfaces; i++) {
        sithSurface* pSurface = &pSector->surfaces[i];
        if (pSurface->adjoin && pSurface->adjoin->sector) {
            stdVR_Map3D_CollectSectorEdges(pSurface->adjoin->sector, depth + 1);
        }
    }
}

static void stdVR_Map3D_CollectSurfaceEdges(sithSector* pSector, sithSurface* pSurface)
{
    int numVerts = pSurface->surfaceInfo.face.numVertices;
    if (!pSector || !pSurface || numVerts < 2) {
        return;
    }

    // Get vertex buffer from world
    rdVector3* pVertices = sithWorld_pCurrentWorld->vertices;
    if (!pVertices) {
        return;
    }

    // Get vertex index array from surface
    int* pVertexIdxs = pSurface->surfaceInfo.face.vertexPosIdx;
    if (!pVertexIdxs) {
        return;
    }

    // Determine if this is a floor surface (for color selection)
    int bIsFloor = (pSurface->surfaceFlags & SITH_SURFACE_FLOOR) != 0;

    // Draw edges around the surface
    for (int i = 0; i < numVerts; i++) {
        int idx1 = pVertexIdxs[i];
        int idx2 = pVertexIdxs[(i + 1) % numVerts];

        rdVector3 v1 = pVertices[idx1];
        rdVector3 v2 = pVertices[idx2];

        // Calculate depth as distance from player (using midpoint)
        rdVector3 mid;
        mid.x = (v1.x + v2.x) * 0.5f - stdVR_map3DState.playerPos.x;
        mid.y = (v1.y + v2.y) * 0.5f - stdVR_map3DState.playerPos.y;
        mid.z = (v1.z + v2.z) * 0.5f - stdVR_map3DState.playerPos.z;
        float depth = sqrtf(mid.x * mid.x + mid.y * mid.y + mid.z * mid.z);

        uint32_t color = stdVR_Map3D_GetDepthColor(depth, bIsFloor);

        // Add to CACHED line buffer (static geometry)
        stdVR_Map3D_AddCachedLine(&v1, &v2, color);
    }
}

static void stdVR_Map3D_AddCachedLine(rdVector3* v1, rdVector3* v2, uint32_t color)
{
    if (stdVR_map3DNumCachedLines >= STDVR_MAP3D_MAX_LINES) {
        return;
    }

    stdVR_Map3DLine* pLine = &stdVR_map3DCachedLines[stdVR_map3DNumCachedLines++];
    pLine->start = *v1;
    pLine->end = *v2;
    pLine->color = color;
}

static void stdVR_Map3D_AddLine(rdVector3* v1, rdVector3* v2, uint32_t color)
{
    if (stdVR_map3DNumLines >= STDVR_MAP3D_MAX_LINES) {
        return;
    }

    stdVR_Map3DLine* pLine = &stdVR_map3DLines[stdVR_map3DNumLines++];
    pLine->start = *v1;
    pLine->end = *v2;
    pLine->color = color;
}

// Draw a rotating 3D diamond (octahedron) marker
static void stdVR_Map3D_Draw3DDiamond(rdVector3* pCenter, float size, uint32_t color, float rotationRad)
{
    // Octahedron vertices (before rotation):
    // Middle plane: N(0,+1,0), E(+1,0,0), S(0,-1,0), W(-1,0,0)
    // Top: (0,0,+1), Bottom: (0,0,-1)

    float cosR = cosf(rotationRad);
    float sinR = sinf(rotationRad);

    // Middle vertices use reduced diameter (2/3 of size)
    float midSize = size * 0.67f;

    // Calculate rotated middle vertices (rotate around Z axis)
    rdVector3 vN, vE, vS, vW, vTop, vBottom;

    // North vertex (0, +midSize, 0) rotated
    vN.x = pCenter->x + (-sinR * midSize);
    vN.y = pCenter->y + (cosR * midSize);
    vN.z = pCenter->z;

    // East vertex (+midSize, 0, 0) rotated
    vE.x = pCenter->x + (cosR * midSize);
    vE.y = pCenter->y + (sinR * midSize);
    vE.z = pCenter->z;

    // South vertex (0, -midSize, 0) rotated
    vS.x = pCenter->x + (sinR * midSize);
    vS.y = pCenter->y + (-cosR * midSize);
    vS.z = pCenter->z;

    // West vertex (-midSize, 0, 0) rotated
    vW.x = pCenter->x + (-cosR * midSize);
    vW.y = pCenter->y + (-sinR * midSize);
    vW.z = pCenter->z;

    // Top and bottom vertices (on Z axis, full size, no rotation needed)
    vTop.x = pCenter->x;
    vTop.y = pCenter->y;
    vTop.z = pCenter->z + size;

    vBottom.x = pCenter->x;
    vBottom.y = pCenter->y;
    vBottom.z = pCenter->z - size;

    // Draw 12 edges of the octahedron
    // Middle square edges (4)
    stdVR_Map3D_AddLine(&vN, &vE, color);
    stdVR_Map3D_AddLine(&vE, &vS, color);
    stdVR_Map3D_AddLine(&vS, &vW, color);
    stdVR_Map3D_AddLine(&vW, &vN, color);

    // Top to middle vertices (4)
    stdVR_Map3D_AddLine(&vTop, &vN, color);
    stdVR_Map3D_AddLine(&vTop, &vE, color);
    stdVR_Map3D_AddLine(&vTop, &vS, color);
    stdVR_Map3D_AddLine(&vTop, &vW, color);

    // Bottom to middle vertices (4)
    stdVR_Map3D_AddLine(&vBottom, &vN, color);
    stdVR_Map3D_AddLine(&vBottom, &vE, color);
    stdVR_Map3D_AddLine(&vBottom, &vS, color);
    stdVR_Map3D_AddLine(&vBottom, &vW, color);
}

// Check if a sector was visited during cache collection
static int stdVR_Map3D_IsSectorVisited(sithSector* pSector)
{
    if (!pSector) return 0;
    int sectorId = pSector->id;
    for (int i = 0; i < stdVR_map3DNumVisitedSectors; i++) {
        if (stdVR_map3DVisitedSectors[i] == sectorId) return 1;
    }
    return 0;
}

static uint32_t stdVR_Map3D_GetDepthColor(float depth, int bIsFloor)
{
    // Smooth gradient based on distance from player
    float t;
    if (depth <= stdVR_map3DGradientNear) {
        t = 0.0f;  // Full brightness
    } else if (depth >= stdVR_map3DGradientFar) {
        t = 1.0f;  // Minimum brightness
    } else {
        // Smooth interpolation between near and far
        t = (depth - stdVR_map3DGradientNear) / (stdVR_map3DGradientFar - stdVR_map3DGradientNear);
    }

    uint8_t red, green, blue, alpha;

    if (bIsFloor) {
        // Floor surfaces: green color with transparency
        green = (uint8_t)(stdVR_map3DColorNear + t * (stdVR_map3DColorFar - stdVR_map3DColorNear));
        red = 0;
        blue = 0;
        alpha = stdVR_map3DFloorAlpha;
    } else {
        // Wall surfaces: cyan/blue-ish color, more transparent
        green = (uint8_t)(stdVR_map3DColorNear * 0.5f + t * (stdVR_map3DColorFar * 0.5f - stdVR_map3DColorNear * 0.5f));
        blue = (uint8_t)(stdVR_map3DWallColorNear + t * (stdVR_map3DWallColorFar - stdVR_map3DWallColorNear));
        red = 0;
        alpha = stdVR_map3DWallAlpha;
    }

    // Return color in AABBGGRR format
    return ((uint32_t)alpha << 24) | ((uint32_t)blue << 16) | ((uint32_t)green << 8) | red;
}

// ============================================================================
// Rendering
// ============================================================================

// Transform game vertex to JKDF2 tracking space (model transform only)
// Coordinate conversion to OpenGL happens in the view matrix
static void stdVR_Map3D_TransformToWorld(rdVector3* pOut, rdVector3* pIn)
{
    // Center on player position (in game coordinates)
    float x = pIn->x - stdVR_map3DState.playerPos.x;
    float y = pIn->y - stdVR_map3DState.playerPos.y;
    float z = pIn->z - stdVR_map3DState.playerPos.z;

    // Apply user rotation around Z axis (vertical axis in game coords)
    float rotRad = stdVR_map3DRotation * (3.14159265f / 180.0f);
    float cosR = cosf(rotRad);
    float sinR = sinf(rotRad);
    float rx = x * cosR - y * sinR;
    float ry = x * sinR + y * cosR;
    x = rx;
    y = ry;

    // Apply scale (shrink the map to a reasonable size in meters)
    float mapScale = 0.035f * stdVR_map3DZoom;
    x *= mapScale;
    y *= mapScale;
    z *= mapScale;

    // Add anchor offset to get world position (JKDF2 tracking space)
    // Keep in JKDF2 coords - view matrix will handle conversion
    pOut->x = stdVR_map3DAnchorPos.x + x;
    pOut->y = stdVR_map3DAnchorPos.y + y;
    pOut->z = stdVR_map3DAnchorPos.z + z;
}

// Helper to multiply two 4x4 matrices (column-major): result = a * b
static void stdVR_Map3D_MultMatrix44(float* result, float* a, float* b)
{
    float temp[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            temp[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    memcpy(result, temp, 16 * sizeof(float));
}

// Build per-eye asymmetric projection matrix (all VR platforms).
// Each eye gets its own projection based on that eye's FOV - matches the scene's real per-eye
// GPU projection and the native per-eye FOV the eye buffer is submitted with.
static void stdVR_Map3D_GetAsymmetricProjection(float* pOut16, int eyeIdx, float zNear, float zFar)
{
    // Use this specific eye's FOV tangents
    float fovLeft = stdVR_clientInfo.eyes[eyeIdx].fovLeft;
    float fovRight = stdVR_clientInfo.eyes[eyeIdx].fovRight;
    float fovUp = stdVR_clientInfo.eyes[eyeIdx].fovUp;
    float fovDown = stdVR_clientInfo.eyes[eyeIdx].fovDown;

    float left = -fovLeft * zNear;
    float right = fovRight * zNear;
    float bottom = -fovDown * zNear;
    float top = fovUp * zNear;

    float width = right - left;
    float height = top - bottom;

    // Column-major 4x4 asymmetric perspective matrix
    memset(pOut16, 0, 16 * sizeof(float));
    pOut16[0] = 2.0f * zNear / width;
    pOut16[5] = 2.0f * zNear / height;
    pOut16[8] = (right + left) / width;
    pOut16[9] = (top + bottom) / height;
    pOut16[10] = -(zFar + zNear) / (zFar - zNear);
    pOut16[11] = -1.0f;
    pOut16[14] = -2.0f * zFar * zNear / (zFar - zNear);
    pOut16[15] = 0.0f;
}

void stdVR_Map3D_Render(int eye)
{
    // Render if enabled OR if closing animation is in progress
    if ((!stdVR_map3DState.bEnabled && !stdVR_map3DClosing) || stdVR_map3DNumLines == 0) {
        return;
    }


    // Initialize shader on first render
    if (!stdVR_map3DShaderInitted) {
        stdVR_Map3D_InitShader();
        if (!stdVR_map3DShaderInitted) {
            return;  // Shader init failed
        }
    }

    // Build MVP matrices for world-fixed rendering
    int numVertices = stdVR_map3DNumLines * 2;
    float mvpMatrices[32];  // 2 x 4x4 = 32 floats


    // Get eye poses from VR system (in JKDF2 space)
    rdMatrix34 eyePoses[2];
    stdVR_GetEyeViewMatrix(0, &eyePoses[0]);
    stdVR_GetEyeViewMatrix(1, &eyePoses[1]);


    // Transform vertices to OpenGL world space (model transform already applied)
    for (int i = 0; i < stdVR_map3DNumLines; i++) {
        stdVR_Map3DLine* pLine = &stdVR_map3DLines[i];

        rdVector3 start, end;
        stdVR_Map3D_TransformToWorld(&start, &pLine->start);
        stdVR_Map3D_TransformToWorld(&end, &pLine->end);

        float r = ((pLine->color >> 0) & 0xFF) / 255.0f;
        float g = ((pLine->color >> 8) & 0xFF) / 255.0f;
        float b = ((pLine->color >> 16) & 0xFF) / 255.0f;
        float a = ((pLine->color >> 24) & 0xFF) / 255.0f;
        if (a < 0.1f) a = 1.0f;

        stdVR_map3DVertices[i * 2 + 0].x = start.x;
        stdVR_map3DVertices[i * 2 + 0].y = start.y;
        stdVR_map3DVertices[i * 2 + 0].z = start.z;
        stdVR_map3DVertices[i * 2 + 0].r = r;
        stdVR_map3DVertices[i * 2 + 0].g = g;
        stdVR_map3DVertices[i * 2 + 0].b = b;
        stdVR_map3DVertices[i * 2 + 0].a = a;

        stdVR_map3DVertices[i * 2 + 1].x = end.x;
        stdVR_map3DVertices[i * 2 + 1].y = end.y;
        stdVR_map3DVertices[i * 2 + 1].z = end.z;
        stdVR_map3DVertices[i * 2 + 1].r = r;
        stdVR_map3DVertices[i * 2 + 1].g = g;
        stdVR_map3DVertices[i * 2 + 1].b = b;
        stdVR_map3DVertices[i * 2 + 1].a = a;
    }

    // Build MVP for each eye
    for (int eyeIdx = 0; eyeIdx < 2; eyeIdx++) {
        float projMat[16];
        float viewMat[16];
        float* mvpMat = &mvpMatrices[eyeIdx * 16];

        float zNear = 0.05f;
        float zFar = 50.0f;

        // Real per-eye asymmetric projection - matches the scene's GPU per-eye projection and the
        // NATIVE per-eye FOV the eye buffer is now submitted with. (Was the union "symmetric"
        // projection to match the OLD union-rendered scene; now the scene projects per-eye for
        // real, so the holomap must too or it renders at the wrong scale/convergence.)
        stdVR_Map3D_GetAsymmetricProjection(projMat, eyeIdx, zNear, zFar);

        // Build view matrix: JKDF2 view rotation + coordinate conversion
        // Combined rotation = CoordConvert * JKDF2_ViewRotation
        // JKDF2 view rotation has rows: rvec, lvec, uvec
        // After coord convert: rows become rvec, uvec, -lvec
        rdMatrix34* pEye = &eyePoses[eyeIdx];
        float rx = pEye->rvec.x, ry = pEye->rvec.y, rz = pEye->rvec.z;
        float fx = pEye->lvec.x, fy = pEye->lvec.y, fz = pEye->lvec.z;
        float ux = pEye->uvec.x, uy = pEye->uvec.y, uz = pEye->uvec.z;
        float px = pEye->scale.x, py = pEye->scale.y, pz = pEye->scale.z;

        // Rotation part (column-major): rows are rvec, uvec, -lvec
        viewMat[0] = rx;   viewMat[4] = ry;   viewMat[8]  = rz;
        viewMat[1] = ux;   viewMat[5] = uy;   viewMat[9]  = uz;
        viewMat[2] = -fx;  viewMat[6] = -fy;  viewMat[10] = -fz;
        viewMat[3] = 0.0f; viewMat[7] = 0.0f; viewMat[11] = 0.0f;

        // Translation: compute eye pos in JKDF2 eye-local, then convert to GL
        float localX = rx*px + ry*py + rz*pz;  // dot(rvec, pos)
        float localY = fx*px + fy*py + fz*pz;  // dot(lvec, pos)
        float localZ = ux*px + uy*py + uz*pz;  // dot(uvec, pos)

        // Convert to OpenGL and negate for view matrix
        viewMat[12] = -localX;      // GL.x = -JK_local.x
        viewMat[13] = -localZ;      // GL.y = -JK_local.z
        viewMat[14] = localY;       // GL.z = JK_local.y (double negative)
        viewMat[15] = 1.0f;

        // MVP = Projection * View
        //stdVR_Map3D_MultMatrix44(mvpMat, projMat, viewMat);
        stdVR_Map3D_MultMatrix44(mvpMat, projMat, viewMat);
    }

    // Save GL state (comprehensive to avoid corrupting main renderer)
    GLint prevProgram;
    GLint prevVAO;
    GLint prevVBO;
    GLint prevFBO;
    GLint prevBlendEnabled;
    GLint prevDepthTestEnabled;
    GLint prevDepthMask;
    GLint prevBlendSrcRGB, prevBlendDstRGB;
    GLint prevCullFace;
    GLint prevViewport[4];
    GLint prevScissorBox[4];
    GLint prevScissorEnabled;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    prevBlendEnabled = glIsEnabled(GL_BLEND);
    prevDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    prevCullFace = glIsEnabled(GL_CULL_FACE);
    prevScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &prevDepthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRGB);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);

    // Also save element buffer binding (std3D uses indexed drawing)
    GLint prevEBO;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevEBO);

    // Clear any pending GL errors
    while (glGetError() != GL_NO_ERROR) {}

    // Set up our rendering state
    glUseProgram(stdVR_map3DShaderProgram);

    // Upload both MVP matrices (array of 2 mat4)
    glUniformMatrix4fv(stdVR_map3DUniformMVP, 2, GL_FALSE, mvpMatrices);

#ifndef TARGET_ANDROID_NATIVE_GLES
    // PC VR: Set eye index for per-eye rendering
    if (eye >= 0 && stdVR_map3DUniformEyeIndex >= 0) {
        glUniform1i(stdVR_map3DUniformEyeIndex, eye);
    }
#endif

    // Bind our VAO which has all vertex attribute state pre-configured
    glBindVertexArray(stdVR_map3DVertexArray);

    // Upload vertex data to our VBO
    glBindBuffer(GL_ARRAY_BUFFER, stdVR_map3DVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, numVertices * sizeof(stdVR_Map3DVertex),
                 stdVR_map3DVertices, GL_STREAM_DRAW);

    // Enable blending, disable depth test for HUD-like overlay
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);  // Lines don't need face culling

    // Enable line smoothing for anti-aliased lines (desktop GL only)
#ifndef TARGET_ANDROID_NATIVE_GLES
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
#endif
    // Note: glLineWidth values other than 1.0 may not be supported on some drivers

    // Clear any pending errors before draw
    while (glGetError() != GL_NO_ERROR) {}

    // Draw lines
    glDrawArrays(GL_LINES, 0, numVertices);

    // Restore GL state (in reverse order)
#ifndef TARGET_ANDROID_NATIVE_GLES
    glDisable(GL_LINE_SMOOTH);
#endif
    glLineWidth(1.0f);
    if (prevCullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glDepthMask(prevDepthMask);
    if (prevDepthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (prevBlendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (prevScissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glBlendFunc(prevBlendSrcRGB, prevBlendDstRGB);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glScissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);

    // Restore buffer, VAO, program, and FBO
    glBindVertexArray(prevVAO);  // Restore VAO first (it affects buffer bindings)
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevEBO);
    glUseProgram(prevProgram);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevFBO);
}

// Note: DrawPlayerMarker removed - rdDebug_DrawLine3 breaks VR MultiView rendering
// TODO: Add player marker using the custom shader when needed

#endif // PLATFORM_VR
