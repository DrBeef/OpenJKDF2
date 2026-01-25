#include "stdVR_AlignmentTool.h"

// Added: VR weapon alignment tool for real-time offset adjustment

#ifdef VR_WEAPON_ALIGNMENT_TOOL

#include "Platform/VR/stdVR.h"
#include "Platform/VR/stdVR_WeaponOffsets.h"
#include "General/stdFont.h"
#include "World/jkPlayer.h"
#include "stdPlatform.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

// Tool state
static int stdVR_alignmentTool_bActive = 0;
static int stdVR_alignmentTool_bInitted = 0;
static stdVR_AlignmentMode stdVR_alignmentTool_mode = STDVR_ALIGN_POSITION;

// Input state tracking for debouncing
static int stdVR_alignmentTool_bGripsHeld = 0;
static int stdVR_alignmentTool_bBPressed = 0;
static int stdVR_alignmentTool_bAPressed = 0;

// Adjustment speeds
#define STDVR_ALIGN_POS_SPEED   0.2f    // meters per second
#define STDVR_ALIGN_SCALE_SPEED 0.5f    // scale units per second
#define STDVR_ALIGN_PITCH_SPEED 30.0f   // degrees per second

// Stick deadzone
#define STDVR_ALIGN_DEADZONE    0.2f

// External font for overlay (from jkHud)
extern stdFont* jkHud_pMsgFontSft;
extern flex_t jkPlayer_hudScale;

void stdVR_AlignmentTool_Startup(void)
{
    if (stdVR_alignmentTool_bInitted) {
        return;
    }

    stdVR_alignmentTool_bActive = 0;
    stdVR_alignmentTool_mode = STDVR_ALIGN_POSITION;
    stdVR_alignmentTool_bGripsHeld = 0;
    stdVR_alignmentTool_bBPressed = 0;
    stdVR_alignmentTool_bAPressed = 0;

    stdVR_alignmentTool_bInitted = 1;
    stdPlatform_Printf("stdVR_AlignmentTool: Initialized\n");
}

void stdVR_AlignmentTool_Toggle(void)
{
    stdVR_alignmentTool_bActive = !stdVR_alignmentTool_bActive;

    if (stdVR_alignmentTool_bActive) {
        stdPlatform_Printf("stdVR_AlignmentTool: Activated\n");
    } else {
        // Save offsets when deactivating
        stdVR_WeaponOffsets_Save();
        stdPlatform_Printf("stdVR_AlignmentTool: Deactivated and saved\n");
    }
}

int stdVR_AlignmentTool_IsActive(void)
{
    return stdVR_alignmentTool_bActive;
}

stdVR_AlignmentMode stdVR_AlignmentTool_GetMode(void)
{
    return stdVR_alignmentTool_mode;
}

static float stdVR_AlignmentTool_ApplyDeadzone(float value)
{
    if (fabsf(value) < STDVR_ALIGN_DEADZONE) {
        return 0.0f;
    }
    // Remap value from deadzone..1.0 to 0.0..1.0
    float sign = value < 0 ? -1.0f : 1.0f;
    float absVal = fabsf(value);
    return sign * (absVal - STDVR_ALIGN_DEADZONE) / (1.0f - STDVR_ALIGN_DEADZONE);
}

