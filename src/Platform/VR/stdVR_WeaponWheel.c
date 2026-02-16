#include "stdVR_WeaponWheel.h"

#ifdef PLATFORM_VR

#include "stdVR.h"
#include "stdVR_Types.h"
#include "stdVR_Input.h"
#include "stdPlatform.h"
#include "Platform/std3D.h"
#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "Gameplay/sithInventory.h"
#include "World/sithWeapon.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithTime.h"
#include "World/jkPlayer.h"
#include "Primitives/rdVector.h"
#include "Primitives/rdMatrix.h"
#include "Engine/rdThing.h"
#include "Engine/rdCamera.h"
#include "Raster/rdCache.h"
#include "World/sithModel.h"
#include "Main/Main.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

// External declarations
extern sithThing* sithPlayer_pLocalPlayerThing;
extern sithItemDescriptor sithInventory_aDescriptors[];
extern stdFont* jkHud_pMsgFontSft;
extern stdVBuffer Video_menuBuffer;
extern int sithOverlayMap_bShowMap;

// Time scale variable (read by sithTime_SetDelta)
float stdVR_weaponWheelTimeScale = 1.0f;

// Wheel state
static stdVR_WheelState stdVR_wheelState = {0};

// Cached weapon models for 3D wheel rendering
typedef struct stdVR_WheelModelCache {
    rdModel3* pModel;       // Pointer to the weapon's POV model (owned by game, not us)
    rdThing rdThing;        // Our own rdThing wrapper for rendering
    int bInitialized;       // 1 if rdThing has been set up for this model
} stdVR_WheelModelCache;

static stdVR_WheelModelCache stdVR_aWheelModelCache[SITHBIN_NUMBINS] = {0};
static float stdVR_wheelRotation = 0.0f;  // Slow spin animation (radians)

// ============================================================================
// Internal: Known POV model filenames for each weapon bin.
// JK and MOTS use different names for fists; other weapons are the same.
// ============================================================================
typedef struct stdVR_WeaponModelName {
    int binIdx;
    const char* aNames[3]; // NULL-terminated list of possible filenames
} stdVR_WeaponModelName;

static const stdVR_WeaponModelName stdVR_aWeaponModelNames[] = {
    { SITHBIN_FISTS,              { "fistv.3do", "kyhand.3do", NULL } },
    { SITHBIN_BRYARPISTOL,        { "bryv.3do",  NULL } },
    { SITHBIN_STORMTROOPER_RIFLE, { "strv.3do",  NULL } },
    { SITHBIN_THERMAL_DETONATOR,  { "detv.3do",  NULL } },
    { SITHBIN_TUSKEN_PROD,        { "bowv.3do",  NULL } },
    { SITHBIN_REPEATER,           { "rptv.3do",  NULL } },
    { SITHBIN_RAIL_DETONATOR,     { "rldv.3do",  NULL } },
    { SITHBIN_SEQUENCER_CHARGE,   { "seqv.3do",  NULL } },
    { SITHBIN_CONCUSSION_RIFLE,   { "conv.3do",  NULL } },
    { SITHBIN_LIGHTSABER,         { "sabv.3do",  NULL } },
};
#define STDVR_NUM_WEAPON_MODELS (sizeof(stdVR_aWeaponModelNames) / sizeof(stdVR_aWeaponModelNames[0]))

// ============================================================================
// Internal: Ensure all weapon models are cached by loading from known names
// ============================================================================
static void stdVR_WeaponWheel_EnsureModelsCached(void)
{
    for (int w = 0; w < (int)STDVR_NUM_WEAPON_MODELS; w++) {
        int binIdx = stdVR_aWeaponModelNames[w].binIdx;
        if (stdVR_aWheelModelCache[binIdx].bInitialized) continue;

        for (int n = 0; stdVR_aWeaponModelNames[w].aNames[n]; n++) {
            rdModel3* pModel = sithModel_LoadEntry(stdVR_aWeaponModelNames[w].aNames[n], 0);
            if (pModel) {
                stdVR_WeaponWheel_CacheModel(binIdx, pModel);
                break;
            }
        }
    }
}

// Initial controller yaw/pitch captured when wheel opens (for relative pointing)
static float stdVR_wheelInitialYaw = 0.0f;
static float stdVR_wheelInitialPitch = 0.0f;

