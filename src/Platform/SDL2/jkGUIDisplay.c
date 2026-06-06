#include "Gui/jkGUIDisplay.h"

#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "General/stdString.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUISetup.h"
#include "World/jkPlayer.h"
#include "Win95/Window.h"
#include "Platform/std3D.h"
#ifdef PLATFORM_VR
#include "Platform/VR/stdVR.h"
#endif

#include "jk.h"

enum jkGuiDecisionButton_t
{
    GUI_GENERAL = 100,
    GUI_GAMEPLAY = 101,
    GUI_DISPLAY = 102,
    GUI_SOUND = 103,
    GUI_CONTROLS = 104,

    GUI_ADVANCED = 105,
};

static int slider_images[2] = {JKGUI_BM_SLIDER_BACK, JKGUI_BM_SLIDER_THUMB};

#ifdef PLATFORM_VR
// ============================================================================
// VR Options menu (replaces Display settings in VR builds)
// ============================================================================
static wchar_t vr_vignette_text[16] = {0};
static wchar_t vr_snap_angle_text[8] = {0};
static wchar_t vr_smooth_speed_text[8] = {0};
static wchar_t vr_height_text[16] = {0};
static wchar_t vr_ss_text[256] = {0};
static wchar_t vr_pitch_text[16] = {0};
// HUD layout tuning sliders: per-field label + value text, plus slider config and the
// jkPlayer globals each slider targets. base/inc/n map slider step <-> value consistently
// across the draw callback, the populate-on-show, and the save-on-OK.
static wchar_t vr_hud_val[5][16] = {0};
static const struct { float base, inc; int n; const wchar_t* label; } s_hudCfg[5] = {
    { 0.05f, 0.02f,  73, L"Width"  },   // 0.05 .. 1.49  (half-width  NDC)
    { 0.05f, 0.02f,  73, L"Height" },   // 0.05 .. 1.49  (half-height NDC)
    {-1.00f, 0.02f, 101, L"X"      },   // -1.00 .. 1.00 (centre NDC, + = right)
    {-1.30f, 0.02f, 116, L"Y"      },   // -1.30 .. 1.00 (centre NDC, - = lower)
    { 0.00f, 0.05f,  61, L"Dist"   },   // 0.00 .. 3.00 m (0 = infinity / no depth)
};
static float* const s_hudGlobals[5] = {
    &jkPlayer_vrHudWidth, &jkPlayer_vrHudHeight, &jkPlayer_vrHudPosX, &jkPlayer_vrHudPosY, &jkPlayer_vrHudDepth
};

void jkGuiDisplay_VRVignetteDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_VRSnapAngleDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_VRSmoothSpeedDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_VRHeightDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_VRSupersampleDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_VRPitchDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_VRHudSliderDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);

// Element indices for VR Options menu
enum {
    VR_EL_HINT = 0, VR_EL_TITLE, VR_EL_TAB_GENERAL, VR_EL_TAB_GAMEPLAY,
    VR_EL_TAB_VROPTIONS, VR_EL_TAB_SOUND, VR_EL_TAB_CONTROLS,
    VR_EL_OK, VR_EL_CANCEL,
    VR_EL_DOMINANT_HAND,     // 9
    VR_EL_WEAPON_CROSSHAIR,  // 10
    VR_EL_MOVE_DIRECTION,    // 11
    VR_EL_SNAP_TURN,         // 12
    VR_EL_VIGNETTE_LABEL,    // 12
    VR_EL_VIGNETTE_SLIDER,   // 13
    VR_EL_VIGNETTE_VAL,      // 14
    VR_EL_SNAP_ANGLE_LABEL,  // 15
    VR_EL_SNAP_ANGLE_SLIDER, // 15
    VR_EL_SNAP_ANGLE_VAL,    // 16
    VR_EL_SMOOTH_SPEED_LABEL,// 17
    VR_EL_SMOOTH_SPEED_SLIDER,// 18
    VR_EL_SMOOTH_SPEED_VAL,  // 19
    VR_EL_HEIGHT_LABEL,      // 20
    VR_EL_HEIGHT_SLIDER,     // 21
    VR_EL_HEIGHT_VAL,        // 22
    VR_EL_SS_LABEL,          // Supersampling: label
    VR_EL_SS_VAL,            // Supersampling: inline value text
    VR_EL_SS_SLIDER,         // Supersampling: slider (0.80 .. 1.25)
    VR_EL_PITCH_LABEL,       // Weapon Pitch: label
    VR_EL_PITCH_SLIDER,      // Weapon Pitch: slider (-25 .. +25 deg)
    VR_EL_PITCH_VAL,         // Weapon Pitch: value text
    VR_EL_HUD_LAYOUT_BTN,    // opens the HUD Layout sub-page
    VR_EL_END,
};

