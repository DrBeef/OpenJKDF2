#include "stdVR_Input.h"

// Added: VR controller input mapping implementation

#ifdef PLATFORM_VR

#include "stdVR.h"
#include "stdVR_Types.h"
#include "Platform/stdControl.h"
#include "stdPlatform.h"
#include "General/stdMath.h"
#include "Primitives/rdVector.h"
#include "Cog/sithCog.h"

#include "Main/jkHud.h"
#include "Main/jkMain.h"
#include "Main/jkSmack.h"

#include <math.h>

// Snap turn state
static int stdVR_snapTurnPending = 0;
static float stdVR_lastSnapTurnInput = 0.0f;

// Menu button state for long-press detection
static int stdVR_menuButtonHeld = 0;
static uint32_t stdVR_menuButtonHoldStart = 0;
#define STDVR_MENU_LONGPRESS_MS 1000  // Hold menu 1 second to recenter

// Walk/run toggle (left thumbstick click)
static int stdVR_walkMode = 1;  // 0 = run (default), 1 = walk

// Deadzone for thumbsticks
#define STDVR_THUMBSTICK_DEADZONE 0.2f
#define STDVR_SNAP_TURN_THRESHOLD 0.7f

// External: get current time in ms
extern uint32_t stdPlatform_GetTimeMsec(void);
// External: controller escape key hook (handled in Window.c)
extern int stdControl_bControllerEscapeKey;

// Apply deadzone to thumbstick input
static float ApplyDeadzone(float value, float deadzone)
{
    if (value > deadzone) {
        return (value - deadzone) / (1.0f - deadzone);
    } else if (value < -deadzone) {
        return (value + deadzone) / (1.0f - deadzone);
    }
    return 0.0f;
}

void stdVR_Input_ProcessSnapTurn(void)
{
    if (!stdVR_bEnabled) {
        return;
    }

    float turnInput = stdVR_clientInfo.analogTurn[0];

    // Apply deadzone
    turnInput = ApplyDeadzone(turnInput, STDVR_THUMBSTICK_DEADZONE);

    if (stdVR_config.turnMode == STDVR_TURN_SNAP) {
        // Snap turn logic
        if (fabsf(turnInput) > STDVR_SNAP_TURN_THRESHOLD && fabsf(stdVR_lastSnapTurnInput) <= STDVR_SNAP_TURN_THRESHOLD) {
            // Trigger snap turn (negate: positive X = right on stick, but positive yaw = left in JKDF2)
            stdVR_snapTurnPending = (turnInput > 0) ? -1 : 1;
        } else if (fabsf(turnInput) <= STDVR_SNAP_TURN_THRESHOLD) {
            // Reset when returning to center
            stdVR_snapTurnPending = 0;
        }
        stdVR_lastSnapTurnInput = turnInput;
    } else {
        stdVR_snapTurnPending = 0;
    }
}

void stdVR_Input_GetMovementDirection(float* pMoveX, float* pMoveY)
{
    if (!stdVR_bEnabled || !pMoveX || !pMoveY) {
        return;
    }

    // Get raw thumbstick input with deadzone
    // OpenXR left stick X is inverted relative to JK strafe (left reports negative),
    // so flip it here to keep "left = strafe left".
    float moveX = -ApplyDeadzone(stdVR_clientInfo.analogMove[0], STDVR_THUMBSTICK_DEADZONE);
    float moveY = ApplyDeadzone(stdVR_clientInfo.analogMove[1], STDVR_THUMBSTICK_DEADZONE);

    // Movement is already oriented based on controller/head direction
    // The actual transformation happens in sithControl when we apply moveYaw
    *pMoveX = moveX;
    *pMoveY = moveY;
}

// Track if menu button triggered escape this frame
static int stdVR_menuTriggeredThisFrame = 0;

