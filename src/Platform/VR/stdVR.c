#include "stdVR.h"

// Added: VR support implementation

#ifdef PLATFORM_VR

#include "Platform/VR/stdVR_OpenXR.h"
#include "Platform/VR/stdVR_Input.h"
#include "Primitives/rdVector.h"
#include "Primitives/rdMatrix.h"
#include "Main/jkMain.h"
#include "stdPlatform.h"

#include <string.h>
#include <math.h>

// Global VR state
int stdVR_bEnabled = 0;
int stdVR_bInitted = 0;
stdVR_ClientInfo stdVR_clientInfo;
stdVR_Config stdVR_config;

// Debug mode for shader testing (0=normal, 1=solid, 2=UV, 3=depth, 4=vertex color)
int std3D_vrDebugMode = 0;

// Frame state tracking
static int stdVR_bFramePending = 0;  // WaitFrame called but EndFrame not yet
static int stdVR_bFrameInProgress = 0;  // BeginFrame called but EndFrame not yet

// Added: Current combined camera+eye view matrix (for weapon rendering)
static rdMatrix34 stdVR_currentEyeViewMat;
static int stdVR_currentEyeViewMatValid = 0;

// Default configuration
static void stdVR_InitDefaultConfig(void)
{
    memset(&stdVR_config, 0, sizeof(stdVR_config));

    stdVR_config.moveDirection = STDVR_MOVE_CONTROLLER;
    stdVR_config.turnMode = STDVR_TURN_SNAP;
    stdVR_config.snapTurnAngle = 45;
    stdVR_config.smoothTurnSpeed = 120.0f;
    stdVR_config.worldScale = 1.0f;
    stdVR_config.heightOffset = 0.0f;
    stdVR_config.bComfortVignette = 1;
    stdVR_config.dominantHand = STDVR_CONTROLLER_RIGHT;
    stdVR_config.supersampling = 1.0f;
}

int stdVR_Startup(void)
{
    if (stdVR_bInitted) {
        return 1;
    }

    stdPlatform_Printf("stdVR: Starting up VR subsystem...\n");

    memset(&stdVR_clientInfo, 0, sizeof(stdVR_clientInfo));
    stdVR_InitDefaultConfig();

    // Initialize OpenXR
    if (!stdVR_OpenXR_Init()) {
        stdPlatform_Printf("stdVR: Failed to initialize OpenXR\n");
        return 0;
    }

    stdVR_bInitted = 1;

    // Sync settings from jkPlayer (except enabled state - we handle that separately)
    stdVR_SyncConfigFromJkPlayer();

    // VR should be enabled by default when OpenXR initializes successfully
    // Only disable if the user explicitly set jkPlayer_vrEnabled = 0 in config
    // For now, always enable since we successfully initialized
    stdVR_bEnabled = 1;

    stdPlatform_Printf("stdVR: VR subsystem initialized successfully\n");
    stdPlatform_Printf("stdVR: Runtime: %s\n", stdVR_GetRuntimeName());
    stdPlatform_Printf("stdVR: VR enabled: %d\n", stdVR_bEnabled);

    return 1;
}

void stdVR_Shutdown(void)
{
    if (!stdVR_bInitted) {
        return;
    }

    stdPlatform_Printf("stdVR: Shutting down VR subsystem...\n");

    stdVR_DestroySession();
    stdVR_OpenXR_Shutdown();

    stdVR_bEnabled = 0;
    stdVR_bInitted = 0;

    stdPlatform_Printf("stdVR: VR subsystem shutdown complete\n");
}

int stdVR_CreateSession(void* pGLContext)
{
    if (!stdVR_bInitted) {
        return 0;
    }

    int result = stdVR_OpenXR_CreateSession(pGLContext);

    // Auto-recenter view when session starts so player isn't offset from character
    if (result) {
        stdPlatform_Printf("stdVR: Session created, recentering view...\n");
        stdVR_RecenterView();
    }

    return result;
}

void stdVR_DestroySession(void)
{
    if (!stdVR_bInitted) {
        return;
    }

    stdVR_OpenXR_DestroySession();
    stdVR_bFramePending = 0;
    stdVR_bFrameInProgress = 0;
}

int stdVR_IsSessionRunning(void)
{
    return stdVR_clientInfo.bSessionRunning;
}

void stdVR_PollEvents(void)
{
    if (!stdVR_bInitted) {
        return;
    }

    stdVR_OpenXR_PollEvents();
}