// Current pointing deviation (for drawing pointer cursor)
static float stdVR_wheelPointerX = 0.0f;  // Normalized: -1 to +1 (left to right)
static float stdVR_wheelPointerY = 0.0f;  // Normalized: -1 to +1 (down to up)

// Minimum angular deviation (degrees) from rest position to register a selection
#define STDVR_WHEEL_POINT_THRESHOLD 8.0f

// Pi constant
#define STDVR_PI 3.14159265f

// ============================================================================
// Internal: Get readable name for a bin index
// ============================================================================
static const char* stdVR_WeaponWheel_GetName(int binIdx)
{
    switch (binIdx) {
        // Weapons
        case SITHBIN_FISTS:              return "Fists";
        case SITHBIN_BRYARPISTOL:        return "Bryar Pistol";
        case SITHBIN_STORMTROOPER_RIFLE: return "Stormtrooper Rifle";
        case SITHBIN_THERMAL_DETONATOR:  return "Thermal Detonator";
        case SITHBIN_TUSKEN_PROD:        return "Bowcaster";
        case SITHBIN_REPEATER:           return "Repeater";
        case SITHBIN_RAIL_DETONATOR:     return "Rail Detonator";
        case SITHBIN_SEQUENCER_CHARGE:   return "Sequencer Charge";
        case SITHBIN_CONCUSSION_RIFLE:   return "Concussion Rifle";
        case SITHBIN_LIGHTSABER:         return "Lightsaber";
        // Force powers
        case SITHBIN_F_JUMP:         return "Force Jump";
        case SITHBIN_F_SPEED:        return "Force Speed";
        case SITHBIN_F_SEEING:       return "Force Seeing";
        case SITHBIN_F_PULL:         return "Force Pull";
        case SITHBIN_F_HEALING:      return "Force Healing";
        case SITHBIN_F_PERSUASION:   return "Force Persuasion";
        case SITHBIN_F_BLINDING:     return "Force Blinding";
        case SITHBIN_F_ABSORB:       return "Force Absorb";
        case SITHBIN_F_PROTECTION:   return "Force Protection";
        case SITHBIN_F_THROW:        return "Force Throw";
        case SITHBIN_F_GRIP:         return "Force Grip";
        case SITHBIN_F_LIGHTNING:    return "Force Lightning";
        case SITHBIN_F_DESTRUCTION:  return "Force Destruction";
        case SITHBIN_F_DEADLYSIGHT:  return "Force Deadly Sight";
        // MOTS force powers
        case SITHBIN_F_FARSIGHT:     return "Force Far Sight";
        case SITHBIN_F_PROJECT:      return "Force Projection";
        case SITHBIN_F_SABERTHROW:   return "Saber Throw";
        case SITHBIN_F_PUSH:         return "Force Push";
        case SITHBIN_F_CHAINLIGHT:   return "Chain Lightning";
        default:                     return "Unknown";
    }
}

// ============================================================================
// Internal: Build segments for weapon wheel
// ============================================================================
static int stdVR_WeaponWheel_BuildWeaponSegments(void)
{
    if (!sithPlayer_pLocalPlayerThing || !sithPlayer_pLocalPlayerThing->actorParams.playerinfo) {
        return 0;
    }

    sithPlayerInfo* pPlayerInfo = sithPlayer_pLocalPlayerThing->actorParams.playerinfo;
    int count = 0;

    for (int bin = SITHBIN_FISTS; bin <= SITHBIN_LIGHTSABER; bin++) {
        if (count >= STDVR_WHEEL_MAX_SEGMENTS) break;

        sithItemDescriptor* pDesc = &sithInventory_aDescriptors[bin];
        sithItemInfo* pInfo = &pPlayerInfo->iteminfo[bin];

        if ((pInfo->state & ITEMSTATE_AVAILABLE) && (pDesc->flags & ITEMINFO_WEAPON) && pInfo->ammoAmt > 0.0f) {
            stdVR_wheelState.aSegments[count].binIdx = bin;
            stdVR_wheelState.aSegments[count].pIcon = pDesc->hudBitmap;
            count++;
        }
    }

    return count;
}

