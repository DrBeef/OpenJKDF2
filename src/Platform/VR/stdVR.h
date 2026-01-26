#ifndef _STDVR_H
#define _STDVR_H

// Added: VR/OpenXR support public interface

#include "types.h"
#include "Platform/VR/stdVR_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PLATFORM_VR

// Global VR state
extern int stdVR_bEnabled;              // Is VR enabled
extern int stdVR_bInitted;              // Is VR module initialized
extern stdVR_ClientInfo stdVR_clientInfo;
extern stdVR_Config stdVR_config;
extern stdVR_MotionConfig stdVR_motionConfig;

// Lifecycle functions
int stdVR_Startup(void);
void stdVR_Shutdown(void);

// Session management
int stdVR_CreateSession(void* pGLContext);
void stdVR_DestroySession(void);
int stdVR_IsSessionRunning(void);
void stdVR_PollEvents(void);            // Poll OpenXR events (call every frame)

// Frame timing (match OpenXR frame cadence)
// Call order: WaitFrame -> UpdateTracking -> BeginFrame -> [render] -> EndFrame
int stdVR_WaitFrame(void);              // xrWaitFrame - blocks until optimal render time
int stdVR_BeginFrame(void);             // xrBeginFrame - start frame for rendering
int stdVR_EndFrame(void);               // xrEndFrame - submit rendered frame to compositor
void stdVR_SubmitEmptyFrame(void);      // Submit frame with no layers (for menus/loading)
int stdVR_IsFramePending(void);         // Check if WaitFrame called but EndFrame not yet
void stdVR_KeepAlive(void);             // Call during loading to prevent headset blackout

// Per-eye rendering
int stdVR_PrepareEyeBuffer(int eye);    // Acquire and bind eye swapchain image
int stdVR_FinishEyeBuffer(int eye);     // Release eye swapchain image
int stdVR_GetCurrentEyeFBO(int eye);    // Get the FBO for the current eye (0 if not active)
int stdVR_GetCurrentEye(void);          // Get which eye is currently being rendered (-1 if none)

// HUD rendering (dedicated quad layer for in-game HUD)
int stdVR_PrepareHudBuffer(void);       // Acquire HUD swapchain and bind FBO
int stdVR_FinishHudBuffer(void);        // Release HUD swapchain image
int stdVR_GetHudFBO(void);              // Get HUD FBO (0 if not active)
void stdVR_GetHudSize(int* pWidth, int* pHeight);  // Get HUD render target size
int stdVR_IsHudEnabled(void);           // Check if HUD quad layer is available

// Tracking
void stdVR_UpdateTracking(void);        // Update HMD and controller poses
void stdVR_GetHMDPose(rdVector3* pPosition, rdVector3* pOrientation);
void stdVR_GetControllerPose(int hand, rdVector3* pPosition, rdVector3* pOrientation);

// View matrices
void stdVR_GetEyeViewMatrix(int eye, rdMatrix34* pOut);
void stdVR_GetEyeProjection(int eye, float* pOut16);
void stdVR_GetEyeProjectionMatrix44(int eye, float* pOut16, float zNear, float zFar);

// Input
void stdVR_UpdateInput(void);           // Poll controller input state
void stdVR_MapInputToGame(void);        // Map VR input to game actions

// Haptics
void stdVR_TriggerHaptic(int hand, float amplitude, float duration, float frequency);
void stdVR_StopHaptic(int hand);

// Utility
void stdVR_RecenterView(void);
void stdVR_GetRecommendedRenderSize(int* pWidth, int* pHeight);
const char* stdVR_GetRuntimeName(void);

// Screen layer mode (for menus/cinematics - renders as 2D quad in 3D space)
int stdVR_UseScreenLayer(void);         // Check if screen layer should be used
void stdVR_SetScreenLayerMode(int bEnable);  // Enable/disable screen layer mode
void stdVR_UpdateScreenLayerSnap(void); // Update snap position when entering screen mode
float stdVR_GetScreenLayerDistance(void);    // Get screen distance from player

