#include "stdVR.h"

// Added: VR support implementation

#ifdef PLATFORM_VR

#include "Platform/VR/stdVR_OpenXR.h"
#include "Platform/VR/stdVR_Input.h"
#include "Primitives/rdVector.h"
#include "Primitives/rdMatrix.h"
#include "stdPlatform.h"

#include <string.h>
#include <math.h>

// Global VR state
int stdVR_bEnabled = 0;
int stdVR_bInitted = 0;
stdVR_ClientInfo stdVR_clientInfo;
stdVR_Config stdVR_config;

// Frame state tracking
static int stdVR_bFramePending = 0;  // WaitFrame called but EndFrame not yet
static int stdVR_bFrameInProgress = 0;  // BeginFrame called but EndFrame not yet

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

    return stdVR_OpenXR_CreateSession(pGLContext);
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
        if (vrWaitFrameCallCount <= 20 || vrWaitFrameCallCount % 100 == 0) {
            VR_Log("stdVR: WaitFrame #%d - previous frame pending, submitting empty\n", vrWaitFrameCallCount);
        }
        stdVR_SubmitEmptyFrame();
        if (stdVR_bFramePending || stdVR_bFrameInProgress) {
            if (vrWaitFrameCallCount <= 20 || vrWaitFrameCallCount % 100 == 0) {
                VR_Log("stdVR: WaitFrame #%d aborted - frame still pending/in-progress\n", vrWaitFrameCallCount);
            }
            return 0;
        }
    }

    int result = stdVR_OpenXR_WaitFrame();
    if (result) {
        stdVR_bFramePending = 1;  // Mark that we need to complete this frame
    }

    if (vrWaitFrameCallCount <= 20 || vrWaitFrameCallCount % 100 == 0) {
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
        if (emptyFrameSkipCount <= 10 || emptyFrameSkipCount % 100 == 0) {
            VR_Log("stdVR: SubmitEmptyFrame skipped #%d (enabled=%d, running=%d, pending=%d)\n",
                emptyFrameSkipCount, stdVR_bEnabled, stdVR_clientInfo.bSessionRunning, stdVR_bFramePending);
        }
        return;
    }

    emptyFrameCount++;
    if (emptyFrameCount <= 20 || emptyFrameCount % 100 == 0) {
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
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return 0;
    }

    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        return 0;
    }

    return stdVR_OpenXR_PrepareEyeBuffer(eye);
}

int stdVR_FinishEyeBuffer(int eye)
{
    if (!stdVR_bEnabled || !stdVR_clientInfo.bSessionRunning) {
        return 0;
    }

    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        return 0;
    }

    return stdVR_OpenXR_FinishEyeBuffer(eye);
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

    // Get the per-eye view matrix (complete eye pose in VR space)
    rdMatrix34 eyePose;
    rdMatrix_Copy34(&eyePose, &stdVR_clientInfo.eyes[eye].viewMatrix);

    // Log eye pose before scaling (first few frames only)
    if (combineCallCount <= 20) {
        extern void VR_Log(const char* fmt, ...);
        VR_Log("stdVR_CombineCameraWithEye: eye %d - eyePose.scale=(%f, %f, %f), worldScale=%f\n",
            eye, eyePose.scale.x, eyePose.scale.y, eyePose.scale.z, worldScale);
    }

    // Scale VR position by world scale
    eyePose.scale.x *= worldScale;
    eyePose.scale.y *= worldScale;
    eyePose.scale.z *= worldScale;

    // Apply height offset
    eyePose.scale.y += stdVR_config.heightOffset;

    // Start with game camera and apply VR eye pose
    // The game camera gives us the base position/orientation in the game world
    // The VR eye pose overlays VR tracking on top of that
    rdMatrix34 combined;
    rdMatrix_Copy34(&combined, pGameCamera);
    rdMatrix_PreMultiply34(&combined, &eyePose);

    rdMatrix_Copy34(pOut, &combined);
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

    // Use screen layer when NOT in 3D gameplay mode
    int shouldUseScreenLayer = (jkGame_isDDraw == 0);

    // Update the client info
    stdVR_clientInfo.bUseScreenLayer = shouldUseScreenLayer;

    // Detect transition INTO screen layer mode - snap position/orientation
    if (shouldUseScreenLayer && !stdVR_prevScreenLayerState) {
        stdVR_UpdateScreenLayerSnap();
        VR_Log("stdVR: Entering screen layer mode (menu)\n");
    }
    else if (!shouldUseScreenLayer && stdVR_prevScreenLayerState) {
        VR_Log("stdVR: Exiting screen layer mode (entering 3D gameplay)\n");
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

#endif // PLATFORM_VR
