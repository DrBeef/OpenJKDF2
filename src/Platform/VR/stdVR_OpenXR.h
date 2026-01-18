#ifndef _STDVR_OPENXR_H
#define _STDVR_OPENXR_H

// Added: OpenXR platform layer internal header

#ifdef PLATFORM_VR

#ifdef __cplusplus
extern "C" {
#endif

// OpenXR initialization and shutdown
int stdVR_OpenXR_Init(void);
void stdVR_OpenXR_Shutdown(void);

// Session management
int stdVR_OpenXR_CreateSession(void* pGLContext);
void stdVR_OpenXR_DestroySession(void);

// Event polling (call every frame to advance session state)
void stdVR_OpenXR_PollEvents(void);

// Frame timing
int stdVR_OpenXR_WaitFrame(void);
int stdVR_OpenXR_BeginFrame(void);
int stdVR_OpenXR_EndFrame(void);
int stdVR_OpenXR_EndFrameEmpty(void);

// Per-eye rendering
int stdVR_OpenXR_PrepareEyeBuffer(int eye);
int stdVR_OpenXR_FinishEyeBuffer(int eye);
int stdVR_OpenXR_GetCurrentEyeFBO(int eye);
int stdVR_OpenXR_GetCurrentEye(void);

// Tracking
void stdVR_OpenXR_UpdateTracking(void);

// Input
void stdVR_OpenXR_UpdateInput(void);

// Haptics
void stdVR_OpenXR_TriggerHaptic(int hand, float amplitude, float duration, float frequency);
void stdVR_OpenXR_StopHaptic(int hand);

// Utility
void stdVR_OpenXR_RecenterView(void);
const char* stdVR_OpenXR_GetRuntimeName(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_VR

#endif // _STDVR_OPENXR_H