// External VR_Log from stdVR_OpenXR.cpp
extern void VR_Log(const char* fmt, ...);

static int vrWaitFrameCallCount = 0;

int stdVR_WaitFrame(void)
{
    if (!stdVR_bEnabled) {
        return 0;
    }

    vrWaitFrameCallCount++;

    // Always poll events to allow session state transitions
    stdVR_OpenXR_PollEvents();

    if (!stdVR_clientInfo.bSessionRunning) {
        return 0;
    }

    // If we already have a pending frame that wasn't completed, submit an empty frame
    // This can happen during loading screens or state transitions
    if (stdVR_bFramePending) {
        if (vrWaitFrameCallCount <= 5) {
            VR_Log("stdVR: WaitFrame #%d - previous frame pending, submitting empty\n", vrWaitFrameCallCount);
        }
        stdVR_SubmitEmptyFrame();
        if (stdVR_bFramePending || stdVR_bFrameInProgress) {
            if (vrWaitFrameCallCount <= 5) {
                VR_Log("stdVR: WaitFrame #%d aborted - frame still pending/in-progress\n", vrWaitFrameCallCount);
            }
            return 0;
        }
    }

    int result = stdVR_OpenXR_WaitFrame();
    if (result) {
        stdVR_bFramePending = 1;  // Mark that we need to complete this frame
    }

    if (vrWaitFrameCallCount <= 5) {
        VR_Log("stdVR: WaitFrame #%d done, pending=%d\n", vrWaitFrameCallCount, stdVR_bFramePending);
    }

    return result;
}

int stdVR_BeginFrame(void)
{
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return 0;
    }

    if (stdVR_OpenXR_BeginFrame()) {
        stdVR_bFrameInProgress = 1;
        return 1;
    }
    return 0;
}

int stdVR_EndFrame(void)
{
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return 0;
    }

    int result = stdVR_OpenXR_EndFrame();
    if (result) {
        stdVR_bFramePending = 0;  // Frame completed
        stdVR_bFrameInProgress = 0;
    }
    return result;
}

void stdVR_SubmitEmptyFrame(void)
{
    static int emptyFrameCount = 0;
    static int emptyFrameSkipCount = 0;

    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning || !stdVR_bFramePending) {
        emptyFrameSkipCount++;
        // Only log first skip
        if (emptyFrameSkipCount == 1) {
            VR_Log("stdVR: SubmitEmptyFrame skipped (enabled=%d, running=%d, pending=%d)\n",
                stdVR_bEnabled, stdVR_clientInfo.bSessionRunning, stdVR_bFramePending);
        }
        return;
    }

    emptyFrameCount++;
    // Only log first few empty frames
    if (emptyFrameCount <= 3) {
        VR_Log("stdVR: Submitting empty frame #%d\n", emptyFrameCount);
    }

    // Submit a frame with no rendered content (just begin/end)
    if (stdVR_bFrameInProgress) {
        // A frame is already in progress; end it with no layers.
        if (stdVR_OpenXR_EndFrameEmpty()) {
            stdVR_bFramePending = 0;
            stdVR_bFrameInProgress = 0;
        }
        return;
    }

    if (stdVR_BeginFrame()) {
        // Don't render anything, just end the frame with no layers
        if (stdVR_OpenXR_EndFrameEmpty()) {
            stdVR_bFramePending = 0;
            stdVR_bFrameInProgress = 0;
        }
    }
}

int stdVR_IsFramePending(void)
{
    return stdVR_bFramePending;
}

