#ifndef STDVR_WEAPONWHEEL_H
#define STDVR_WEAPONWHEEL_H

// Added: VR weapon wheel and force power wheel overlay

#include "types.h"
#include "Primitives/rdModel3.h"
#include "Primitives/rdMatrix.h"

#ifdef PLATFORM_VR

#ifdef __cplusplus
extern "C" {
#endif

#define STDVR_WHEEL_NONE    0
#define STDVR_WHEEL_WEAPON  1
#define STDVR_WHEEL_FORCE   2
// Added: the off-hand wheel has two pages - Force powers and inventory items - flipped with
// the off-hand thumbstick while the grip is held. Selecting an item USES it immediately.
#define STDVR_WHEEL_ITEMS   3

// 14 covers DF2 (10 weapons / 14 force powers). MOTS reaches 17 of each, so the cap would
// silently truncate the wheel there.
#define STDVR_WHEEL_MAX_SEGMENTS 18

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
// Which wheel is up: STDVR_WHEEL_NONE / _WEAPON / _FORCE
int stdVR_WeaponWheel_GetActiveWheel(void);

// Check if grip is captured by wheel for a given hand (0=left, 1=right)
int stdVR_WeaponWheel_IsGripSuppressed(int hand);

// Cache a weapon's POV model for 3D wheel rendering
void stdVR_WeaponWheel_CacheModel(int binIdx, rdModel3* pModel);

// Draw cached 3D weapon models in camera space (call from jkPlayer_DrawPov)
void stdVR_WeaponWheel_Draw3D(rdMatrix34* pCameraWorldMat);

// Free all cached model entries (call on level unload)
void stdVR_WeaponWheel_ResetCache(void);

#ifdef __cplusplus
}
#endif

#else // !PLATFORM_VR

static inline void stdVR_WeaponWheel_Update(void) {}
static inline void stdVR_WeaponWheel_Draw(int hudWidth, int hudHeight) { (void)hudWidth; (void)hudHeight; }
static inline int stdVR_WeaponWheel_IsActive(void) { return 0; }
static inline int stdVR_WeaponWheel_GetActiveWheel(void) { return 0; }
static inline int stdVR_WeaponWheel_IsGripSuppressed(int hand) { (void)hand; return 0; }
static inline void stdVR_WeaponWheel_CacheModel(int binIdx, rdModel3* pModel) { (void)binIdx; (void)pModel; }
static inline void stdVR_WeaponWheel_Draw3D(rdMatrix34* pCameraWorldMat) { (void)pCameraWorldMat; }
static inline void stdVR_WeaponWheel_ResetCache(void) {}

#endif // PLATFORM_VR

#endif // STDVR_WEAPONWHEEL_H