// ============================================================================
// Internal: Build segments for force power wheel
// ============================================================================
static int stdVR_WeaponWheel_BuildForceSegments(void)
{
    if (!sithPlayer_pLocalPlayerThing || !sithPlayer_pLocalPlayerThing->actorParams.playerinfo) {
        return 0;
    }

    sithPlayerInfo* pPlayerInfo = sithPlayer_pLocalPlayerThing->actorParams.playerinfo;
    int count = 0;

    int fpEnd = Main_bMotsCompat ? SITHBIN_F_CHAINLIGHT : SITHBIN_F_DEADLYSIGHT;

    for (int bin = SITHBIN_F_JUMP; bin <= fpEnd; bin++) {
        if (count >= STDVR_WHEEL_MAX_SEGMENTS) break;

        sithItemDescriptor* pDesc = &sithInventory_aDescriptors[bin];
        sithItemInfo* pInfo = &pPlayerInfo->iteminfo[bin];

        if ((pInfo->state & ITEMSTATE_AVAILABLE) && (pDesc->flags & ITEMINFO_POWER) && pInfo->ammoAmt > 0.0f) {
            stdVR_wheelState.aSegments[count].binIdx = bin;
            stdVR_wheelState.aSegments[count].pIcon = pDesc->hudBitmap;
            count++;
        }
    }

    return count;
}

// ============================================================================
// Internal: Compute controller yaw and pitch.
// Yaw is relative to the HMD's forward direction (horizontal plane).
// Pitch is relative to the world horizon (gravity), with ~90 degrees
// subtracted to compensate for the natural controller grip angle.
// In JK coordinates: X=right, Y=forward, Z=up.
// ============================================================================
static void stdVR_WeaponWheel_GetControllerAngles(int hand, float* pYaw, float* pPitch)
{
    *pYaw = 0.0f;
    *pPitch = 0.0f;

    stdVR_ControllerState* pCtrl = &stdVR_clientInfo.controllers[hand];
    if (!pCtrl->bTracking) return;

    *pYaw = stdVR_clientInfo.hmdOrientation.y - pCtrl->orientation.y;
    *pPitch = pCtrl->orientation.x - 50.f;
}

// ============================================================================
// Internal: Get selection angle from controller yaw/pitch deviation.
// Returns angle on wheel (0=up, clockwise) and angular deviation magnitude.
// ============================================================================
static void stdVR_WeaponWheel_GetPointingAngle(int hand, float* pAngle, float* pMagnitude)
{
    *pAngle = 0.0f;
    *pMagnitude = 0.0f;

    float yaw = 0.0f, pitch = 0.0f;
    stdVR_WeaponWheel_GetControllerAngles(hand, &yaw, &pitch);

    // Deviation from the rest position captured when wheel opened
    float yawDev = yaw - stdVR_wheelInitialYaw;

    // Store normalized pointer position for cursor drawing
    float maxAngle = 20.f;
    stdVR_wheelPointerX = yawDev / maxAngle;
    stdVR_wheelPointerY = pitch / maxAngle;

    // Clamp to -1..1
    if (stdVR_wheelPointerX > 1.0f) stdVR_wheelPointerX = 1.0f;
    if (stdVR_wheelPointerX < -1.0f) stdVR_wheelPointerX = -1.0f;
    if (stdVR_wheelPointerY > 1.0f) stdVR_wheelPointerY = 1.0f;
    if (stdVR_wheelPointerY < -1.0f) stdVR_wheelPointerY = -1.0f;

    *pMagnitude = sqrtf(yawDev * yawDev + pitch * pitch);

    // Map to wheel angle: yaw=right, pitch=up → 0=up, clockwise positive
    *pAngle = atan2f(yawDev, pitch);
    if (*pAngle < 0.0f) *pAngle += 2.0f * STDVR_PI;
}