// Called during long operations (loading, etc.) to keep VR headset from going black
void stdVR_KeepAlive(void)
{
    static int keepAliveCount = 0;

    if (!stdVR_bEnabled || !stdVR_bInitted) {
        return;
    }

    keepAliveCount++;
    if (keepAliveCount <= 20 || keepAliveCount % 50 == 0) {
        stdPlatform_Printf("stdVR: KeepAlive #%d called (pending=%d, running=%d)\n",
            keepAliveCount, stdVR_bFramePending, stdVR_clientInfo.bSessionRunning);
    }

    // Poll events to keep session state machine running
    stdVR_OpenXR_PollEvents();

    if (!stdVR_clientInfo.bSessionRunning) {
        if (keepAliveCount <= 10) {
            stdPlatform_Printf("stdVR: KeepAlive - session not running\n");
        }
        return;
    }

    // If we have a pending frame, finish it
    if (stdVR_bFramePending) {
        if (keepAliveCount <= 20 || keepAliveCount % 50 == 0) {
            stdPlatform_Printf("stdVR: KeepAlive #%d - submitting pending frame\n", keepAliveCount);
        }
        stdVR_SubmitEmptyFrame();
    }

    // Start and immediately finish a new frame to keep the headset alive
    if (stdVR_OpenXR_WaitFrame()) {
        stdVR_bFramePending = 1;  // Mark frame as pending so SubmitEmptyFrame works
        if (keepAliveCount <= 20 || keepAliveCount % 50 == 0) {
            stdPlatform_Printf("stdVR: KeepAlive #%d - submitting new empty frame\n", keepAliveCount);
        }
        stdVR_SubmitEmptyFrame();
    } else {
        if (keepAliveCount <= 20) {
            stdPlatform_Printf("stdVR: KeepAlive #%d - WaitFrame failed\n", keepAliveCount);
        }
    }
}

int stdVR_PrepareEyeBuffer(int eye)
{
    static int prepareWrapperCount[2] = {0, 0};
    if (eye >= 0 && eye < 2) prepareWrapperCount[eye]++;

    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        // Only log first block per eye
        if (prepareWrapperCount[eye >= 0 && eye < 2 ? eye : 0] == 1) {
            VR_Log("stdVR_PrepareEyeBuffer(%d) BLOCKED: enabled=%d, running=%d\n",
                eye, stdVR_bEnabled, stdVR_clientInfo.bSessionRunning);
        }
        return 0;
    }

    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        VR_Log("stdVR_PrepareEyeBuffer(%d) BLOCKED: eye out of range\n", eye);
        return 0;
    }

    // Only log first 2 calls per eye (one frame)
    if (prepareWrapperCount[eye] <= 2) {
        VR_Log("stdVR_PrepareEyeBuffer(%d) #%d\n", eye, prepareWrapperCount[eye]);
    }
    return stdVR_OpenXR_PrepareEyeBuffer(eye);
}

int stdVR_FinishEyeBuffer(int eye)
{
    static int finishWrapperCount[2] = {0, 0};
    if (eye >= 0 && eye < 2) finishWrapperCount[eye]++;

    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        // Only log first block per eye
        if (finishWrapperCount[eye >= 0 && eye < 2 ? eye : 0] == 1) {
            VR_Log("stdVR_FinishEyeBuffer(%d) BLOCKED: enabled=%d, running=%d\n",
                eye, stdVR_bEnabled, stdVR_clientInfo.bSessionRunning);
        }
        return 0;
    }

    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        VR_Log("stdVR_FinishEyeBuffer(%d) BLOCKED: eye out of range\n", eye);
        return 0;
    }

    // Only log first 2 calls per eye (one frame)
    if (finishWrapperCount[eye] <= 2) {
        VR_Log("stdVR_FinishEyeBuffer(%d) #%d\n", eye, finishWrapperCount[eye]);
    }
    return stdVR_OpenXR_FinishEyeBuffer(eye);
}

int stdVR_GetCurrentEyeFBO(int eye)
{
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return 0;
    }

    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        return 0;
    }

    return stdVR_OpenXR_GetCurrentEyeFBO(eye);
}

int stdVR_GetCurrentEye(void)
{
    // Don't check bEnabled - the OpenXR layer manages the actual state
    // The extern variable may be set even when the wrapper thinks VR is disabled
    return stdVR_OpenXR_GetCurrentEye();
}

void stdVR_UpdateTracking(void)
{
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return;
    }

    stdVR_OpenXR_UpdateTracking();

    // Update move direction based on config
    if (stdVR_config.moveDirection == STDVR_MOVE_HEAD) {
        rdVector_Copy3(&stdVR_clientInfo.moveForward, &stdVR_clientInfo.hmdPoseMatrix.lvec);
        stdVR_clientInfo.moveYaw = stdVR_clientInfo.hmdOrientation.y;
    } else {
        // Use dominant hand controller orientation
        int hand = stdVR_config.dominantHand;
        if (stdVR_clientInfo.controllers[hand].bTracking) {
            rdVector_Copy3(&stdVR_clientInfo.moveForward, &stdVR_clientInfo.controllers[hand].poseMatrix.lvec);
            stdVR_clientInfo.moveYaw = stdVR_clientInfo.controllers[hand].orientation.y;
        } else {
            // Fallback to head
            rdVector_Copy3(&stdVR_clientInfo.moveForward, &stdVR_clientInfo.hmdPoseMatrix.lvec);
            stdVR_clientInfo.moveYaw = stdVR_clientInfo.hmdOrientation.y;
        }
    }
}

