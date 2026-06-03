#include "stdVR_WeaponOffsets.h"

// Added: Per-weapon VR offset system implementation

#ifdef PLATFORM_VR

#include "Platform/VR/stdVR.h"
#include "General/stdJSON.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "stdPlatform.h"
#include "engine_config.h"

#include <string.h>
#include <stdio.h>

// Global array of per-weapon offsets
stdVR_WeaponOffset stdVR_aWeaponOffsets[STDVR_MAX_WEAPON_BINS];

static int stdVR_weaponOffsets_bInitted = 0;

// Filename for weapon offsets (separate from cvars to avoid limit)
#ifdef ARCH_WASM
#define STDVR_WEAPON_OFFSETS_FNAME "persist/jkdf2xr_vr_weapons.json"
#else
#define STDVR_WEAPON_OFFSETS_FNAME "jkdf2xr_vr_weapons.json"
#endif

// Initialize all offsets with defaults from stdVR_motionConfig
static void stdVR_WeaponOffsets_InitDefaults(void)
{
    for (int i = 0; i < STDVR_MAX_WEAPON_BINS; i++) {
        stdVR_aWeaponOffsets[i].offsetX = stdVR_motionConfig.weaponOffsetX;
        stdVR_aWeaponOffsets[i].offsetY = stdVR_motionConfig.weaponOffsetY;
        stdVR_aWeaponOffsets[i].offsetZ = stdVR_motionConfig.weaponOffsetZ;
        stdVR_aWeaponOffsets[i].modelScale = stdVR_motionConfig.weaponModelScale;
        stdVR_aWeaponOffsets[i].pitchAdjust = stdVR_motionConfig.weaponPitchAdjust;
        stdVR_aWeaponOffsets[i].bConfigured = 0;
    }
}

// Load weapon offsets from JSON file
static void stdVR_WeaponOffsets_Load(void)
{
    char keyName[64];

    for (int i = 0; i < STDVR_MAX_WEAPON_BINS; i++) {
        stdVR_WeaponOffset* pOffset = &stdVR_aWeaponOffsets[i];

        // Check if this weapon has saved data
        snprintf(keyName, sizeof(keyName), "weapon_%d_configured", i);
        int configured = stdJSON_GetInt(STDVR_WEAPON_OFFSETS_FNAME, keyName, 0);

        if (configured) {
            snprintf(keyName, sizeof(keyName), "weapon_%d_offset_x", i);
            pOffset->offsetX = stdJSON_GetFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->offsetX);

            snprintf(keyName, sizeof(keyName), "weapon_%d_offset_y", i);
            pOffset->offsetY = stdJSON_GetFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->offsetY);

            snprintf(keyName, sizeof(keyName), "weapon_%d_offset_z", i);
            pOffset->offsetZ = stdJSON_GetFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->offsetZ);

            snprintf(keyName, sizeof(keyName), "weapon_%d_scale", i);
            pOffset->modelScale = stdJSON_GetFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->modelScale);

            snprintf(keyName, sizeof(keyName), "weapon_%d_pitch", i);
            pOffset->pitchAdjust = stdJSON_GetFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->pitchAdjust);

            pOffset->bConfigured = 1;

            stdPlatform_Printf("stdVR_WeaponOffsets: Loaded offsets for weapon %d\n", i);
        }
    }
}

void stdVR_WeaponOffsets_Startup(void)
{
    if (stdVR_weaponOffsets_bInitted) {
        return;
    }

    stdPlatform_Printf("stdVR_WeaponOffsets: Starting up weapon offsets system...\n");

    // Initialize all offsets with defaults
    stdVR_WeaponOffsets_InitDefaults();

    // Load any saved offsets from file
    stdVR_WeaponOffsets_Load();

    stdVR_weaponOffsets_bInitted = 1;
    stdPlatform_Printf("stdVR_WeaponOffsets: Weapon offsets system ready\n");
}

void stdVR_WeaponOffsets_Shutdown(void)
{
    if (!stdVR_weaponOffsets_bInitted) {
        return;
    }

    stdPlatform_Printf("stdVR_WeaponOffsets: Shutting down weapon offsets system...\n");
    stdVR_weaponOffsets_bInitted = 0;
}

stdVR_WeaponOffset* stdVR_GetWeaponOffset(int binIdx)
{
    if (binIdx < 0 || binIdx >= STDVR_MAX_WEAPON_BINS) {
        return NULL;
    }

    stdVR_WeaponOffset* pOffset = &stdVR_aWeaponOffsets[binIdx];

    // If this weapon hasn't been configured, return the offset anyway
    // (it will have default values from stdVR_motionConfig)
    return pOffset;
}

int stdVR_GetCurrentWeaponBin(void)
{
    if (!sithPlayer_pLocalPlayerThing) {
        return -1;
    }

    return sithInventory_GetCurWeapon(sithPlayer_pLocalPlayerThing);
}

stdVR_WeaponOffset* stdVR_GetCurrentWeaponOffset(void)
{
    int binIdx = stdVR_GetCurrentWeaponBin();
    if (binIdx < 0) {
        return NULL;
    }

    return stdVR_GetWeaponOffset(binIdx);
}

void stdVR_WeaponOffsets_Save(void)
{
    char keyName[64];

    if (!stdVR_weaponOffsets_bInitted) {
        return;
    }

    stdPlatform_Printf("stdVR_WeaponOffsets: Saving weapon offsets...\n");

    // Only save weapons that have been configured
    for (int i = 0; i < STDVR_MAX_WEAPON_BINS; i++) {
        stdVR_WeaponOffset* pOffset = &stdVR_aWeaponOffsets[i];

        if (!pOffset->bConfigured) {
            continue;
        }

        snprintf(keyName, sizeof(keyName), "weapon_%d_configured", i);
        stdJSON_SaveInt(STDVR_WEAPON_OFFSETS_FNAME, keyName, 1);

        snprintf(keyName, sizeof(keyName), "weapon_%d_offset_x", i);
        stdJSON_SaveFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->offsetX);

        snprintf(keyName, sizeof(keyName), "weapon_%d_offset_y", i);
        stdJSON_SaveFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->offsetY);

        snprintf(keyName, sizeof(keyName), "weapon_%d_offset_z", i);
        stdJSON_SaveFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->offsetZ);

        snprintf(keyName, sizeof(keyName), "weapon_%d_scale", i);
        stdJSON_SaveFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->modelScale);

        snprintf(keyName, sizeof(keyName), "weapon_%d_pitch", i);
        stdJSON_SaveFloat(STDVR_WEAPON_OFFSETS_FNAME, keyName, pOffset->pitchAdjust);

        stdPlatform_Printf("stdVR_WeaponOffsets: Saved offsets for weapon %d\n", i);
    }

    stdPlatform_Printf("stdVR_WeaponOffsets: Save complete\n");
}

#endif // PLATFORM_VR