static jkGuiElement jkGuiDisplay_aElements[VR_EL_END + 1] = {
    { ELEMENT_TEXT,        0,            0, NULL,                        3, {0, 410, 640, 20},   1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,        0,            6, "GUI_SETUP",                 3, {20, 20, 600, 40},   1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_GENERAL,  2, "GUI_GENERAL",              3, {20, 80, 120, 40},   1, 0, "GUI_GENERAL_HINT",                0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_GAMEPLAY, 2, "GUI_GAMEPLAY",             3, {140, 80, 120, 40},  1, 0, "GUI_GAMEPLAY_HINT",               0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_DISPLAY,  2, "GUIEXT_VR_OPTIONS",        3, {260, 80, 120, 40},  1, 0, "GUIEXT_VR_OPTIONS_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_SOUND,    2, "GUI_SOUND",                3, {380, 80, 120, 40},  1, 0, "GUI_SOUND_HINT",                  0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_CONTROLS, 2, "GUI_CONTROLS",             3, {500, 80, 120, 40},  1, 0, "GUI_CONTROLS_HINT",               0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  1,            2, "GUI_OK",                   3, {440, 430, 200, 40}, 1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON, -1,            2, "GUI_CANCEL",               3, {0, 430, 200, 40},   1, 0, NULL,                              0, 0, 0, {0}, 0},

    // Left column: checkboxes (y=140..230)
    { ELEMENT_CHECKBOX,    0,            0, "GUIEXT_VR_DOMINANT_HAND",   0, {30, 140, 270, 20},  1, 0, "GUIEXT_VR_DOMINANT_HAND_HINT",    0, 0, 0, {0}, 0},
    { ELEMENT_CHECKBOX,    0,            0, "GUIEXT_VR_WEAPON_CROSSHAIR",0, {30, 170, 270, 20},  1, 0, "GUIEXT_VR_WEAPON_CROSSHAIR_HINT", 0, 0, 0, {0}, 0},
    { ELEMENT_CHECKBOX,    0,            0, "GUIEXT_VR_MOVE_DIRECTION",  0, {30, 200, 270, 20},  1, 0, "GUIEXT_VR_MOVE_DIRECTION_HINT",   0, 0, 0, {0}, 0},
    { ELEMENT_CHECKBOX,    0,            0, "GUIEXT_VR_SNAP_TURN",       0, {30, 230, 270, 20},  1, 0, "GUIEXT_VR_SNAP_TURN_HINT",        0, 0, 0, {0}, 0},
    // Left column: comfort vignette slider (moved down to y=262 so the label clears the
    // Snap Turn checkbox at y=230; it was previously clipping into it)
    { ELEMENT_TEXT,        0,            0, "GUIEXT_VR_COMFORT_VIGNETTE",0, {30, 262, 270, 20},  1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER,      0,            0, (const char*)10,             0, {30, 282, 270, 30},  1, 0, "GUIEXT_VR_COMFORT_VIGNETTE_HINT", jkGuiDisplay_VRVignetteDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,        0,            0, vr_vignette_text,            3, {30, 312, 270, 20},  1, 0, NULL,                              0, 0, 0, {0}, 0},
    // Right column: sliders (x=330..620)
    { ELEMENT_TEXT,        0,            0, "GUIEXT_VR_SNAP_ANGLE",      0, {330, 130, 280, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER,      0,            0, (const char*)2,              0, {330, 150, 280, 30}, 1, 0, "GUIEXT_VR_SNAP_ANGLE_HINT",       jkGuiDisplay_VRSnapAngleDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,        0,            0, vr_snap_angle_text,          3, {330, 180, 280, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},

    { ELEMENT_TEXT,        0,            0, "GUIEXT_VR_SMOOTH_SPEED",    0, {330, 210, 280, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER,      0,            0, (const char*)280,            0, {330, 230, 280, 30}, 1, 0, "GUIEXT_VR_SMOOTH_SPEED_HINT",     jkGuiDisplay_VRSmoothSpeedDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,        0,            0, vr_smooth_speed_text,        3, {330, 260, 280, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},

    { ELEMENT_TEXT,        0,            0, "GUIEXT_VR_HEIGHT_OFFSET",   0, {330, 290, 280, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER,      0,            0, (const char*)100,            0, {330, 310, 280, 30}, 1, 0, "GUIEXT_VR_HEIGHT_OFFSET_HINT",    jkGuiDisplay_VRHeightDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,        0,            0, vr_height_text,              3, {330, 340, 280, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},

    // Right column: supersampling slider (0.80 .. 1.25) with inline value text
    { ELEMENT_TEXT,        0,            0, "GUIEXT_VR_SUPERSAMPLING",   2, {330, 366, 180, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,        0,            0, vr_ss_text,                  2, {510, 366, 100, 20}, 1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER,      0,            0, (const char*)45,             0, {330, 386, 280, 30}, 1, 0, "GUIEXT_VR_SUPERSAMPLING_HINT",    jkGuiDisplay_VRSupersampleDraw, 0, slider_images, {0}, 0},

    // Left column: weapon pitch adjust slider (-25 .. +25 degrees). Label set in Startup.
    { ELEMENT_TEXT,        0,            0, NULL,                        0, {30, 340, 270, 20},  1, 0, NULL,                              0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER,      0,            0, (const char*)50,             0, {30, 360, 270, 30},  1, 0, NULL,                              jkGuiDisplay_VRPitchDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,        0,            0, vr_pitch_text,               3, {30, 390, 270, 20},  1, 0, NULL,                              0, 0, 0, {0}, 0},

    // Button to open the dedicated HUD Layout sub-page (full-width sliders need their own page).
    // Placed on the bottom button row (between Cancel and OK) to free the left column for sliders.
    { ELEMENT_TEXTBUTTON, 500, 2, NULL,      3, { 220, 430, 200, 40}, 1, 0, NULL, 0, 0, 0, {0}, 0},

    { ELEMENT_END,         0,            0, NULL,                        0, {0},                 0, 0, NULL,                              0, 0, 0, {0}, 0},
};

#else // !PLATFORM_VR — Desktop Display settings

static wchar_t render_level[256] = {0};
static wchar_t gamma_level[256] = {0};
static wchar_t hud_level[256] = {0};
static wchar_t vr_render_level[256] = {0};

static wchar_t slider_val_text[5] = {0};
static wchar_t slider_val_text_2[5] = {0};

void jkGuiDisplay_FovDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);
void jkGuiDisplay_FramelimitDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw);

static jkGuiElement jkGuiDisplay_aElements[31] = {
    { ELEMENT_TEXT,        0,            0, NULL,                   3, {0, 410, 640, 20},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,        0,            6, "GUI_SETUP",            3, {20, 20, 600, 40},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_GENERAL,  2, "GUI_GENERAL",          3, {20, 80, 120, 40},   1, 0, "GUI_GENERAL_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_GAMEPLAY, 2, "GUI_GAMEPLAY",         3, {140, 80, 120, 40},  1, 0, "GUI_GAMEPLAY_HINT",         0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_DISPLAY,  2, "GUI_DISPLAY",          3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_SOUND,    2, "GUI_SOUND",            3, {380, 80, 120, 40},  1, 0, "GUI_SOUND_HINT",            0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_CONTROLS, 2, "GUI_CONTROLS",         3, {500, 80, 120, 40},  1, 0, "GUI_CONTROLS_HINT",         0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  1,            2, "GUI_OK",               3, {440, 430, 200, 40}, 1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON, -1,            2, "GUI_CANCEL",           3, {0, 430, 200, 40},   1, 0, NULL,                        0, 0, 0, {0}, 0},

    // 9
    {ELEMENT_TEXT,         0,            0, "GUIEXT_FOV",                 3, {20, 130, 300, 30}, 1,  0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_SLIDER,       0,            0, (const char*)(FOV_MAX - FOV_MIN),                    0, {10, 160, 320, 30}, 1, 0, "GUIEXT_FOV_HINT", jkGuiDisplay_FovDraw, 0, slider_images, {0}, 0},
    {ELEMENT_TEXT,         0,            0, slider_val_text,        3, {20, 190, 300, 30}, 1,  0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_FOV_VERTICAL",    0, {20, 210, 200, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_FULLSCREEN",    0, {400, 150, 200, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_HIDPI",    0, {400, 180, 200, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_TEXTURE_FILTERING",    0, {400, 210, 200, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_SQUARE_ASPECT",    0, {20, 240, 300, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},

    // 17
    {ELEMENT_TEXT,         0,            0, "GUIEXT_FPS_LIMIT",                 3, {20, 280, 300, 30}, 1,  0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_SLIDER,       0,            0, (const char*)(FPS_LIMIT_MAX - FPS_LIMIT_MIN),                    0, {10, 310, 320, 30}, 1, 0, "GUIEXT_FPS_LIMIT_HINT", jkGuiDisplay_FramelimitDraw, 0, slider_images, {0}, 0},
    {ELEMENT_TEXT,         0,            0, slider_val_text_2,        3, {20, 340, 300, 30}, 1,  0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_VSYNC",    0, {20, 360, 300, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    
    // 21
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_BLOOM",    0, {400, 240, 300, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},

    // 22
    {ELEMENT_CHECKBOX,     0,            0, "GUIEXT_EN_SSAO",    0, {400, 270, 300, 40}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    
    // 23
    { ELEMENT_TEXT,        0,            0, "GUIEXT_SSAA_MULT",            2, {400, 320, 120, 20},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBOX,      0,            0, NULL,    100, {530, 320, 80, 20}, 1,  0, NULL, 0, 0, 0, {0}, 0},
    
    // 25
    { ELEMENT_TEXT,        0,            0, "GUIEXT_GAMMA_VAL",            2, {400, 350, 120, 20},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBOX,      0,            0, NULL,    100, {530, 350, 80, 20}, 1,  0, NULL, 0, 0, 0, {0}, 0},

    // 27
    { ELEMENT_TEXT,        0,            0, "GUIEXT_HUD_SCALE",            2, {400, 380, 120, 20},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBOX,      0,            0, NULL,    100, {530, 380, 80, 20}, 1,  0, NULL, 0, 0, 0, {0}, 0},

    { ELEMENT_TEXTBUTTON,  GUI_ADVANCED, 2, "GUI_ADVANCED",               3, {220, 430, 200, 40}, 1, 0, NULL,                        0, 0, 0, {0}, 0},

    { ELEMENT_END,         0,            0, NULL,                   0, {0},                 0, 0, NULL,                        0, 0, 0, {0}, 0},
};

#endif // !PLATFORM_VR

static jkGuiMenu jkGuiDisplay_menu = { jkGuiDisplay_aElements, 0, 0xFF, 0xE1, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };

// ----------------------------------------------------------------------------
// Dedicated "HUD Layout" sub-page: full-width sliders (270px ~= the slider bitmap, so the
// whole range is reachable) with generous vertical spacing. label + slider + value per row.
// ----------------------------------------------------------------------------
enum {
    VRHUD_TITLE = 0, VRHUD_OK, VRHUD_CANCEL,
    VRHUD_PAD0, VRHUD_PAD1, VRHUD_PAD2, VRHUD_PAD3,   // fill the menu's reserved tab zone (idx 2-6)
    VRHUD_W_LABEL, VRHUD_W_SLIDER, VRHUD_W_VAL,       // sliders MUST be at index >= 7 (outside tab zone)
    VRHUD_H_LABEL, VRHUD_H_SLIDER, VRHUD_H_VAL,
    VRHUD_X_LABEL, VRHUD_X_SLIDER, VRHUD_X_VAL,
    VRHUD_Y_LABEL, VRHUD_Y_SLIDER, VRHUD_Y_VAL,
    VRHUD_D_LABEL, VRHUD_D_SLIDER, VRHUD_D_VAL,
    VRHUD_END
};
static wchar_t vrhud_title_text[32] = {0};
static jkGuiElement jkGuiDisplay_aElementsVRHud[VRHUD_END + 1] = {
    { ELEMENT_TEXT,       0, 6, NULL,            3, { 20, 40, 600, 40}, 1, 0, NULL, 0, 0, 0, {0}, 0}, // title
    { ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK",        3, {440,430, 200, 40}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,-1, 2, "GUI_CANCEL",    3, {  0,430, 200, 40}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    // Invisible placeholders occupying the menu's reserved tab-zone indices (2-6); the menu/tab
    // navigation treats those indices specially, which broke a slider placed there (it buzzed
    // but wouldn't drag). Keeping the real sliders at index >= 7 avoids that.
    { ELEMENT_TEXT,   0, 0, NULL,            3, {0, 0, 0, 0}, 0, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {0, 0, 0, 0}, 0, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {0, 0, 0, 0}, 0, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {0, 0, 0, 0}, 0, 0, NULL, 0, 0, 0, {0}, 0},
    // Row pitch 64px with 28px-tall sliders so each slider's rect clears the +/-32px hit-pad
    // of the neighbouring sliders (otherwise the lower part of one slider grabs the next one).
    { ELEMENT_TEXT,   0, 0, NULL,            3, { 40, 111,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER, 0, 0, (const char*)73, 0, {140, 110, 270, 28}, 1, 0, NULL, jkGuiDisplay_VRHudSliderDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {420, 111,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, { 40, 175,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER, 0, 0, (const char*)73, 0, {140, 174, 270, 28}, 1, 0, NULL, jkGuiDisplay_VRHudSliderDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {420, 175,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, { 40, 239,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER, 0, 0, (const char*)101,0, {140, 238, 270, 28}, 1, 0, NULL, jkGuiDisplay_VRHudSliderDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {420, 239,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, { 40, 303,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER, 0, 0, (const char*)116,0, {140, 302, 270, 28}, 1, 0, NULL, jkGuiDisplay_VRHudSliderDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {420, 303,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, { 40, 367,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_SLIDER, 0, 0, (const char*)61, 0, {140, 366, 270, 28}, 1, 0, NULL, jkGuiDisplay_VRHudSliderDraw, 0, slider_images, {0}, 0},
    { ELEMENT_TEXT,   0, 0, NULL,            3, {420, 367,  90, 28}, 1, 0, NULL, 0, 0, 0, {0}, 0},
    { ELEMENT_END,    0, 0, NULL,            0, {0},                 0, 0, NULL, 0, 0, 0, {0}, 0},
};
static jkGuiMenu jkGuiDisplay_menuVRHud = { jkGuiDisplay_aElementsVRHud, 0, 0xFF, 0xE1, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };
int jkGuiDisplay_ShowVRHud(void);

#ifndef PLATFORM_VR
static jkGuiElement jkGuiDisplay_aElementsAdvanced[22] = {
    { ELEMENT_TEXT,        0,            0, NULL,                   3, {0, 410, 640, 20},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXT,        0,            6, "GUI_SETUP",            3, {20, 20, 600, 40},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_GENERAL,  2, "GUI_GENERAL",          3, {20, 80, 120, 40},   1, 0, "GUI_GENERAL_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_GAMEPLAY, 2, "GUI_GAMEPLAY",         3, {140, 80, 120, 40},  1, 0, "GUI_GAMEPLAY_HINT",         0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_DISPLAY,  2, "GUI_DISPLAY",          3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_SOUND,    2, "GUI_SOUND",            3, {380, 80, 120, 40},  1, 0, "GUI_SOUND_HINT",            0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON,  GUI_CONTROLS, 2, "GUI_CONTROLS",         3, {500, 80, 120, 40},  1, 0, "GUI_CONTROLS_HINT",         0, 0, 0, {0}, 0},
    
    { ELEMENT_TEXTBUTTON,  1,            2, "GUI_OK",               3, {440, 430, 200, 40}, 1, 0, NULL,                        0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON, -1,            2, "GUI_CANCEL",           3, {0, 430, 200, 40},   1, 0, NULL,                        0, 0, 0, {0}, 0},
    
    { ELEMENT_CHECKBOX,    0,            0, "GUIEXT_EN_JKGFXMOD",            0, {20, 150, 300, 40},  1, 0, "GUIEXT_EN_JKGFXMOD_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_CHECKBOX,    0,            0, "GUIEXT_EN_TEXTURE_PRECACHE",   0, {20, 190, 300, 40},  1, 0, "GUIEXT_EN_TEXTURE_PRECACHE_HINT",          0, 0, 0, {0}, 0},

    { ELEMENT_TEXT,        0,            0, "GUIEXT_VR_SSAA_MULT",           2, {20, 240, 200, 20},  1, 0, "GUIEXT_VR_SSAA_MULT_HINT",          0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBOX,     0,            0, NULL,                            100, {230, 240, 80, 20}, 1, 0, NULL,                        0, 0, 0, {0}, 0},
    
    { ELEMENT_END,         0,            0, NULL,                   0, {0},                 0, 0, NULL,                        0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiDisplay_menuAdvanced = { jkGuiDisplay_aElementsAdvanced, 0, 0xFF, 0xE1, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };
#endif // !PLATFORM_VR


void jkGuiDisplay_Startup()
{
    jkGui_InitMenu(&jkGuiDisplay_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);

#ifdef PLATFORM_VR
    // VR Options: supersampling slider's inline value text (slider drives it; see VRSupersampleDraw)
    jkGuiDisplay_aElements[VR_EL_SS_VAL].wstr = vr_ss_text;
    jk_snwprintf(vr_ss_text, 255, L"%.2f", (flex32_t)jkPlayer_vrSupersampling);

    // Weapon pitch slider: static label + value text (slider drives the value; see VRPitchDraw)
    jkGuiDisplay_aElements[VR_EL_PITCH_LABEL].wstr = L"Weapon Pitch";
    jkGuiDisplay_aElements[VR_EL_PITCH_VAL].wstr = vr_pitch_text;

    // "HUD Layout" button label on the main VR Options menu.
    jkGuiDisplay_aElements[VR_EL_HUD_LAYOUT_BTN].wstr = L"HUD Layout...";

    // Init the HUD Layout sub-page and point its label/value elements at their buffers.
    jkGui_InitMenu(&jkGuiDisplay_menuVRHud, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jk_snwprintf(vrhud_title_text, 31, L"VR HUD Layout");
    jkGuiDisplay_aElementsVRHud[VRHUD_TITLE].wstr = vrhud_title_text;
    for (int i = 0; i < 5; i++) {
        jkGuiDisplay_aElementsVRHud[VRHUD_W_LABEL + i*3].wstr = s_hudCfg[i].label;
        jkGuiDisplay_aElementsVRHud[VRHUD_W_VAL   + i*3].wstr = vr_hud_val[i];
    }
#else
    // Desktop Display: set up textboxes and advanced menu
    jkGui_InitMenu(&jkGuiDisplay_menuAdvanced, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGuiDisplay_aElements[24].wstr = render_level;
    jkGuiDisplay_aElements[26].wstr = gamma_level;
    jkGuiDisplay_aElements[28].wstr = hud_level;
    jkGuiDisplay_aElementsAdvanced[12].wstr = vr_render_level;

    flex32_t ftmp;
    ftmp = jkPlayer_ssaaMultiple;
    jk_snwprintf(render_level, 255, L"%.2f", ftmp);
    ftmp = jkPlayer_gamma;
    jk_snwprintf(gamma_level, 255, L"%.2f", ftmp);
    ftmp = jkPlayer_hudScale;
    jk_snwprintf(hud_level, 255, L"%.2f", ftmp);
    jk_snwprintf(vr_render_level, 255, L"%.2f", 1.0f);
#endif
}

void jkGuiDisplay_Shutdown()
{
    ;
}

#ifdef PLATFORM_VR
// ============================================================================
// VR Options slider draw functions
// ============================================================================

void jkGuiDisplay_VRVignetteDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    int val = jkGuiDisplay_aElements[VR_EL_VIGNETTE_SLIDER].selectedTextEntry;
    if (val == 0)
        jk_snwprintf(vr_vignette_text, 16, L"Off");
    else
        jk_snwprintf(vr_vignette_text, 16, L"%d", val);
    jkGuiDisplay_aElements[VR_EL_VIGNETTE_VAL].wstr = vr_vignette_text;
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[VR_EL_VIGNETTE_VAL], menu, 1);
}

void jkGuiDisplay_VRSnapAngleDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    static const int angles[] = {30, 45, 90};
    int idx = jkGuiDisplay_aElements[VR_EL_SNAP_ANGLE_SLIDER].selectedTextEntry;
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    jk_snwprintf(vr_snap_angle_text, 8, L"%d", angles[idx]);
    jkGuiDisplay_aElements[VR_EL_SNAP_ANGLE_VAL].wstr = vr_snap_angle_text;
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[VR_EL_SNAP_ANGLE_VAL], menu, 1);
}

void jkGuiDisplay_VRSmoothSpeedDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    int speed = 20 + jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_SLIDER].selectedTextEntry;
    jk_snwprintf(vr_smooth_speed_text, 8, L"%d", speed);
    jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_VAL].wstr = vr_smooth_speed_text;
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_VAL], menu, 1);
}

void jkGuiDisplay_VRHeightDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    float height = ((float)jkGuiDisplay_aElements[VR_EL_HEIGHT_SLIDER].selectedTextEntry / 100.0f) - 0.5f;
    jk_snwprintf(vr_height_text, 16, L"%.2f m", height);
    jkGuiDisplay_aElements[VR_EL_HEIGHT_VAL].wstr = vr_height_text;
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[VR_EL_HEIGHT_VAL], menu, 1);
}

void jkGuiDisplay_VRSupersampleDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    // Slider step 0..45 -> 0.80 .. 1.25 supersampling, in 0.01 increments
    float ss = 0.80f + 0.01f * (float)jkGuiDisplay_aElements[VR_EL_SS_SLIDER].selectedTextEntry;
    jk_snwprintf(vr_ss_text, 255, L"%.2f", (flex32_t)ss);
    jkGuiDisplay_aElements[VR_EL_SS_VAL].wstr = vr_ss_text;
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[VR_EL_SS_VAL], menu, 1);
}

void jkGuiDisplay_VRPitchDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    // Slider step 0..50 -> -25 .. +25 degrees (step 25 = 0)
    int deg = jkGuiDisplay_aElements[VR_EL_PITCH_SLIDER].selectedTextEntry - 25;
    jk_snwprintf(vr_pitch_text, 16, L"%d deg", deg);
    jkGuiDisplay_aElements[VR_EL_PITCH_VAL].wstr = vr_pitch_text;
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[VR_EL_PITCH_VAL], menu, 1);
}

// Shared draw for the 5 HUD layout sliders. Identifies which one by element pointer, maps the
// slider step to a value via s_hudCfg, and writes the value text next to it.
void jkGuiDisplay_VRHudSliderDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    int k = -1;
    for (int i = 0; i < 5; i++) {
        if (element == &jkGuiDisplay_aElementsVRHud[VRHUD_W_SLIDER + i*3]) { k = i; break; }
    }
    if (k >= 0) {
        int step = element->selectedTextEntry;
        if (step < 0) step = 0;
        if (step >= s_hudCfg[k].n) step = s_hudCfg[k].n - 1;
        float v = s_hudCfg[k].base + s_hudCfg[k].inc * (float)step;
        jk_snwprintf(vr_hud_val[k], 15, L"%.2f", (flex32_t)v);
        jkGuiDisplay_aElementsVRHud[VRHUD_W_VAL + k*3].wstr = vr_hud_val[k];
    }
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    if (k >= 0)
        jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElementsVRHud[VRHUD_W_VAL + k*3], menu, 1);
}

// Show the HUD Layout sub-page: full-width sliders for the 5 HUD layout values.
int jkGuiDisplay_ShowVRHud(void)
{
    int v0;
    jkGui_sub_412E20(&jkGuiDisplay_menuVRHud, 100, 104, 102);
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiDisplay_menuVRHud, &jkGuiDisplay_aElementsVRHud[VRHUD_OK]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiDisplay_menuVRHud, &jkGuiDisplay_aElementsVRHud[VRHUD_CANCEL]);
    jkGuiSetup_sub_412EF0(&jkGuiDisplay_menuVRHud, 0);

    // Populate sliders from current globals.
    for (int i = 0; i < 5; i++) {
        int step = (int)(((*s_hudGlobals[i]) - s_hudCfg[i].base) / s_hudCfg[i].inc + 0.5f);
        if (step < 0) step = 0;
        if (step >= s_hudCfg[i].n) step = s_hudCfg[i].n - 1;
        jkGuiDisplay_aElementsVRHud[VRHUD_W_SLIDER + i*3].selectedTextEntry = step;
    }

    v0 = jkGuiRend_DisplayAndReturnClicked(&jkGuiDisplay_menuVRHud);
    if (v0 != -1) {
        for (int i = 0; i < 5; i++) {
            int step = jkGuiDisplay_aElementsVRHud[VRHUD_W_SLIDER + i*3].selectedTextEntry;
            if (step < 0) step = 0;
            if (step >= s_hudCfg[i].n) step = s_hudCfg[i].n - 1;
            *s_hudGlobals[i] = s_hudCfg[i].base + s_hudCfg[i].inc * (float)step;
        }
        stdVR_SyncConfigFromJkPlayer();
        jkPlayer_WriteConf(jkPlayer_playerShortName);
    }
    return v0;
}

#else // !PLATFORM_VR

void jkGuiDisplay_FovDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    uint32_t tmp = FOV_MIN + jkGuiDisplay_aElements[10].selectedTextEntry;
    
    jk_snwprintf(slider_val_text, 5, L"%u", tmp);
    jkGuiDisplay_aElements[11].wstr = slider_val_text;
    
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[11], menu, 1);
}

void jkGuiDisplay_FramelimitDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int redraw)
{
    uint32_t tmp = FPS_LIMIT_MIN + jkGuiDisplay_aElements[18].selectedTextEntry;
    
    if (tmp)
        jk_snwprintf(slider_val_text_2, 5, L"%u", tmp);
    else
        jk_snwprintf(slider_val_text_2, 5, L"None");

    jkGuiDisplay_aElements[19].wstr = slider_val_text_2;
    
    jkGuiRend_SliderDraw(element, menu, vbuf, redraw);
    
    jkGuiRend_UpdateAndDrawClickable(&jkGuiDisplay_aElements[19], menu, 1);
}

#endif // !PLATFORM_VR (desktop draw functions)

#ifndef PLATFORM_VR
int jkGuiDisplay_ShowAdvanced()
{
    int v0; // esi
#ifdef PLATFORM_VR
    flex32_t ftmp;
#endif

    jkGui_sub_412E20(&jkGuiDisplay_menuAdvanced, 100, 104, 100);
    jkGuiDisplay_aElementsAdvanced[9].selectedTextEntry = jkPlayer_bEnableJkgm;
    jkGuiDisplay_aElementsAdvanced[10].selectedTextEntry = jkPlayer_bEnableTexturePrecache;

#ifdef PLATFORM_VR
    jkGuiDisplay_aElementsAdvanced[11].bIsVisible = 1;
    jkGuiDisplay_aElementsAdvanced[12].bIsVisible = 1;
    ftmp = jkPlayer_vrSupersampling;
    jk_snwprintf(vr_render_level, 255, L"%.2f", ftmp);
#else
    jkGuiDisplay_aElementsAdvanced[11].bIsVisible = 0;
    jkGuiDisplay_aElementsAdvanced[12].bIsVisible = 0;
    jk_snwprintf(vr_render_level, 255, L"%.2f", 1.0f);
#endif
    
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiDisplay_menuAdvanced, &jkGuiDisplay_aElementsAdvanced[7]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiDisplay_menuAdvanced, &jkGuiDisplay_aElementsAdvanced[8]);
    jkGuiSetup_sub_412EF0(&jkGuiDisplay_menuAdvanced, 0);

    while (1)
    {
        v0 = jkGuiRend_DisplayAndReturnClicked(&jkGuiDisplay_menuAdvanced);

        if ( v0 != -1 )
        {
            jkPlayer_bEnableJkgm = jkGuiDisplay_aElementsAdvanced[9].selectedTextEntry;
            jkPlayer_bEnableTexturePrecache = jkGuiDisplay_aElementsAdvanced[10].selectedTextEntry;

#ifdef PLATFORM_VR
            char tmp[256];
            stdString_WcharToChar(tmp, vr_render_level, 255);
            if (_sscanf(tmp, "%f", &ftmp) != 1) {
                jkPlayer_vrSupersampling = 1.0f;
            } else {
                jkPlayer_vrSupersampling = ftmp;
            }
#endif

            std3D_PurgeEntireTextureCache();

            jkPlayer_WriteConf(jkPlayer_playerShortName);

#ifdef PLATFORM_VR
            stdVR_SyncConfigFromJkPlayer();
#endif
        }
        break;
    }
    return v0;
}

#endif // !PLATFORM_VR (ShowAdvanced)

int jkGuiDisplay_Show()
{
#ifdef PLATFORM_VR
    // ========================================================================
    // VR Options menu
    // ========================================================================
    int v0;

    jkGui_sub_412E20(&jkGuiDisplay_menu, 100, 104, 102);
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiDisplay_menu, &jkGuiDisplay_aElements[VR_EL_OK]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiDisplay_menu, &jkGuiDisplay_aElements[VR_EL_CANCEL]);
    jkGuiSetup_sub_412EF0(&jkGuiDisplay_menu, 0);

    // Load current VR settings into elements
    jkGuiDisplay_aElements[VR_EL_DOMINANT_HAND].selectedTextEntry     = jkPlayer_vrDominantHand;
    jkGuiDisplay_aElements[VR_EL_WEAPON_CROSSHAIR].selectedTextEntry = jkPlayer_vrWeaponCrosshair;
    jkGuiDisplay_aElements[VR_EL_MOVE_DIRECTION].selectedTextEntry    = jkPlayer_vrMoveDirection;
    jkGuiDisplay_aElements[VR_EL_SNAP_TURN].selectedTextEntry         = (jkPlayer_vrSnapTurnAngle > 0) ? 1 : 0;

    // Vignette slider: 0=off, 1-10=intensity
    {
        int vigVal = jkPlayer_vrComfortVignette;
        if (vigVal < 0) vigVal = 0;
        if (vigVal > 10) vigVal = 10;
        jkGuiDisplay_aElements[VR_EL_VIGNETTE_SLIDER].selectedTextEntry = vigVal;
    }

    // Snap angle: map 30->0, 45->1, 90->2
    {
        int snapIdx = 0;
        if (jkPlayer_vrSnapTurnAngle == 45) snapIdx = 1;
        else if (jkPlayer_vrSnapTurnAngle >= 90) snapIdx = 2;
        jkGuiDisplay_aElements[VR_EL_SNAP_ANGLE_SLIDER].selectedTextEntry = snapIdx;
    }

    // Smooth speed: offset from 20 minimum
    jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_SLIDER].selectedTextEntry = jkPlayer_vrSmoothTurnSpeed - 20;
    if (jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_SLIDER].selectedTextEntry < 0)
        jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_SLIDER].selectedTextEntry = 0;

    // Height: map float -0.5..+0.5 to slider 0..100
    jkGuiDisplay_aElements[VR_EL_HEIGHT_SLIDER].selectedTextEntry = (int)((jkPlayer_vrHeightOffset + 0.5f) * 100.0f);

    // Supersampling slider: 0.80..1.25 in 0.01 steps (slider step 0..45)
    {
        int ssStep = (int)((jkPlayer_vrSupersampling - 0.80f) / 0.01f + 0.5f);
        if (ssStep < 0) ssStep = 0;
        if (ssStep > 45) ssStep = 45;
        jkGuiDisplay_aElements[VR_EL_SS_SLIDER].selectedTextEntry = ssStep;
    }

    // Weapon pitch slider: -25..+25 degrees (slider step 0..50, 25 = 0 deg)
    {
        int pitchStep = jkPlayer_vrWeaponPitchAdjust + 25;
        if (pitchStep < 0) pitchStep = 0;
        if (pitchStep > 50) pitchStep = 50;
        jkGuiDisplay_aElements[VR_EL_PITCH_SLIDER].selectedTextEntry = pitchStep;
    }

vr_redisplay:
    v0 = jkGuiRend_DisplayAndReturnClicked(&jkGuiDisplay_menu);
    if (v0 == 500) {
        // "HUD Layout..." button -> open the dedicated sub-page, then return here.
        jkGuiDisplay_ShowVRHud();
        goto vr_redisplay;
    }
    if (v0 != -1)
    {
        // Write values back to jkPlayer globals
        jkPlayer_vrDominantHand    = jkGuiDisplay_aElements[VR_EL_DOMINANT_HAND].selectedTextEntry;
        jkPlayer_vrWeaponCrosshair = jkGuiDisplay_aElements[VR_EL_WEAPON_CROSSHAIR].selectedTextEntry;
        jkPlayer_vrMoveDirection   = jkGuiDisplay_aElements[VR_EL_MOVE_DIRECTION].selectedTextEntry;
        jkPlayer_vrComfortVignette = jkGuiDisplay_aElements[VR_EL_VIGNETTE_SLIDER].selectedTextEntry;

        // Snap turn mode
        if (jkGuiDisplay_aElements[VR_EL_SNAP_TURN].selectedTextEntry) {
            static const int angles[] = {30, 45, 90};
            int idx = jkGuiDisplay_aElements[VR_EL_SNAP_ANGLE_SLIDER].selectedTextEntry;
            if (idx < 0) idx = 0;
            if (idx > 2) idx = 2;
            jkPlayer_vrSnapTurnAngle = angles[idx];
        } else {
            jkPlayer_vrSnapTurnAngle = 0;
        }

        // Smooth turn speed (20-300 deg/sec)
        jkPlayer_vrSmoothTurnSpeed = 20 + jkGuiDisplay_aElements[VR_EL_SMOOTH_SPEED_SLIDER].selectedTextEntry;

        // Height offset (-0.5 to +0.5 meters)
        jkPlayer_vrHeightOffset = ((float)jkGuiDisplay_aElements[VR_EL_HEIGHT_SLIDER].selectedTextEntry / 100.0f) - 0.5f;

        // Supersampling (slider: 0.80 .. 1.25)
        jkPlayer_vrSupersampling = 0.80f + 0.01f * (float)jkGuiDisplay_aElements[VR_EL_SS_SLIDER].selectedTextEntry;

        // Weapon pitch adjust (slider: -25 .. +25 degrees)
        jkPlayer_vrWeaponPitchAdjust = jkGuiDisplay_aElements[VR_EL_PITCH_SLIDER].selectedTextEntry - 25;

        // (HUD layout values are edited on the HUD Layout sub-page, not here.)

        // Apply and save
        stdVR_SyncConfigFromJkPlayer();
        jkPlayer_WriteConf(jkPlayer_playerShortName);
    }
    return v0;

#else
    // ========================================================================
    // Desktop Display settings
    // ========================================================================
    flex32_t ftmp;
    int v0;

    jkGui_sub_412E20(&jkGuiDisplay_menu, 102, 104, 102);
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiDisplay_menu, &jkGuiDisplay_aElements[7]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiDisplay_menu, &jkGuiDisplay_aElements[8]);
    jkGuiSetup_sub_412EF0(&jkGuiDisplay_menu, 0);

    jkGuiDisplay_aElements[10].selectedTextEntry = jkPlayer_fov - FOV_MIN;
    jkGuiDisplay_aElements[12].selectedTextEntry = jkPlayer_fovIsVertical;
    jkGuiDisplay_aElements[13].selectedTextEntry = Window_isFullscreen;
    jkGuiDisplay_aElements[14].selectedTextEntry = Window_isHiDpi;
    jkGuiDisplay_aElements[15].selectedTextEntry = jkPlayer_enableTextureFilter;
    jkGuiDisplay_aElements[16].selectedTextEntry = jkPlayer_enableOrigAspect;

    jkGuiDisplay_aElements[18].selectedTextEntry = jkPlayer_fpslimit - FPS_LIMIT_MIN;
    jkGuiDisplay_aElements[20].selectedTextEntry = jkPlayer_enableVsync;
    jkGuiDisplay_aElements[21].selectedTextEntry = jkPlayer_enableBloom;
    jkGuiDisplay_aElements[22].selectedTextEntry = jkPlayer_enableSSAO;

    ftmp = jkPlayer_ssaaMultiple;
    jk_snwprintf(render_level, 255, L"%.2f", ftmp);
    ftmp = jkPlayer_gamma;
    jk_snwprintf(gamma_level, 255, L"%.2f", ftmp);
    ftmp = jkPlayer_hudScale;
    jk_snwprintf(hud_level, 255, L"%.2f", ftmp);

continue_menu:
    v0 = jkGuiRend_DisplayAndReturnClicked(&jkGuiDisplay_menu);
    if (v0 == GUI_ADVANCED)
    {
        jkGuiDisplay_ShowAdvanced();
        goto continue_menu;
    }
    else if ( v0 != -1 )
    {
        jkPlayer_fov = FOV_MIN + jkGuiDisplay_aElements[10].selectedTextEntry;
        jkPlayer_fovIsVertical = jkGuiDisplay_aElements[12].selectedTextEntry;
        Window_SetFullscreen(jkGuiDisplay_aElements[13].selectedTextEntry);
        Window_SetHiDpi(jkGuiDisplay_aElements[14].selectedTextEntry);
        jkPlayer_enableTextureFilter = jkGuiDisplay_aElements[15].selectedTextEntry;
        jkPlayer_enableOrigAspect = jkGuiDisplay_aElements[16].selectedTextEntry;
        jkPlayer_fpslimit = FPS_LIMIT_MIN + jkGuiDisplay_aElements[18].selectedTextEntry;
        jkPlayer_enableVsync = jkGuiDisplay_aElements[20].selectedTextEntry;
        jkPlayer_enableBloom = jkGuiDisplay_aElements[21].selectedTextEntry;
        jkPlayer_enableSSAO = jkGuiDisplay_aElements[22].selectedTextEntry;

        char tmp[256];
        stdString_WcharToChar(tmp, render_level, 255);

        if(_sscanf(tmp, "%f", &ftmp) != 1) {
            jkPlayer_ssaaMultiple = 1.0;
        }
        else {
            jkPlayer_ssaaMultiple = ftmp;
        }

        stdString_WcharToChar(tmp, gamma_level, 255);
        if(_sscanf(tmp, "%f", &ftmp) != 1) {
            jkPlayer_gamma = 1.0;
        }
        else {
            jkPlayer_gamma = ftmp;
        }

        stdString_WcharToChar(tmp, hud_level, 255);
        if(_sscanf(tmp, "%f", &ftmp) != 1) {
            jkPlayer_hudScale = 1.0;
        }
        else {
            jkPlayer_hudScale = ftmp;
        }

        if (jkPlayer_hudScale > 100.0) {
            jkPlayer_hudScale = 100.0;
        }

        jkPlayer_WriteConf(jkPlayer_playerShortName);

        // Make sure filter settings get applied
        std3D_UpdateSettings();
    }
    return v0;
#endif // PLATFORM_VR
}

void jkGuiDisplay_sub_4149C0(){}