void stdVR_GetHMDPose(rdVector3* pPosition, rdVector3* pOrientation)
{
    if (pPosition) {
        rdVector_Copy3(pPosition, &stdVR_clientInfo.hmdPosition);
    }
    if (pOrientation) {
        rdVector_Copy3(pOrientation, &stdVR_clientInfo.hmdOrientation);
    }
}

void stdVR_GetControllerPose(int hand, rdVector3* pPosition, rdVector3* pOrientation)
{
    if (hand < 0 || hand >= STDVR_CONTROLLER_COUNT) {
        return;
    }

    if (pPosition) {
        rdVector_Copy3(pPosition, &stdVR_clientInfo.controllers[hand].position);
    }
    if (pOrientation) {
        rdVector_Copy3(pOrientation, &stdVR_clientInfo.controllers[hand].orientation);
    }
}

void stdVR_GetEyeViewMatrix(int eye, rdMatrix34* pOut)
{
    if (!pOut || eye < 0 || eye >= STDVR_EYE_COUNT) {
        return;
    }

    rdMatrix_Copy34(pOut, &stdVR_clientInfo.eyes[eye].viewMatrix);
}

void stdVR_GetEyeProjection(int eye, float* pOut16)
{
    if (!pOut16 || eye < 0 || eye >= STDVR_EYE_COUNT) {
        return;
    }

    memcpy(pOut16, stdVR_clientInfo.eyes[eye].projectionMatrix, 16 * sizeof(float));
}

void stdVR_GetEyeProjectionMatrix44(int eye, float* pOut16, float zNear, float zFar)
{
    if (!pOut16 || eye < 0 || eye >= STDVR_EYE_COUNT) {
        return;
    }

    stdVR_EyeView* pEye = &stdVR_clientInfo.eyes[eye];

    // Build asymmetric projection matrix from FOV tangents
    float left = -pEye->fovLeft * zNear;
    float right = pEye->fovRight * zNear;
    float bottom = -pEye->fovDown * zNear;
    float top = pEye->fovUp * zNear;

    float width = right - left;
    float height = top - bottom;

    // Column-major 4x4 perspective matrix
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

void stdVR_UpdateInput(void)
{
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return;
    }

    stdVR_OpenXR_UpdateInput();
}

void stdVR_MapInputToGame(void)
{
    if (!stdVR_bEnabled) {
        return;
    }

    stdVR_Input_MapToGame();
}

void stdVR_TriggerHaptic(int hand, float amplitude, float duration, float frequency)
{
    if (!stdVR_bEnabled || hand < 0 || hand >= STDVR_CONTROLLER_COUNT) {
        return;
    }

    stdVR_OpenXR_TriggerHaptic(hand, amplitude, duration, frequency);
}

void stdVR_StopHaptic(int hand)
{
    if (!stdVR_bEnabled || hand < 0 || hand >= STDVR_CONTROLLER_COUNT) {
        return;
    }

    stdVR_OpenXR_StopHaptic(hand);
}

void stdVR_RecenterView(void)
{
    if (!stdVR_bEnabled) {
        return;
    }

    stdVR_OpenXR_RecenterView();
}

void stdVR_GetRecommendedRenderSize(int* pWidth, int* pHeight)
{
    if (pWidth) {
        *pWidth = stdVR_clientInfo.renderWidth;
    }
    if (pHeight) {
        *pHeight = stdVR_clientInfo.renderHeight;
    }
}

const char* stdVR_GetRuntimeName(void)
{
    return stdVR_OpenXR_GetRuntimeName();
}