void stdVR_Input_MapToGame(void)
{
    if (!stdVR_bEnabled) {
        return;
    }

    stdVR_menuTriggeredThisFrame = 0;

    // Process snap turn
    stdVR_Input_ProcessSnapTurn();

    // Get movement input
    float moveX = 0.0f, moveY = 0.0f;
    stdVR_Input_GetMovementDirection(&moveX, &moveY);

    // Map movement axes
    // INPUT_FUNC_FORWARD = 0, INPUT_FUNC_TURN = 1, INPUT_FUNC_SLIDE = 2, etc.
    // These will be handled in sithControl_PlayerMovement when VR is enabled

    // Handle turn input
    if (stdVR_config.turnMode == STDVR_TURN_SNAP) {
        // Snap turn is handled separately via stdVR_snapTurnPending
        // Return the pending snap turn angle
        if (stdVR_snapTurnPending != 0) {
            // Snap turn will be applied in sithControl
        }
    } else {
        // Smooth turn - let the axis value pass through
    }

    // Handle menu button (escape/pause)
    // Short press = menu/escape, Long press = recenter
    // Accept Y button as menu on Quest (the system menu button isn't accessible)
    int menuButtonDown = (stdVR_clientInfo.buttonState & STDVR_BTN_MENU) != 0;
    if (menuButtonDown) {
        if (!stdVR_menuButtonHeld) {
            // Button just pressed
            stdVR_menuButtonHeld = 1;
            stdVR_menuButtonHoldStart = stdPlatform_GetTimeMsec();
        } else if (stdVR_menuButtonHeld == 1) {
            // Check for long press (recenter)
            uint32_t holdTime = stdPlatform_GetTimeMsec() - stdVR_menuButtonHoldStart;
            if (holdTime >= STDVR_MENU_LONGPRESS_MS) {
                // Long press detected - recenter view
                stdVR_RecenterView();
                stdVR_TriggerHaptic(STDVR_CONTROLLER_LEFT, 0.8f, 0.2f, 200.0f);
                stdVR_menuButtonHeld = 2;  // Mark as handled (prevent repeated recenter)
            }
        }
    } else {
        // Button released
        if (stdVR_menuButtonHeld == 1) {
            // Short press - trigger escape/menu
            stdVR_menuTriggeredThisFrame = 1;
            stdVR_TriggerHaptic(STDVR_CONTROLLER_LEFT, 0.3f, 0.1f, 100.0f);
        }
        stdVR_menuButtonHeld = 0;
    }
    // Mirror JKXR-style logic: use screen layer when UI/cinematics/menus are active
    int guiState = jkSmack_GetCurrentGuiState();
    int inGameplay = (guiState == JK_GAMEMODE_GAMEPLAY);
    if (inGameplay && stdVR_menuTriggeredThisFrame) {
        if (Main_bMotsCompat) {
            if (!jkGuiMultiplayer_mpcInfo.pCutsceneCog) {
                if (jkHud_bChatOpen)
                    jkHud_idk_time();
                else
                    jkMain_do_guistate6();
            } else {
                sithCog_SendMessage(jkGuiMultiplayer_mpcInfo.pCutsceneCog, SITH_MESSAGE_ESCAPED, 0,
                                    0, 0, 0, 0);
            }
        } else {
            if (jkHud_bChatOpen)
                jkHud_idk_time();
            else
                jkMain_do_guistate6();
        }
    }

    // Simulate a controller escape key press on short menu release (one-frame pulse)
    stdControl_bControllerEscapeKey = stdVR_menuTriggeredThisFrame ? 1 : 0;

    // Map buttons to game actions using stdControl functions
    // Note: We don't call stdControl_SetKey directly here because
    // the actual input processing happens in the sithControl tick.
    // Instead, the VR button state is read directly by the modified
    // sithControl code when PLATFORM_VR is defined.

    // Trigger haptic feedback for fire
    if (stdVR_clientInfo.buttonPressed & STDVR_BTN_TRIGGER_R) {
        stdVR_TriggerHaptic(STDVR_CONTROLLER_RIGHT, 0.5f, 0.1f, 100.0f);
    }

    // Trigger haptic feedback for alt fire (grip)
    if (stdVR_clientInfo.buttonPressed & STDVR_BTN_GRIP_R) {
        stdVR_TriggerHaptic(STDVR_CONTROLLER_RIGHT, 0.3f, 0.1f, 50.0f);
    }

    // Trigger haptic feedback for force power
    if (stdVR_clientInfo.buttonPressed & STDVR_BTN_TRIGGER_L) {
        stdVR_TriggerHaptic(STDVR_CONTROLLER_LEFT, 0.4f, 0.15f, 75.0f);
    }

    // Left thumbstick click = toggle walk/run mode
    if (stdVR_clientInfo.buttonPressed & STDVR_BTN_THUMBSTICK_L) {
        stdVR_walkMode = !stdVR_walkMode;
        // Haptic feedback: short pulse for run, double pulse for walk
        if (stdVR_walkMode) {
            // Walk mode: two short pulses
            stdVR_TriggerHaptic(STDVR_CONTROLLER_LEFT, 0.3f, 0.1f, 100.0f);
        } else {
            // Run mode: one longer pulse
            stdVR_TriggerHaptic(STDVR_CONTROLLER_LEFT, 0.5f, 0.15f, 150.0f);
        }
    }
}

