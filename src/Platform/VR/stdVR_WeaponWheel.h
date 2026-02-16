#ifndef STDVR_WEAPONWHEEL_H
#define STDVR_WEAPONWHEEL_H

// Added: VR weapon wheel and force power wheel overlay

#include "types.h"

#ifdef PLATFORM_VR

#ifdef __cplusplus
extern "C" {
#endif

#define STDVR_WHEEL_NONE    0
#define STDVR_WHEEL_WEAPON  1
#define STDVR_WHEEL_FORCE   2

#define STDVR_WHEEL_MAX_SEGMENTS 14

// Wheel segment (one selectable item)
typedef struct stdVR_WheelSegment {
    int binIdx;             // Inventory bin index (SITHBIN_*)
    stdBitmap* pIcon;       // HUD icon bitmap
} stdVR_WheelSegment;

// Wheel state
typedef struct stdVR_WheelState {
    int activeWheel;                                // STDVR_WHEEL_NONE/WEAPON/FORCE
    int numSegments;                                // Number of populated segments
    stdVR_WheelSegment aSegments[STDVR_WHEEL_MAX_SEGMENTS];
    int highlightedSegment;                         // Currently pointed-at segment (-1 = none)
    int prevHighlightedSegment;                     // For haptic-on-change
} stdVR_WheelState;

// Update wheel state (call each frame from stdVR_Input_MapToGame)
void stdVR_WeaponWheel_Update(void);

// Draw wheel overlay on HUD (call from HUD render path)
void stdVR_WeaponWheel_Draw(int hudWidth, int hudHeight);

// Check if any wheel is currently active
int stdVR_WeaponWheel_IsActive(void);

// Check if grip is captured by wheel for a given hand (0=left, 1=right)
int stdVR_WeaponWheel_IsGripSuppressed(int hand);

#ifdef __cplusplus
}
#endif

#else // !PLATFORM_VR

static inline void stdVR_WeaponWheel_Update(void) {}
static inline void stdVR_WeaponWheel_Draw(int hudWidth, int hudHeight) { (void)hudWidth; (void)hudHeight; }
static inline int stdVR_WeaponWheel_IsActive(void) { return 0; }
static inline int stdVR_WeaponWheel_IsGripSuppressed(int hand) { (void)hand; return 0; }

#endif // PLATFORM_VR

#endif // STDVR_WEAPONWHEEL_H