void stdVR_AlignmentTool_Update(float deltaSeconds)
{
    if (!stdVR_bEnabled) {
        return;
    }

    // Check for toggle activation: both grips + B button
    int bBothGrips = (stdVR_clientInfo.gripLeft > 0.5f) && (stdVR_clientInfo.gripRight > 0.5f);
    int bBButton = (stdVR_clientInfo.buttonState & STDVR_BTN_B) != 0;

    // Toggle on B press while both grips held (with debounce)
    if (bBothGrips && bBButton) {
        if (!stdVR_alignmentTool_bGripsHeld || !stdVR_alignmentTool_bBPressed) {
            stdVR_AlignmentTool_Toggle();
        }
    }
    stdVR_alignmentTool_bGripsHeld = bBothGrips;
    stdVR_alignmentTool_bBPressed = bBButton;

    if (!stdVR_alignmentTool_bActive) {
        return;
    }

    // Check for mode cycle: A button
    int bAButton = (stdVR_clientInfo.buttonState & STDVR_BTN_A) != 0;
    if (bAButton && !stdVR_alignmentTool_bAPressed) {
        stdVR_alignmentTool_mode = (stdVR_alignmentTool_mode + 1) % STDVR_ALIGN_MODE_COUNT;
        stdPlatform_Printf("stdVR_AlignmentTool: Mode changed to %d\n", stdVR_alignmentTool_mode);
    }
    stdVR_alignmentTool_bAPressed = bAButton;

    // Get current weapon offset
    stdVR_WeaponOffset* pOffset = stdVR_GetCurrentWeaponOffset();
    if (!pOffset) {
        return;
    }

    // Get stick inputs
    float leftStickX = stdVR_AlignmentTool_ApplyDeadzone(stdVR_clientInfo.analogMove[0]);
    float leftStickY = stdVR_AlignmentTool_ApplyDeadzone(stdVR_clientInfo.analogMove[1]);
    float rightStickY = stdVR_AlignmentTool_ApplyDeadzone(stdVR_clientInfo.analogTurn[1]);

    // Apply adjustments based on mode
    switch (stdVR_alignmentTool_mode) {
        case STDVR_ALIGN_POSITION:
            // Left stick X/Y = offset X/Y, Right stick Y = offset Z
            pOffset->offsetX += leftStickX * STDVR_ALIGN_POS_SPEED * deltaSeconds;
            pOffset->offsetY += leftStickY * STDVR_ALIGN_POS_SPEED * deltaSeconds;
            pOffset->offsetZ += rightStickY * STDVR_ALIGN_POS_SPEED * deltaSeconds;
            break;

        case STDVR_ALIGN_SCALE:
            // Left stick Y = scale adjustment
            pOffset->modelScale += leftStickY * STDVR_ALIGN_SCALE_SPEED * deltaSeconds;
            // Clamp scale to reasonable range
            if (pOffset->modelScale < 0.1f) pOffset->modelScale = 0.1f;
            if (pOffset->modelScale > 5.0f) pOffset->modelScale = 5.0f;
            break;

        case STDVR_ALIGN_PITCH:
            // Left stick Y = pitch adjustment
            pOffset->pitchAdjust += leftStickY * STDVR_ALIGN_PITCH_SPEED * deltaSeconds;
            // Clamp pitch to reasonable range
            if (pOffset->pitchAdjust < -90.0f) pOffset->pitchAdjust = -90.0f;
            if (pOffset->pitchAdjust > 90.0f) pOffset->pitchAdjust = 90.0f;
            break;

        default:
            break;
    }

    // Mark this weapon as configured
    pOffset->bConfigured = 1;
}

void stdVR_AlignmentTool_DrawOverlay(void)
{
    if (!stdVR_alignmentTool_bActive) {
        return;
    }

    if (!jkHud_pMsgFontSft) {
        return;
    }

    int binIdx = stdVR_GetCurrentWeaponBin();
    stdVR_WeaponOffset* pOffset = stdVR_GetCurrentWeaponOffset();

    char buf[256];
    int y = 50;
    int fontHeight = stdFont_GetHeight(jkHud_pMsgFontSft) + 2;
    flex_t scale = jkPlayer_hudScale;

    // Title
    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "=== VR WEAPON ALIGNMENT ===", 1, scale);
    y += fontHeight;

    // Current weapon bin
    snprintf(buf, sizeof(buf), "Weapon Bin: %d", binIdx);
    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
    y += fontHeight;

    // Current mode
    const char* modeNames[] = { "POSITION", "SCALE", "PITCH" };
    snprintf(buf, sizeof(buf), "Mode: %s (A to cycle)",
        stdVR_alignmentTool_mode < STDVR_ALIGN_MODE_COUNT ? modeNames[stdVR_alignmentTool_mode] : "???");
    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
    y += fontHeight;
    y += fontHeight; // Extra spacing

    if (pOffset) {
        // Position values
        snprintf(buf, sizeof(buf), "Offset X: %.3f m", pOffset->offsetX);
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
        y += fontHeight;

        snprintf(buf, sizeof(buf), "Offset Y: %.3f m", pOffset->offsetY);
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
        y += fontHeight;

        snprintf(buf, sizeof(buf), "Offset Z: %.3f m", pOffset->offsetZ);
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
        y += fontHeight;

        snprintf(buf, sizeof(buf), "Scale: %.2f", pOffset->modelScale);
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
        y += fontHeight;

        snprintf(buf, sizeof(buf), "Pitch: %.1f deg", pOffset->pitchAdjust);
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
        y += fontHeight;

        snprintf(buf, sizeof(buf), "Configured: %s", pOffset->bConfigured ? "Yes" : "No");
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, buf, 1, scale);
        y += fontHeight;
    } else {
        stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "No weapon selected", 1, scale);
        y += fontHeight;
    }

    y += fontHeight; // Extra spacing

    // Controls help
    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "Controls:", 1, scale);
    y += fontHeight;
    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "  Both Grips + B: Toggle tool", 1, scale);
    y += fontHeight;
    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "  A: Cycle mode", 1, scale);
    y += fontHeight;

    switch (stdVR_alignmentTool_mode) {
        case STDVR_ALIGN_POSITION:
            stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "  Left Stick: X/Y offset", 1, scale);
            y += fontHeight;
            stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "  Right Stick Y: Z offset", 1, scale);
            break;
        case STDVR_ALIGN_SCALE:
            stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "  Left Stick Y: Scale", 1, scale);
            break;
        case STDVR_ALIGN_PITCH:
            stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, 10, y, 640, "  Left Stick Y: Pitch", 1, scale);
            break;
        default:
            break;
    }
}

#endif // VR_WEAPON_ALIGNMENT_TOOL