// ============================================================================
// Public: Update weapon wheel state
// ============================================================================
void stdVR_WeaponWheel_Update(void)
{
    if (!stdVR_bEnabled || !stdVR_IsSessionRunning()) {
        return;
    }

    // Don't activate wheels while the holomap is showing
    if (sithOverlayMap_bShowMap) {
        return;
    }

    int dominantHand = stdVR_config.dominantHand;
    int offhand = 1 - dominantHand;

    uint32_t btnGripDominant = (dominantHand == STDVR_CONTROLLER_RIGHT) ? STDVR_BTN_GRIP_R : STDVR_BTN_GRIP_L;
    uint32_t btnGripOffhand = (offhand == STDVR_CONTROLLER_RIGHT) ? STDVR_BTN_GRIP_R : STDVR_BTN_GRIP_L;

    // Check for wheel activation (grip pressed)
    if (stdVR_clientInfo.buttonPressed & btnGripDominant) {
        stdVR_WeaponWheel_EnsureModelsCached();
        int numSegs = stdVR_WeaponWheel_BuildWeaponSegments();
        if (numSegs > 0) {
            stdVR_wheelState.activeWheel = STDVR_WHEEL_WEAPON;
            stdVR_wheelState.numSegments = numSegs;
            stdVR_wheelState.highlightedSegment = -1;
            stdVR_wheelState.prevHighlightedSegment = -1;
            stdVR_weaponWheelTimeScale = 0.1f;
            stdVR_wheelPointerX = 0.0f;
            stdVR_wheelPointerY = 0.0f;
            // Capture rest yaw/pitch for relative pointing
            stdVR_WeaponWheel_GetControllerAngles(dominantHand, &stdVR_wheelInitialYaw, &stdVR_wheelInitialPitch);
            stdVR_TriggerHaptic(dominantHand, 0.3f, 0.1f, 100.0f);
        }
    }
    else if (stdVR_clientInfo.buttonPressed & btnGripOffhand) {
        int numSegs = stdVR_WeaponWheel_BuildForceSegments();
        if (numSegs > 0) {
            stdVR_wheelState.activeWheel = STDVR_WHEEL_FORCE;
            stdVR_wheelState.numSegments = numSegs;
            stdVR_wheelState.highlightedSegment = -1;
            stdVR_wheelState.prevHighlightedSegment = -1;
            stdVR_weaponWheelTimeScale = 0.1f;
            stdVR_wheelPointerX = 0.0f;
            stdVR_wheelPointerY = 0.0f;
            // Capture rest yaw/pitch for relative pointing
            stdVR_WeaponWheel_GetControllerAngles(offhand, &stdVR_wheelInitialYaw, &stdVR_wheelInitialPitch);
            stdVR_TriggerHaptic(offhand, 0.3f, 0.1f, 100.0f);
        }
    }

    // While wheel is active, process controller pointing for selection
    if (stdVR_wheelState.activeWheel != STDVR_WHEEL_NONE) {
        int controlHand = (stdVR_wheelState.activeWheel == STDVR_WHEEL_WEAPON) ? dominantHand : offhand;
        uint32_t btnGrip = (stdVR_wheelState.activeWheel == STDVR_WHEEL_WEAPON) ? btnGripDominant : btnGripOffhand;

        // Check for grip release → confirm selection
        if (stdVR_clientInfo.buttonReleased & btnGrip) {
            if (stdVR_wheelState.highlightedSegment >= 0 && sithPlayer_pLocalPlayerThing) {
                int binIdx = stdVR_wheelState.aSegments[stdVR_wheelState.highlightedSegment].binIdx;

                if (stdVR_wheelState.activeWheel == STDVR_WHEEL_WEAPON) {
                    sithWeapon_SelectWeapon(sithPlayer_pLocalPlayerThing, binIdx, 0);
                } else {
                    sithInventory_SelectPower(sithPlayer_pLocalPlayerThing, binIdx);
                }

                stdVR_TriggerHaptic(controlHand, 0.5f, 0.15f, 150.0f);
            }

            // Reset wheel state
            stdVR_wheelState.activeWheel = STDVR_WHEEL_NONE;
            stdVR_wheelState.numSegments = 0;
            stdVR_wheelState.highlightedSegment = -1;
            stdVR_wheelState.prevHighlightedSegment = -1;
            stdVR_weaponWheelTimeScale = 1.0f;
            return;
        }

        // Use controller pointing direction for segment selection
        float angle = 0.0f, magnitude = 0.0f;
        stdVR_WeaponWheel_GetPointingAngle(controlHand, &angle, &magnitude);

        if (magnitude > STDVR_WHEEL_POINT_THRESHOLD) {
            float segmentArc = (2.0f * STDVR_PI) / stdVR_wheelState.numSegments;
            int segIdx = (int)(angle / segmentArc);
            if (segIdx >= stdVR_wheelState.numSegments) segIdx = stdVR_wheelState.numSegments - 1;
            stdVR_wheelState.highlightedSegment = segIdx;
        } else {
            stdVR_wheelState.highlightedSegment = -1;
        }

        // Haptic on segment change
        if (stdVR_wheelState.highlightedSegment != stdVR_wheelState.prevHighlightedSegment) {
            if (stdVR_wheelState.highlightedSegment >= 0) {
                stdVR_TriggerHaptic(controlHand, 0.2f, 0.05f, 80.0f);
            }
            stdVR_wheelState.prevHighlightedSegment = stdVR_wheelState.highlightedSegment;
        }
    }
}

