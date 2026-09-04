#ifndef _STDVR_WEAPON_OFFSETS_H
#define _STDVR_WEAPON_OFFSETS_H

// Added: Per-weapon VR offset configuration

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PLATFORM_VR

// Per-weapon VR offset configuration
typedef struct stdVR_WeaponOffset {
    float offsetX;          // Left/right offset (meters)
    float offsetY;          // Forward/back offset (meters)
    float offsetZ;          // Up/down offset (meters)
    float modelScale;       // Scale multiplier (default 1.0)
    float pitchAdjust;      // Pitch rotation (degrees)
    float yawAdjust;        // Yaw rotation (degrees)
    float rollAdjust;       // Roll rotation (degrees)
    int bConfigured;        // Has this weapon been manually configured?
} stdVR_WeaponOffset;

#define STDVR_MAX_WEAPON_BINS (200)  // Match SITHBIN_NUMBINS

extern stdVR_WeaponOffset stdVR_aWeaponOffsets[STDVR_MAX_WEAPON_BINS];

// Initialize the weapon offsets system and register cvars
void stdVR_WeaponOffsets_Startup(void);

// Shutdown the weapon offsets system
void stdVR_WeaponOffsets_Shutdown(void);

// Get offset for a specific weapon bin index
// Returns pointer to offset struct, or NULL if binIdx is out of range
stdVR_WeaponOffset* stdVR_GetWeaponOffset(int binIdx);

// Get offset for the current player's weapon
// Returns pointer to offset struct, or NULL if no player/weapon
stdVR_WeaponOffset* stdVR_GetCurrentWeaponOffset(void);

// Save all configured weapon offsets to cvars (triggers cvar save)
void stdVR_WeaponOffsets_Save(void);

// Get the current weapon bin index for the local player
// Returns -1 if no player or no weapon
int stdVR_GetCurrentWeaponBin(void);

#else // !PLATFORM_VR

// Stub implementations when VR is not compiled in
static inline void stdVR_WeaponOffsets_Startup(void) {}
static inline void stdVR_WeaponOffsets_Shutdown(void) {}
static inline void* stdVR_GetWeaponOffset(int binIdx) { (void)binIdx; return 0; }
static inline void* stdVR_GetCurrentWeaponOffset(void) { return 0; }
static inline void stdVR_WeaponOffsets_Save(void) {}
static inline int stdVR_GetCurrentWeaponBin(void) { return -1; }

#endif // PLATFORM_VR

#ifdef __cplusplus
}
#endif

#endif // _STDVR_WEAPON_OFFSETS_H