void stdVR_CombineCameraWithEye(const rdMatrix34* pGameCamera, int eye, rdMatrix34* pOut)
{
    static int combineCallCount = 0;
    combineCallCount++;

    if (!pGameCamera || !pOut || eye < 0 || eye >= STDVR_EYE_COUNT) {
        return;
    }

    float worldScale = stdVR_config.worldScale;
    if (worldScale <= 0.0f) {
        worldScale = 1.0f;
    }

    // Reference position: neutral standing position in VR tracking space (converted to JKDF2 coords)
    // OpenXR: Y-up, -Z forward. JKDF2: Z-up, Y-forward
    // Conversion: JKDF2.x = XR.x, JKDF2.y = -XR.z, JKDF2.z = XR.y
    // OpenXR standing height (0, 1.7, 0) becomes JKDF2 (0, 0, 1.7)
    static const rdVector3 referencePos = { 0.0f, 0.0f, 1.7f };

    // Get the HMD center pose (already converted to JKDF2 coordinates)
    rdMatrix34 hmdPose;
    rdMatrix_Copy34(&hmdPose, &stdVR_clientInfo.hmdPoseMatrix);

    // Get the per-eye pose for IPD calculation
    rdMatrix34 eyePose;
    rdMatrix_Copy34(&eyePose, &stdVR_clientInfo.eyes[eye].viewMatrix);

    // Calculate HMD position offset from reference (6DOF tracking offset)
    rdVector3 hmdOffset;
    hmdOffset.x = (hmdPose.scale.x - referencePos.x) * worldScale;
    hmdOffset.y = (hmdPose.scale.y - referencePos.y) * worldScale;
    hmdOffset.z = (hmdPose.scale.z - referencePos.z) * worldScale;

    // Calculate per-eye IPD offset (difference from HMD center)
    rdVector3 ipdOffset;
    ipdOffset.x = (eyePose.scale.x - hmdPose.scale.x) * worldScale;
    ipdOffset.y = (eyePose.scale.y - hmdPose.scale.y) * worldScale;
    ipdOffset.z = (eyePose.scale.z - hmdPose.scale.z) * worldScale;

    // Log first few calls for debugging
    if (combineCallCount <= 4) {
        extern void VR_Log(const char* fmt, ...);
        VR_Log("stdVR_CombineCameraWithEye: eye %d\n", eye);
        VR_Log("  HMD pos=(%.4f,%.4f,%.4f)\n", hmdPose.scale.x, hmdPose.scale.y, hmdPose.scale.z);
        VR_Log("  Eye pos=(%.4f,%.4f,%.4f)\n", eyePose.scale.x, eyePose.scale.y, eyePose.scale.z);
        VR_Log("  IPD offset raw=(%.5f,%.5f,%.5f)\n", ipdOffset.x, ipdOffset.y, ipdOffset.z);
    }

    // Build the combined view matrix:
    // 1. Start with game camera (player's view in game world)
    // 2. Apply 6DOF head position offset (physical head movement in VR space)
    // 3. Apply per-eye IPD offset
    //
    // Note: For now, we keep the game camera's orientation and only apply position offsets.
    // This gives 6DOF position tracking while the player's look direction is controlled
    // by the game's input system. Full orientation integration would require combining
    // the HMD rotation with the game camera rotation.

    rdMatrix34 combined;
    rdMatrix_Copy34(&combined, pGameCamera);  // Start with game camera orientation + position

    // Transform the offsets from VR tracking space to game world space
    // The HMD offset is in VR tracking coordinates, we need to rotate it by the body orientation
    rdVector3 hmdOffsetWorld, ipdOffsetWorld;
    rdMatrix_TransformVector34(&hmdOffsetWorld, &hmdOffset, pGameCamera);
    rdMatrix_TransformVector34(&ipdOffsetWorld, &ipdOffset, pGameCamera);

    // Apply position offsets
    combined.scale.x += hmdOffsetWorld.x + ipdOffsetWorld.x;
    combined.scale.y += hmdOffsetWorld.y + ipdOffsetWorld.y;
    combined.scale.z += hmdOffsetWorld.z + ipdOffsetWorld.z;

    // Log transformed offsets and final position
    if (combineCallCount <= 4) {
        extern void VR_Log(const char* fmt, ...);
        VR_Log("  IPD offset transformed=(%.5f,%.5f,%.5f)\n", ipdOffsetWorld.x, ipdOffsetWorld.y, ipdOffsetWorld.z);
        VR_Log("  Game camera pos=(%.4f,%.4f,%.4f)\n", pGameCamera->scale.x, pGameCamera->scale.y, pGameCamera->scale.z);
        VR_Log("  Final combined pos=(%.4f,%.4f,%.4f)\n", combined.scale.x, combined.scale.y, combined.scale.z);
    }

    rdMatrix_Copy34(pOut, &combined);
}

// Added: Set the current eye view matrix (called from sithCamera_SetVRView)
void stdVR_SetCurrentEyeViewMatrix(const rdMatrix34* pMat)
{
    if (pMat) {
        rdMatrix_Copy34(&stdVR_currentEyeViewMat, pMat);
        stdVR_currentEyeViewMatValid = 1;
    } else {
        stdVR_currentEyeViewMatValid = 0;
    }
}