// VR Menu cursor (for interacting with menus via controller pointing)
void stdVR_UpdateMenuCursor(void);      // Update cursor position from controller angles
void stdVR_GetMenuCursorPos(int* pX, int* pY);  // Get cursor screen position
int stdVR_IsMenuCursorActive(void);     // Is cursor active (screen layer mode)
int stdVR_GetMenuTriggerPressed(void);  // Was trigger pressed this frame
int stdVR_GetMenuTriggerReleased(void); // Was trigger released this frame

// Helper function to combine game camera with VR eye offset
void stdVR_CombineCameraWithEye(const rdMatrix34* pGameCamera, int eye, rdMatrix34* pOut);
// Helper function to combine game camera with HMD center pose (no IPD)
void stdVR_CombineCameraWithHMD(const rdMatrix34* pGameCamera, rdMatrix34* pOut);

// Added: Get/set current eye view matrix (for weapon rendering in VR)
void stdVR_SetCurrentEyeViewMatrix(const rdMatrix34* pMat);
int stdVR_GetCurrentEyeViewMatrix(rdMatrix34* pOut);
void stdVR_ClearCurrentEyeViewMatrix(void);

// Motion controls helper functions
int stdVR_GetDominantHand(void);
stdVR_ControllerState* stdVR_GetController(int hand);
stdVR_ControllerState* stdVR_GetDominantController(void);
stdVR_ControllerState* stdVR_GetOffhandController(void);
void stdVR_ControllerToWorld(int hand, rdVector3* pWorldPos);
void stdVR_GetControllerAimDirection(int hand, rdVector3* pDirection);
void stdVR_GetControllerWorldMatrix(int hand, rdMatrix34* pMatrix);
int stdVR_IsSwingTriggered(void);
float stdVR_GetSwingSpeed(void);
int stdVR_GetControllerViewMatrix(int hand, rdMatrix34* pViewMat);

// Debug visualization
void stdVR_DrawDebugControllerAxes(int hand);

// Settings sync functions (to/from jkPlayer settings)
void stdVR_SyncConfigFromJkPlayer(void);
void stdVR_SyncConfigToJkPlayer(void);

#else // !PLATFORM_VR

// Stub implementations when VR is not compiled in
#define stdVR_bEnabled 0
#define stdVR_bInitted 0

static inline int stdVR_Startup(void) { return 0; }
static inline void stdVR_Shutdown(void) {}
static inline int stdVR_CreateSession(void* ctx) { (void)ctx; return 0; }
static inline void stdVR_DestroySession(void) {}
static inline int stdVR_IsSessionRunning(void) { return 0; }
static inline void stdVR_PollEvents(void) {}
static inline int stdVR_WaitFrame(void) { return 0; }
static inline int stdVR_BeginFrame(void) { return 0; }
static inline int stdVR_EndFrame(void) { return 0; }
static inline int stdVR_PrepareEyeBuffer(int eye) { (void)eye; return 0; }
static inline int stdVR_FinishEyeBuffer(int eye) { (void)eye; return 0; }
static inline int stdVR_GetCurrentEyeFBO(int eye) { (void)eye; return 0; }
static inline int stdVR_GetCurrentEye(void) { return -1; }
static inline int stdVR_PrepareHudBuffer(void) { return 0; }
static inline int stdVR_FinishHudBuffer(void) { return 0; }
static inline int stdVR_GetHudFBO(void) { return 0; }
static inline void stdVR_GetHudSize(int* pWidth, int* pHeight) { if (pWidth) *pWidth = 0; if (pHeight) *pHeight = 0; }
static inline int stdVR_IsHudEnabled(void) { return 0; }
static inline void stdVR_UpdateTracking(void) {}
static inline void stdVR_UpdateInput(void) {}
static inline void stdVR_MapInputToGame(void) {}
static inline void stdVR_KeepAlive(void) {}
static inline void stdVR_SetCurrentEyeViewMatrix(const void* pMat) { (void)pMat; }
static inline int stdVR_GetCurrentEyeViewMatrix(void* pOut) { (void)pOut; return 0; }
static inline void stdVR_ClearCurrentEyeViewMatrix(void) {}

#endif // PLATFORM_VR

#ifdef __cplusplus
}
#endif

#endif // _STDVR_H
