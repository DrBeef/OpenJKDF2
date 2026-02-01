#ifndef STDVR_MAP3D_H
#define STDVR_MAP3D_H

#include "types.h"
#include "Primitives/rdVector.h"

#ifdef PLATFORM_VR

// Maximum number of line segments to collect
#define STDVR_MAP3D_MAX_LINES 16384

// Maximum sector traversal depth
#define STDVR_MAP3D_MAX_DEPTH 20

// Map line segment with color
typedef struct stdVR_Map3DLine {
    rdVector3 start;
    rdVector3 end;
    uint32_t color;     // RGBA packed
} stdVR_Map3DLine;

// Map display state
typedef struct stdVR_Map3DState {
    int bEnabled;              // Is map currently visible
    int bInitialized;          // Has system been initialized
    rdVector3 position;        // Position offset in VR space (relative to player)
    float scale;               // World-to-VR scale factor
    float rotation;            // Y-axis rotation (degrees)
    rdVector3 playerPos;       // Player position for centering
    float playerYaw;           // Player facing direction
} stdVR_Map3DState;

// Initialize/shutdown
void stdVR_Map3D_Startup(void);
void stdVR_Map3D_Shutdown(void);

// Toggle map visibility
void stdVR_Map3D_Toggle(void);
int stdVR_Map3D_IsVisible(void);

// Update and render (call from VR render loop)
void stdVR_Map3D_Update(void);
void stdVR_Map3D_Render(int eye);  // eye = -1 for MultiView, 0/1 for per-eye PC VR

// Configuration
void stdVR_Map3D_SetScale(float scale);
void stdVR_Map3D_SetDistance(float distance);
void stdVR_Map3D_SetHeight(float height);

// Rotation control (for thumbstick input)
void stdVR_Map3D_Rotate(float deltaAngle);

// Zoom control (for thumbstick input)
void stdVR_Map3D_Zoom(float deltaZoom);

// Two-handed gesture control (scale, rotate, translate with both grips held)
void stdVR_Map3D_ProcessGestures(int bothGripsHeld, rdVector3* pLeftPos, rdVector3* pRightPos);

// Game pause control - returns 1 if game logic should pause
int stdVR_Map3D_ShouldPauseGame(void);

#else // !PLATFORM_VR

// Stub implementations when VR is not compiled in
static inline void stdVR_Map3D_Startup(void) {}
static inline void stdVR_Map3D_Shutdown(void) {}
static inline void stdVR_Map3D_Toggle(void) {}
static inline int stdVR_Map3D_IsVisible(void) { return 0; }
static inline void stdVR_Map3D_Update(void) {}
static inline void stdVR_Map3D_Render(int eye) { (void)eye; }
static inline void stdVR_Map3D_SetScale(float scale) { (void)scale; }
static inline void stdVR_Map3D_SetDistance(float distance) { (void)distance; }
static inline void stdVR_Map3D_SetHeight(float height) { (void)height; }
static inline void stdVR_Map3D_Rotate(float deltaAngle) { (void)deltaAngle; }
static inline void stdVR_Map3D_Zoom(float deltaZoom) { (void)deltaZoom; }
static inline void stdVR_Map3D_ProcessGestures(int bothGripsHeld, rdVector3* pLeftPos, rdVector3* pRightPos) { (void)bothGripsHeld; (void)pLeftPos; (void)pRightPos; }
static inline int stdVR_Map3D_ShouldPauseGame(void) { return 0; }

#endif // PLATFORM_VR

#endif // STDVR_MAP3D_H