// Added: Get the current combined camera+eye view matrix for this eye
// Returns 1 if valid, 0 if not (should fall back to base camera)
int stdVR_GetCurrentEyeViewMatrix(rdMatrix34* pOut)
{
    if (!pOut) return 0;
    if (!stdVR_currentEyeViewMatValid) return 0;

    rdMatrix_Copy34(pOut, &stdVR_currentEyeViewMat);
    return 1;
}

// Added: Clear the current eye view matrix (called when not rendering an eye)
void stdVR_ClearCurrentEyeViewMatrix(void)
{
    stdVR_currentEyeViewMatValid = 0;
}

// Sync jkPlayer VR settings to stdVR_config
void stdVR_SyncConfigFromJkPlayer(void)
{
    // Import from jkPlayer.c
    extern int jkPlayer_vrEnabled;
    extern int jkPlayer_vrSnapTurnAngle;
    extern int jkPlayer_vrSmoothTurnSpeed;
    extern float jkPlayer_vrWorldScale;
    extern float jkPlayer_vrHeightOffset;
    extern int jkPlayer_vrComfortVignette;
    extern int jkPlayer_vrDominantHand;
    extern int jkPlayer_vrMoveDirection;
    extern float jkPlayer_vrSupersampling;

    // Don't let profile load disable VR if it's already running
    // The enabled state should only be changed through explicit user action
    // or at startup before VR is initialized
    if (!stdVR_bEnabled || !stdVR_IsSessionRunning()) {
        stdVR_bEnabled = jkPlayer_vrEnabled;
    }

    // Movement/turn settings
    stdVR_config.moveDirection = jkPlayer_vrMoveDirection; // 0=head, 1=controller
    if (jkPlayer_vrSnapTurnAngle > 0) {
        stdVR_config.turnMode = STDVR_TURN_SNAP;
        stdVR_config.snapTurnAngle = jkPlayer_vrSnapTurnAngle;
    } else {
        stdVR_config.turnMode = STDVR_TURN_SMOOTH;
    }
    stdVR_config.smoothTurnSpeed = (float)jkPlayer_vrSmoothTurnSpeed;

    // Scale and comfort
    stdVR_config.worldScale = jkPlayer_vrWorldScale;
    stdVR_config.heightOffset = jkPlayer_vrHeightOffset;
    stdVR_config.bComfortVignette = jkPlayer_vrComfortVignette;

    // Handedness
    stdVR_config.dominantHand = jkPlayer_vrDominantHand;

    // Quality
    stdVR_config.supersampling = jkPlayer_vrSupersampling;
    if (stdVR_config.supersampling <= 0.0f) {
        stdVR_config.supersampling = 1.0f;
    }
}

// Push stdVR_config settings back to jkPlayer
void stdVR_SyncConfigToJkPlayer(void)
{
    extern int jkPlayer_vrEnabled;
    extern int jkPlayer_vrSnapTurnAngle;
    extern int jkPlayer_vrSmoothTurnSpeed;
    extern float jkPlayer_vrWorldScale;
    extern float jkPlayer_vrHeightOffset;
    extern int jkPlayer_vrComfortVignette;
    extern int jkPlayer_vrDominantHand;
    extern int jkPlayer_vrMoveDirection;
    extern float jkPlayer_vrSupersampling;

    jkPlayer_vrEnabled = stdVR_bEnabled;
    jkPlayer_vrMoveDirection = stdVR_config.moveDirection;
    if (stdVR_config.turnMode == STDVR_TURN_SNAP) {
        jkPlayer_vrSnapTurnAngle = stdVR_config.snapTurnAngle;
    } else {
        jkPlayer_vrSnapTurnAngle = 0;
    }
    jkPlayer_vrSmoothTurnSpeed = (int)stdVR_config.smoothTurnSpeed;
    jkPlayer_vrWorldScale = stdVR_config.worldScale;
    jkPlayer_vrHeightOffset = stdVR_config.heightOffset;
    jkPlayer_vrComfortVignette = stdVR_config.bComfortVignette;
    jkPlayer_vrDominantHand = stdVR_config.dominantHand;
    jkPlayer_vrSupersampling = stdVR_config.supersampling;
}

