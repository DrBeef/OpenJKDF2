#ifndef _STDVR_TYPES_H
#define _STDVR_TYPES_H

// Added: VR support types

#include "types.h"
#include "Primitives/rdVector.h"
#include "Primitives/rdMatrix.h"

#ifdef __cplusplus
extern "C" {
#endif

// VR Eye indices
#define STDVR_EYE_LEFT   0
#define STDVR_EYE_RIGHT  1
#define STDVR_EYE_COUNT  2

// VR Controller indices
#define STDVR_CONTROLLER_LEFT   0
#define STDVR_CONTROLLER_RIGHT  1
#define STDVR_CONTROLLER_COUNT  2

// VR Button bit flags
#define STDVR_BTN_TRIGGER_R    0x0001
#define STDVR_BTN_TRIGGER_L    0x0002
#define STDVR_BTN_GRIP_R       0x0004
#define STDVR_BTN_GRIP_L       0x0008
#define STDVR_BTN_A            0x0010
#define STDVR_BTN_B            0x0020
#define STDVR_BTN_X            0x0040
#define STDVR_BTN_Y            0x0080
#define STDVR_BTN_MENU         0x0100
#define STDVR_BTN_THUMBSTICK_L 0x0200
#define STDVR_BTN_THUMBSTICK_R 0x0400

// VR movement direction modes
#define STDVR_MOVE_HEAD       0  // Movement follows head orientation
#define STDVR_MOVE_CONTROLLER 1  // Movement follows controller orientation

// VR turn modes
#define STDVR_TURN_SMOOTH     0
#define STDVR_TURN_SNAP       1

// Per-eye view information
typedef struct stdVR_EyeView {
    rdMatrix34 viewMatrix;          // Eye view matrix (camera space)
    float projectionMatrix[16];     // 4x4 asymmetric projection matrix
    float fovLeft;                  // Left tangent of FOV
    float fovRight;                 // Right tangent of FOV
    float fovUp;                    // Up tangent of FOV
    float fovDown;                  // Down tangent of FOV
} stdVR_EyeView;

// Controller pose and state
typedef struct stdVR_ControllerState {
    rdVector3 position;             // Position in world space
    rdVector3 orientation;          // Pitch, Yaw, Roll in degrees
    rdMatrix34 poseMatrix;          // Full pose matrix
    int bTracking;                  // Is controller being tracked
    int bActive;                    // Is controller active/connected
} stdVR_ControllerState;

// Main VR client information structure
typedef struct stdVR_ClientInfo {
    // HMD tracking
    rdVector3 hmdPosition;          // HMD position in world space
    rdVector3 hmdOrientation;       // HMD orientation (Pitch, Yaw, Roll)
    rdMatrix34 hmdPoseMatrix;       // Full HMD pose matrix

    // Per-eye view data
    stdVR_EyeView eyes[STDVR_EYE_COUNT];

    // Controller tracking
    stdVR_ControllerState controllers[STDVR_CONTROLLER_COUNT];

    // Decoupled movement direction (for comfort)
    rdVector3 moveForward;          // Forward direction for movement
    float moveYaw;                  // Yaw angle for movement calculations

    // Analog input state
    float analogMove[2];            // Left stick: [0]=X (strafe), [1]=Y (forward)
    float analogTurn[2];            // Right stick: [0]=X (turn), [1]=Y (unused)
    float triggerLeft;              // Left trigger value (0-1)
    float triggerRight;             // Right trigger value (0-1)
    float gripLeft;                 // Left grip value (0-1)
    float gripRight;                // Right grip value (0-1)

    // Button state
    uint32_t buttonState;           // Currently pressed buttons
    uint32_t buttonPressed;         // Buttons pressed this frame
    uint32_t buttonReleased;        // Buttons released this frame

    // Runtime info
    int bSessionRunning;            // Is VR session active
    int bShouldRender;              // Should we render this frame
    int64_t predictedDisplayTime;   // Predicted display time for this frame

    // Render target info
    int renderWidth;                // Per-eye render width
    int renderHeight;               // Per-eye render height

    // Screen layer mode (for menus/cinematics)
    int bUseScreenLayer;            // True when showing 2D content in quad layer
    rdVector3 screenLayerSnapPos;   // HMD position when entering screen layer mode
    float screenLayerSnapYaw;       // HMD yaw when entering screen layer mode
    float screenLayerDistance;      // Distance of screen from player (meters)
    float screenLayerWidth;         // Screen width in meters
    float screenLayerHeight;        // Screen height in meters
} stdVR_ClientInfo;

// VR configuration settings
typedef struct stdVR_Config {
    // Movement settings
    int moveDirection;              // STDVR_MOVE_HEAD or STDVR_MOVE_CONTROLLER
    int turnMode;                   // STDVR_TURN_SMOOTH or STDVR_TURN_SNAP
    int snapTurnAngle;              // Snap turn angle in degrees (30, 45, 90)
    float smoothTurnSpeed;          // Smooth turn speed in degrees/sec

    // Scale and comfort
    float worldScale;               // World scale multiplier (default 1.0)
    float heightOffset;             // Player height offset in meters
    int bComfortVignette;           // Enable comfort vignette during movement

    // Handedness
    int dominantHand;               // 0=left, 1=right

    // Quality settings
    float supersampling;            // Render scale multiplier
} stdVR_Config;

#ifdef __cplusplus
}
#endif

#endif // _STDVR_TYPES_H
