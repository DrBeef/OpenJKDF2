#ifndef _STDVR_ALIGNMENT_TOOL_H
#define _STDVR_ALIGNMENT_TOOL_H

// Added: VR weapon alignment tool for real-time offset adjustment

#include "engine_config.h"

#ifdef VR_WEAPON_ALIGNMENT_TOOL

#ifdef __cplusplus
extern "C" {
#endif

// Adjustment modes for the alignment tool
typedef enum stdVR_AlignmentMode {
    STDVR_ALIGN_POSITION = 0,   // Adjust X/Y/Z position offsets
    STDVR_ALIGN_SCALE = 1,      // Adjust model scale
    STDVR_ALIGN_PITCH = 2,      // Adjust pitch rotation
    STDVR_ALIGN_MODE_COUNT = 3
} stdVR_AlignmentMode;

// Initialize the alignment tool (call from stdVR_Startup)
void stdVR_AlignmentTool_Startup(void);

// Update the alignment tool each frame (call from main game tick)
// deltaSeconds: time since last frame
void stdVR_AlignmentTool_Update(float deltaSeconds);

// Draw the alignment tool HUD overlay (call from HUD rendering)
void stdVR_AlignmentTool_DrawOverlay(void);

// Toggle the alignment tool on/off
void stdVR_AlignmentTool_Toggle(void);

// Check if the alignment tool is currently active
int stdVR_AlignmentTool_IsActive(void);

// Get the current adjustment mode
stdVR_AlignmentMode stdVR_AlignmentTool_GetMode(void);

#ifdef __cplusplus
}
#endif

#endif // VR_WEAPON_ALIGNMENT_TOOL

#endif // _STDVR_ALIGNMENT_TOOL_H