// Screen layer mode functions (for menus/cinematics)
// Previous screen layer state for detecting transitions
static int stdVR_prevScreenLayerState = 0;

// Check if we should use screen layer mode (2D quad) instead of stereo projection
int stdVR_UseScreenLayer(void)
{
    // Import jkGame_isDDraw from jkGame.c
    // jkGame_isDDraw is 0 for menus/2D mode, 1 for 3D gameplay
    extern int jkGame_isDDraw;
    extern int jkGuiBuildMulti_bRendering;
    extern int jkSmack_GetCurrentGuiState(void);
#ifdef QUAKE_CONSOLE
    extern int jkQuakeConsole_bOpen;
#endif

    // Mirror JKXR-style logic: use screen layer when UI/cinematics/menus are active
    int guiState = jkSmack_GetCurrentGuiState();
    int inGameplay = (guiState == JK_GAMEMODE_GAMEPLAY);

    int shouldUseScreenLayer = (jkGame_isDDraw == 0) || !inGameplay || jkGuiBuildMulti_bRendering;
#ifdef QUAKE_CONSOLE
    if (jkQuakeConsole_bOpen) {
        shouldUseScreenLayer = 1;
    }
#endif

    // Update the client info
    stdVR_clientInfo.bUseScreenLayer = shouldUseScreenLayer;

    // Detect transition INTO screen layer mode - snap position/orientation
    // Only log transitions, not every frame
    if (shouldUseScreenLayer && !stdVR_prevScreenLayerState) {
        stdVR_UpdateScreenLayerSnap();
        VR_Log("stdVR: Entering screen layer mode\n");
    }
    else if (!shouldUseScreenLayer && stdVR_prevScreenLayerState) {
        VR_Log("stdVR: Exiting screen layer mode (3D gameplay)\n");
    }

    stdVR_prevScreenLayerState = shouldUseScreenLayer;

    return shouldUseScreenLayer;
}

// Explicitly set screen layer mode
void stdVR_SetScreenLayerMode(int bEnable)
{
    if (bEnable && !stdVR_clientInfo.bUseScreenLayer) {
        stdVR_UpdateScreenLayerSnap();
    }
    stdVR_clientInfo.bUseScreenLayer = bEnable;
    stdVR_prevScreenLayerState = bEnable;
}

// Update the snap position/orientation when entering screen layer mode
void stdVR_UpdateScreenLayerSnap(void)
{
    // Store current HMD position and yaw for screen placement
    rdVector_Copy3(&stdVR_clientInfo.screenLayerSnapPos, &stdVR_clientInfo.hmdPosition);
    stdVR_clientInfo.screenLayerSnapYaw = stdVR_clientInfo.hmdOrientation.y;

    // Initialize screen layer parameters if not already set
    if (stdVR_clientInfo.screenLayerDistance <= 0.0f) {
        stdVR_clientInfo.screenLayerDistance = 4.0f;  // 4 meters away
    }
    if (stdVR_clientInfo.screenLayerWidth <= 0.0f) {
        stdVR_clientInfo.screenLayerWidth = 6.0f;     // 6 meters wide
    }
    if (stdVR_clientInfo.screenLayerHeight <= 0.0f) {
        stdVR_clientInfo.screenLayerHeight = 4.5f;    // 4.5 meters tall (4:3 aspect)
    }

    VR_Log("stdVR: Screen layer snap - pos(%.2f, %.2f, %.2f), yaw=%.1f, dist=%.1f\n",
        stdVR_clientInfo.screenLayerSnapPos.x,
        stdVR_clientInfo.screenLayerSnapPos.y,
        stdVR_clientInfo.screenLayerSnapPos.z,
        stdVR_clientInfo.screenLayerSnapYaw,
        stdVR_clientInfo.screenLayerDistance);
}

// Get the screen layer distance from player
float stdVR_GetScreenLayerDistance(void)
{
    if (stdVR_clientInfo.screenLayerDistance <= 0.0f) {
        return 4.0f;  // Default distance
    }
    return stdVR_clientInfo.screenLayerDistance;
}

// ============================================================================
// VR Menu Cursor - Controller-based pointing for menu interaction
// ============================================================================

// Previous trigger state for edge detection
static int stdVR_prevMenuTriggerDown = 0;