// ============================================================================
// Public: Draw weapon wheel overlay on HUD
// Coordinates are in Window_xSize/Window_ySize space (matches Video_menuBuffer
// which std3D_DrawUI* scales from). DrawUIRenderListToCurrentFBO then rescales
// to the actual HUD FBO.
// ============================================================================
void stdVR_WeaponWheel_Draw(int hudWidth, int hudHeight)
{
    (void)hudWidth; (void)hudHeight;

    if (stdVR_wheelState.activeWheel == STDVR_WHEEL_NONE || stdVR_wheelState.numSegments == 0) {
        return;
    }

    // std3D_DrawUI* expects coordinates in Video_menuBuffer coordinate space.
    // This gets scaled to Window size, then DrawUIRenderListToCurrentFBO scales to HUD FBO.
    float coordW = (float)Video_menuBuffer.format.width;
    float coordH = (float)Video_menuBuffer.format.height;
    if (coordW < 1.0f || coordH < 1.0f) return;

    float centerX = coordW * 0.54f;
    float centerY = coordH * 0.5f;
    float radius = coordH * 0.35f;
    int bWeaponWheel = (stdVR_wheelState.activeWheel == STDVR_WHEEL_WEAPON);

    // Draw full-screen semi-transparent dark background
    // Weapon wheel: very subtle so 3D models dominate. Force wheel: solid overlay.
    {
        rdRect bgRect;
        bgRect.x = 0;
        bgRect.y = 0;
        bgRect.width = (int)coordW;
        bgRect.height = (int)coordH;
        int bgAlpha = bWeaponWheel ? 40 : 160;
        std3D_DrawUIClearedRectRGBA(0, 0, 0, bgAlpha, &bgRect);
    }

    float segmentArc = (2.0f * STDVR_PI) / stdVR_wheelState.numSegments;
    flex_t labelScale = coordH / 480.0f;

    for (int i = 0; i < stdVR_wheelState.numSegments; i++) {
        // Angle at center of this segment (0 = up, clockwise)
        float segAngle = segmentArc * i + segmentArc * 0.5f;

        // Position on circle: sin for X (right), -cos for Y (screen Y is down)
        float iconX = centerX + sinf(segAngle) * radius;
        float iconY = centerY - cosf(segAngle) * radius;

        int bHighlighted = (i == stdVR_wheelState.highlightedSegment);

        if (!bWeaponWheel) {
            // Force wheel: highlight background + icon bitmap
            if (bHighlighted) {
                int hlSize = (int)(coordH * 0.06f);
                rdRect hlRect;
                hlRect.x = (int)iconX - hlSize;
                hlRect.y = (int)iconY - hlSize;
                hlRect.width = hlSize * 2;
                hlRect.height = hlSize * 2;
                std3D_DrawUIClearedRectRGBA(50, 150, 255, 180, &hlRect);
            }

            stdBitmap* pIcon = stdVR_wheelState.aSegments[i].pIcon;
            if (pIcon && pIcon->mipSurfaces && pIcon->mipSurfaces[0]) {
                float iconScale = bHighlighted ? (coordH / 160.0f) : (coordH / 240.0f);
                uint8_t brightness = bHighlighted ? 255 : 180;
                uint8_t alpha = bHighlighted ? 255 : 220;

                int iconW = pIcon->mipSurfaces[0]->format.width;
                int iconH = pIcon->mipSurfaces[0]->format.height;
                float drawX = iconX - (iconW * iconScale * 0.5f);
                float drawY = iconY - (iconH * iconScale * 0.5f);

                std3D_DrawUIBitmapRGBA(pIcon, 0, (flex_t)drawX, (flex_t)drawY,
                                       NULL, (flex_t)iconScale, (flex_t)iconScale, 0,
                                       brightness, brightness, brightness, alpha);
            }
        }

        // Text labels
        if (jkHud_pMsgFontSft) {
            const char* name = stdVR_WeaponWheel_GetName(stdVR_wheelState.aSegments[i].binIdx);
            int textLen = (int)strlen(name);

            if (bWeaponWheel) {
                // Weapon wheel: show name under the highlighted weapon's position
                if (bHighlighted) {
                    flex_t segFontScale = labelScale * 1.3f;
                    int approxW = (int)(textLen * 7.0f * segFontScale);
                    int textX = (int)iconX - approxW / 2;
                    int textY = (int)iconY + (int)(coordH * 0.035f);
                    stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, textX, textY,
                                         (int)coordW, name, 1, segFontScale);
                }
            } else {
                // Force wheel: name at each segment position
                flex_t segFontScale = bHighlighted ? labelScale * 1.3f : labelScale;
                int approxW = (int)(textLen * 7.0f * segFontScale);
                stdBitmap* pIcon = stdVR_wheelState.aSegments[i].pIcon;
                int labelOffsetY = (pIcon && pIcon->mipSurfaces && pIcon->mipSurfaces[0])
                    ? (int)(coordH * 0.04f) : 0;
                int textX = (int)iconX - approxW / 2;
                int textY = (int)iconY + labelOffsetY - (int)(6.0f * segFontScale);
                stdFont_DrawAsciiGPU(jkHud_pMsgFontSft, textX, textY,
                                     (int)coordW, name,
                                     bHighlighted ? 1 : 0, segFontScale);
            }
        }
    }

    // Draw pointer cursor (small crosshair) where the controller is pointing
    {
        float cursorX = centerX + stdVR_wheelPointerX * radius;
        float cursorY = centerY - stdVR_wheelPointerY * radius;  // Y inverted (screen Y is down)
        int crossSize = (int)(coordH * 0.012f);
        if (crossSize < 2) crossSize = 2;
        int crossThick = (int)(coordH * 0.004f);
        if (crossThick < 1) crossThick = 1;

        // Horizontal bar of crosshair
        rdRect hBar;
        hBar.x = (int)cursorX - crossSize;
        hBar.y = (int)cursorY - crossThick;
        hBar.width = crossSize * 2;
        hBar.height = crossThick * 2;
        std3D_DrawUIClearedRectRGBA(255, 255, 255, 240, &hBar);

        // Vertical bar of crosshair
        rdRect vBar;
        vBar.x = (int)cursorX - crossThick;
        vBar.y = (int)cursorY - crossSize;
        vBar.width = crossThick * 2;
        vBar.height = crossSize * 2;
        std3D_DrawUIClearedRectRGBA(255, 255, 255, 240, &vBar);
    }
}