// Check if menu button just triggered escape (short press release)
int stdVR_Input_IsMenuPressed(void)
{
    return stdVR_menuTriggeredThisFrame;
}

// Get current snap turn angle (called from sithControl)
int stdVR_Input_GetSnapTurnAngle(void)
{
    if (!stdVR_bEnabled || stdVR_config.turnMode != STDVR_TURN_SNAP) {
        return 0;
    }

    if (stdVR_snapTurnPending != 0) {
        int angle = stdVR_snapTurnPending * stdVR_config.snapTurnAngle;
        stdVR_snapTurnPending = 0; // Consume the snap turn
        return angle;
    }

    return 0;
}

// Get smooth turn speed (called from sithControl)
float stdVR_Input_GetSmoothTurnSpeed(void)
{
    if (!stdVR_bEnabled || stdVR_config.turnMode != STDVR_TURN_SMOOTH) {
        return 0.0f;
    }

    float turnInput = ApplyDeadzone(stdVR_clientInfo.analogTurn[0], STDVR_THUMBSTICK_DEADZONE);
    // Negate because positive X = right on stick, but positive yaw = left in JKDF2
    return -turnInput * stdVR_config.smoothTurnSpeed;
}

// Check if a VR button is currently pressed
int stdVR_Input_IsButtonDown(uint32_t button)
{
    if (!stdVR_bEnabled) {
        return 0;
    }
    return (stdVR_clientInfo.buttonState & button) != 0;
}

// Check if a VR button was just pressed this frame
int stdVR_Input_IsButtonPressed(uint32_t button)
{
    if (!stdVR_bEnabled) {
        return 0;
    }
    return (stdVR_clientInfo.buttonPressed & button) != 0;
}

// Check if a VR button was just released this frame
int stdVR_Input_IsButtonReleased(uint32_t button)
{
    if (!stdVR_bEnabled) {
        return 0;
    }
    return (stdVR_clientInfo.buttonReleased & button) != 0;
}

// Get trigger value (0-1)
float stdVR_Input_GetTrigger(int hand)
{
    if (!stdVR_bEnabled) {
        return 0.0f;
    }
    return (hand == STDVR_CONTROLLER_LEFT) ? stdVR_clientInfo.triggerLeft : stdVR_clientInfo.triggerRight;
}

// Get grip value (0-1)
float stdVR_Input_GetGrip(int hand)
{
    if (!stdVR_bEnabled) {
        return 0.0f;
    }
    return (hand == STDVR_CONTROLLER_LEFT) ? stdVR_clientInfo.gripLeft : stdVR_clientInfo.gripRight;
}

// Check if walk mode is active (toggled via left thumbstick click)
int stdVR_Input_IsWalkMode(void)
{
    if (!stdVR_bEnabled) {
        return 0;
    }
    return stdVR_walkMode;
}

#endif // PLATFORM_VR