// Update cursor position from controller angles
// Uses the right controller (or dominant hand) to aim at the screen
void stdVR_UpdateMenuCursor(void)
{
    // Only active in screen layer mode
    if (!stdVR_clientInfo.bUseScreenLayer) {
        stdVR_clientInfo.bMenuCursorActive = 0;
        stdVR_clientInfo.bMenuTriggerPressed = 0;
        stdVR_clientInfo.bMenuTriggerReleased = 0;
        return;
    }

    stdVR_clientInfo.bMenuCursorActive = 1;

    // Use dominant hand controller for menu pointing
    int controllerIndex = stdVR_config.dominantHand;
    stdVR_ControllerState* pController = &stdVR_clientInfo.controllers[controllerIndex];

    // Get controller orientation for pointing
    // For controller pointing at a virtual screen:
    // - Roll (Z rotation) controls horizontal cursor position (wrist tilt left/right)
    // - Pitch (X rotation) controls vertical cursor position (wrist tilt up/down)
    float controllerRoll, controllerPitch;
    if (pController->bTracking) {
        controllerRoll = pController->orientation.z;   // Roll for X
        controllerPitch = pController->orientation.x;  // Pitch for Y
    } else {
        // Fallback to HMD orientation if controller not tracked
        controllerRoll = stdVR_clientInfo.hmdOrientation.z;
        controllerPitch = stdVR_clientInfo.hmdOrientation.x;
    }

    // Convert to normalized cursor coordinates (0.0 - 1.0)
    // Based on user testing: pitch controls X, roll controls Y
    float pitchRange = 45.0f;  // Degrees of pitch that spans the screen width
    float rollRange = 30.0f;   // Degrees of roll that spans the screen height (more sensitive)

    // X: Pitch controls horizontal (negate so tilting left moves cursor left)
    float cursorX = 0.5f - (controllerPitch / pitchRange) * 0.5f;

    // Y: Roll controls vertical, with offset to account for natural controller hold angle
    // Adding offset so neutral hold position is closer to screen center
    // Negate so tilting up moves cursor up
    float rollOffset = 15.0f;  // Assume controller is naturally tilted ~15 degrees
    float cursorY = 0.5f - ((controllerRoll - rollOffset) / rollRange) * 0.5f;

    // Clamp to valid range
    if (cursorX < 0.0f) cursorX = 0.0f;
    if (cursorX > 1.0f) cursorX = 1.0f;
    if (cursorY < 0.0f) cursorY = 0.0f;
    if (cursorY > 1.0f) cursorY = 1.0f;

    stdVR_clientInfo.menuCursorX = cursorX;
    stdVR_clientInfo.menuCursorY = cursorY;

    // Convert to screen pixel coordinates (assuming 640x480 menu resolution)
    stdVR_clientInfo.menuCursorScreenX = (int)(cursorX * 640.0f);
    stdVR_clientInfo.menuCursorScreenY = (int)(cursorY * 480.0f);

    // Handle trigger input for "clicks"
    // Use the trigger from the same controller
    int triggerDown = 0;
    if (controllerIndex == STDVR_CONTROLLER_RIGHT) {
        triggerDown = (stdVR_clientInfo.triggerRight > 0.5f) ? 1 : 0;
    } else {
        triggerDown = (stdVR_clientInfo.triggerLeft > 0.5f) ? 1 : 0;
    }

    // Edge detection for press/release events
    stdVR_clientInfo.bMenuTriggerPressed = (triggerDown && !stdVR_prevMenuTriggerDown);
    stdVR_clientInfo.bMenuTriggerReleased = (!triggerDown && stdVR_prevMenuTriggerDown);
    stdVR_clientInfo.bMenuTriggerDown = triggerDown;

    stdVR_prevMenuTriggerDown = triggerDown;
    // Menu cursor logging removed - too verbose for normal operation
}

// Get cursor screen position
void stdVR_GetMenuCursorPos(int* pX, int* pY)
{
    if (pX) *pX = stdVR_clientInfo.menuCursorScreenX;
    if (pY) *pY = stdVR_clientInfo.menuCursorScreenY;
}

// Is cursor active (only in screen layer mode)
int stdVR_IsMenuCursorActive(void)
{
    return stdVR_clientInfo.bMenuCursorActive;
}

// Was trigger pressed this frame (for mouse down)
int stdVR_GetMenuTriggerPressed(void)
{
    return stdVR_clientInfo.bMenuTriggerPressed;
}

// Was trigger released this frame (for mouse up/click)
int stdVR_GetMenuTriggerReleased(void)
{
    return stdVR_clientInfo.bMenuTriggerReleased;
}

#endif // PLATFORM_VR