// ============================================================================
// Public: Check if wheel is active
// ============================================================================
int stdVR_WeaponWheel_IsActive(void)
{
    return stdVR_wheelState.activeWheel != STDVR_WHEEL_NONE;
}

// ============================================================================
// Public: Check if grip is suppressed for a given hand
// ============================================================================
int stdVR_WeaponWheel_IsGripSuppressed(int hand)
{
    if (stdVR_wheelState.activeWheel == STDVR_WHEEL_NONE) {
        return 0;
    }

    int dominantHand = stdVR_config.dominantHand;
    int offhand = 1 - dominantHand;

    if (stdVR_wheelState.activeWheel == STDVR_WHEEL_WEAPON && hand == dominantHand) {
        return 1;
    }
    if (stdVR_wheelState.activeWheel == STDVR_WHEEL_FORCE && hand == offhand) {
        return 1;
    }

    return 0;
}

// ============================================================================
// Public: Cache a weapon's POV model for 3D wheel rendering
// ============================================================================
void stdVR_WeaponWheel_CacheModel(int binIdx, rdModel3* pModel)
{
    if (binIdx < 0 || binIdx >= SITHBIN_NUMBINS || !pModel) return;

    stdVR_WheelModelCache* pCache = &stdVR_aWheelModelCache[binIdx];

    // Already cached with the same model — skip
    if (pCache->bInitialized && pCache->pModel == pModel) return;

    // Different model or not yet cached — free old, set up new
    if (pCache->bInitialized) {
        rdThing_FreeEntry(&pCache->rdThing);
        pCache->bInitialized = 0;
    }

    rdThing_NewEntry(&pCache->rdThing, sithPlayer_pLocalPlayerThing);
    if (rdThing_SetModel3(&pCache->rdThing, pModel)) {
        pCache->pModel = pModel;
        pCache->bInitialized = 1;
    }
}

// ============================================================================
// Public: Draw cached 3D weapon models aligned with the 2D HUD overlay.
// Uses inverse projection from HUD screen coordinates to world-space so that
// each 3D model lines up with its corresponding text/cursor on the HUD layer.
// ============================================================================
void stdVR_WeaponWheel_Draw3D(rdMatrix34* pCameraWorldMat)
{
    if (stdVR_wheelState.activeWheel != STDVR_WHEEL_WEAPON || stdVR_wheelState.numSegments == 0) {
        return;
    }

    // Use the VR per-eye matrix so models track with head movement.
    rdMatrix34 eyeMat;
    if (stdVR_GetCurrentEyeViewMatrix(&eyeMat)) {
        pCameraWorldMat = &eyeMat;
    }

    // Get VR frustum tangents for inverse projection.
    // These are set per-eye by sithCamera just before rendering:
    //   nearLeft = -fovLeft (negative), right = fovRight (positive)
    //   nearTop  = fovUp (positive),    bottom = -fovDown (negative)
    if (!rdCamera_pCurCamera || !rdCamera_pCurCamera->pClipFrustum) return;
    rdClipFrustum* pFrustum = rdCamera_pCurCamera->pClipFrustum;

    float left   = pFrustum->nearLeft;   // negative (left boundary tangent)
    float right  = pFrustum->right;      // positive (right boundary tangent)
    float top    = pFrustum->nearTop;    // positive (top boundary tangent)
    float bottom = pFrustum->bottom;     // negative (bottom boundary tangent)

    // Sanity check: frustum must have non-zero extent
    float hExtent = right - left;
    float vExtent = top - bottom;
    if (hExtent < 0.001f || vExtent < 0.001f) return;

    // HUD coordinate space (same as stdVR_WeaponWheel_Draw uses)
    float coordW = (float)Video_menuBuffer.format.width;
    float coordH = (float)Video_menuBuffer.format.height;
    if (coordW < 1.0f || coordH < 1.0f) return;

    float centerX = coordW * 0.5f;
    float centerY = coordH * 0.5f;
    float hudRadius = coordH * 0.11f;  // Must match Draw()

    // World scale and forward depth for model placement
    float ws = stdVR_config.worldScale;
    if (ws <= 0.001f) ws = 0.09f;
    float forwardDist = 3.0f * ws;  // 2m forward from eye in JK units

    float normalScale = 0.25f;
    float highlightScale = 0.35f;

    // Advance turntable rotation (~45 deg/sec at 60fps)
    stdVR_wheelRotation += 0.016f * 0.8f;
    if (stdVR_wheelRotation > 2.0f * STDVR_PI) {
        stdVR_wheelRotation -= 2.0f * STDVR_PI;
    }

    float segmentArc = (2.0f * STDVR_PI) / stdVR_wheelState.numSegments;

    // Camera basis vectors
    rdVector3 camRight, camUp, camFwd, camPos;
    rdVector_Copy3(&camRight, &pCameraWorldMat->rvec);
    rdVector_Copy3(&camUp, &pCameraWorldMat->uvec);
    rdVector_Copy3(&camFwd, &pCameraWorldMat->lvec);
    rdVector_Copy3(&camPos, &pCameraWorldMat->scale);

    // Disable software backface culling (model space vs view space mismatch)
    extern int rdGetRenderOptions(void);
    extern void rdSetRenderOptions(int options);
    int savedRenderOptions = rdGetRenderOptions();
    rdSetRenderOptions(savedRenderOptions & ~1);

    for (int i = 0; i < stdVR_wheelState.numSegments; i++) {
        int binIdx = stdVR_wheelState.aSegments[i].binIdx;
        if (binIdx < 0 || binIdx >= SITHBIN_NUMBINS) continue;

        stdVR_WheelModelCache* pCache = &stdVR_aWheelModelCache[binIdx];
        if (!pCache->bInitialized) continue;

        int bHighlighted = (i == stdVR_wheelState.highlightedSegment);

        // Compute HUD position (same formula as Draw for exact alignment)
        float segAngle = segmentArc * i + segmentArc * 0.5f;
        float iconX = centerX + sinf(segAngle) * hudRadius;
        float iconY = centerY - cosf(segAngle) * hudRadius;

        // Normalize HUD position to 0..1 (proportional screen coords)
        float nx = iconX / coordW;
        float ny = iconY / coordH;

        // Inverse projection: screen coords → view-space at forward depth.
        // Derived from rdCamera_PerspProjectVR forward equations:
        //   screen_x = offsetX + vx * (2*half_w / (right-left)) / vy
        //   screen_y = offsetY - vz * (2*half_h / (top-bottom)) / vy
        // Solving for vx, vz at normalized coords:
        float vy = forwardDist;
        float vx = vy * (nx * hExtent + left);
        float vz = vy * (top - ny * vExtent);

        // Transform view-space to world-space
        // JK view convention: x=right(rvec), y=forward(lvec), z=up(uvec)
        rdVector3 worldPos;
        worldPos.x = camPos.x + camRight.x * vx + camFwd.x * vy + camUp.x * vz;
        worldPos.y = camPos.y + camRight.y * vx + camFwd.y * vy + camUp.y * vz;
        worldPos.z = camPos.z + camRight.z * vx + camFwd.z * vy + camUp.z * vz;

        float modelScale = bHighlighted ? highlightScale : normalScale;

        // Build model matrix: vertical display pose with turntable spin.
        // POV models have barrel along Y (lvec) and weapon body in -Z (uvec).
        // Remap axes so barrel points UP and weapon face points toward viewer:
        //   model X (rvec) = spin right    (horizontal, rotating)
        //   model Y (lvec) = camera up     (barrel points upward)
        //   model Z (uvec) = -spin forward (weapon top faces viewer)
        // Handedness check: rvec × lvec = rotRight × camUp = rotFwd = -uvec ✓
        float sinSpin = sinf(stdVR_wheelRotation);
        float cosSpin = cosf(stdVR_wheelRotation);

        rdVector3 rotRight, rotFwd;
        rotRight.x = camRight.x * cosSpin + camFwd.x * sinSpin;
        rotRight.y = camRight.y * cosSpin + camFwd.y * sinSpin;
        rotRight.z = camRight.z * cosSpin + camFwd.z * sinSpin;
        rotFwd.x = -camRight.x * sinSpin + camFwd.x * cosSpin;
        rotFwd.y = -camRight.y * sinSpin + camFwd.y * cosSpin;
        rotFwd.z = -camRight.z * sinSpin + camFwd.z * cosSpin;

        rdMatrix34 modelMat;
        modelMat.rvec.x = rotRight.x * modelScale;
        modelMat.rvec.y = rotRight.y * modelScale;
        modelMat.rvec.z = rotRight.z * modelScale;
        modelMat.lvec.x = camUp.x * modelScale;
        modelMat.lvec.y = camUp.y * modelScale;
        modelMat.lvec.z = camUp.z * modelScale;
        modelMat.uvec.x = -rotFwd.x * modelScale;
        modelMat.uvec.y = -rotFwd.y * modelScale;
        modelMat.uvec.z = -rotFwd.z * modelScale;
        rdVector_Copy3(&modelMat.scale, &worldPos);

        // Force hierarchy matrix rebuild per eye
        pCache->rdThing.frameTrue = 0;
        rdThing_Draw(&pCache->rdThing, &modelMat);
        rdCache_Flush();
    }

    rdSetRenderOptions(savedRenderOptions);
}

// ============================================================================
// Public: Free all cached model entries
// ============================================================================
void stdVR_WeaponWheel_ResetCache(void)
{
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        if (stdVR_aWheelModelCache[i].bInitialized) {
            rdThing_FreeEntry(&stdVR_aWheelModelCache[i].rdThing);
        }
    }
    _memset(stdVR_aWheelModelCache, 0, sizeof(stdVR_aWheelModelCache));
    stdVR_wheelRotation = 0.0f;
}

#endif // PLATFORM_VR
