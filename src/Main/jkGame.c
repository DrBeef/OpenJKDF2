#include "jkGame.h"

#include "General/stdPalEffects.h"
#include "Main/sithMain.h"
#include "Engine/rdroid.h"
#include "Raster/rdCache.h"
#include "Engine/sithRender.h"
#include "World/sithWorld.h"
#include "World/jkPlayer.h"
#include "World/sithSector.h"
#include "Win95/Video.h"
#include "Win95/stdComm.h"
#include "Platform/std3D.h"
#include "Win95/stdDisplay.h"
#include "Main/jkHud.h"
#include "Main/jkHudInv.h"
#include "Main/jkHudScope.h"
#include "Main/jkHudCameraView.h"
#include "Main/jkDev.h"
#include "Main/jkQuakeConsole.h"
#include "Engine/rdColormap.h"
#include "Engine/sithCamera.h"
#include "General/stdString.h"

#include "stdPlatform.h"
#include "jk.h"

#if defined(TARGET_TWL)
#include <nds.h>
#endif

// Added: VR support
#ifdef PLATFORM_VR
#include "Platform/VR/stdVR.h"
#include "Engine/rdCamera.h"
#include "World/sithWeapon.h"
#include "World/sithTemplate.h"
#include "World/sithThing.h"
#include "Gameplay/sithTime.h"
#include "SDL2_helper.h"
extern sithThing* sithPlayer_pLocalPlayerThing;
extern flex_t sithTime_deltaSeconds;
#endif

int jkGame_Startup()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    
    sithWorld_SetSectionParser("jk", jkGame_ParseSection);
    jkGame_bInitted = 1;
    return 1;
}

int jkGame_ParseSection(sithWorld* a1, int a2)
{
    return a2 == 0;
}

void jkGame_ForceRefresh()
{
    sithCamera_Close();
    rdCanvas_Free(Video_pCanvas);
#ifdef SDL2_RENDER
    rdCanvas_Free(Video_pCanvasOverlayMap);
#endif
}

void jkGame_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    
    jkGame_bInitted = 0;
}

void jkGame_ScreensizeIncrease()
{
    if ( Video_modeStruct.viewSizeIdx < 0xAu )
    {
        // MOTS added
        if (Main_bMotsCompat) {
            jkHudScope_Close();
            jkHudCameraView_Close();
        }

#ifndef LINUX_TMP
        sithCamera_Close();
        rdCanvas_Free(Video_pCanvas);
#ifdef SDL2_RENDER
        rdCanvas_Free(Video_pCanvasOverlayMap);
#endif
        ++Video_modeStruct.viewSizeIdx;
        Video_camera_related();
#endif
        // MOTS added
        if (Main_bMotsCompat) {
            jkHudScope_Open();
            jkHudCameraView_Open();
        }
    }
}

void jkGame_ScreensizeDecrease()
{
    if ( Video_modeStruct.viewSizeIdx )
    {
        // MOTS added
        if (Main_bMotsCompat) {
            jkHudScope_Close();
            jkHudCameraView_Close();
        }

#ifndef LINUX_TMP
        sithCamera_Close();
        rdCanvas_Free(Video_pCanvas);
#ifdef SDL2_RENDER
        rdCanvas_Free(Video_pCanvasOverlayMap);
#endif
        --Video_modeStruct.viewSizeIdx;
        Video_camera_related();
#endif
        // MOTS added
        if (Main_bMotsCompat) {
            jkHudScope_Open();
            jkHudCameraView_Open();
        }
    }
}

void jkGame_SetDefaultSettings()
{
    jkPlayer_setFullSubtitles = 0;
    jkPlayer_setDisableCutscenes = 0;
    jkPlayer_setRotateOverlayMap = 1;
    jkPlayer_setDrawStatus = 1;
    jkPlayer_setCrosshair = 0;
    jkPlayer_setSaberCam = 0;
}

int jkGame_Update()
{
    int64_t v0; // rcx
    sithThing *v2; // esi
    int v3; // eax
    flex_d_t v4; // st7
    int result; // eax
    int v6; // [esp+1Ch] [ebp-1Ch]

    static int jkGame_Update_Start = 0;
    static int jkGame_Update_ClearScreen = 0;
    static int jkGame_Update_AdvanceFrame = 0;
    static int jkGame_Update_UpdateCamera = 0;
    static int jkGame_Update_DrawPov = 0;
    static int jkGame_Update_HudDrawn = 0;
    static int jkGame_Update_End = 0;

    jkGame_Update_Start = stdPlatform_GetTimeMsec();

    // HACK HACK HACK: Adjust zNear depending on if we're using the scope/camera views
#if defined(SDL2_RENDER) || defined(TARGET_TWL)
    if (sithCamera_cameras[0].rdCam.pClipFrustum) {
        sithCamera_cameras[0].rdCam.pClipFrustum->zNear = SITHCAMERA_ZNEAR_FIRSTPERSON;

        if (Main_bMotsCompat) {
            if (playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_SCOPEHUD) {
                sithCamera_cameras[0].rdCam.pClipFrustum->zNear = SITHCAMERA_ZNEAR;
            }
            if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_80000000) != 0) {
                sithCamera_cameras[0].rdCam.pClipFrustum->zNear = SITHCAMERA_ZNEAR;
            }
        }
    }
    
#endif

#if defined(SDL2_RENDER) || defined(TARGET_TWL)
    // HACK
    Video_modeStruct.b3DAccel = 1;
#endif

#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( Video_modeStruct.Video_8606C0 || Video_modeStruct.geoMode <= 2 )
#endif
#if !defined(TARGET_TWL)
        stdDisplay_VBufferFill(Video_pMenuBuffer, Video_fillColor, 0); // Significant delay on TWL
#endif
    jkDev_DrawLog();
    jkHudInv_ClearRects();
    jkHud_ClearRects(0);
    jkGame_Update_ClearScreen = stdPlatform_GetTimeMsec();

    stdPalEffects_UpdatePalette(stdDisplay_GetPalette());
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( Video_modeStruct.b3DAccel )
#endif
        rdSetColorEffects(&stdPalEffects_state.effect);

#if defined(SDL2_RENDER) || defined(TARGET_TWL)
    _memcpy(stdDisplay_masterPalette, sithWorld_pCurrentWorld->colormaps->colors, 0x300);
#endif

// Added: VR stereo rendering path
#ifdef PLATFORM_VR
    // Forward declarations for VR logging and state
    extern void VR_Log(const char* fmt, ...);
    extern int stdVR_currentEye;

#ifdef VR_WEAPON_ALIGNMENT_TOOL
    // Update the weapon alignment tool (processes controller input for adjustment)
    if (stdVR_bEnabled) {
        stdVR_AlignmentTool_Update(sithTime_deltaSeconds);
    }
#endif

    // Check for VR test without headset mode
    extern int32_t Main_bVRTest;
    extern int32_t Main_bVRTestNoHeadset;
    extern int32_t Main_vrTestFrameTarget;
    extern int32_t Main_vrTestFrameCount;
    extern int32_t Main_vrTestState;

    // Simulated VR test mode (no headset required) - works when gameplay starts via any method
    if (Main_bVRTestNoHeadset && jkGame_isDDraw && (!stdVR_bEnabled || !stdVR_IsSessionRunning())) {
        static int simVRFrameCount = 0;
        simVRFrameCount++;

        // Mark gameplay started
        if (Main_vrTestState == 0) {
            Main_vrTestState = 1;
            VR_Log("=== VR-SIM AUTO-TEST: Gameplay started, will screenshot at frame %d ===\n", Main_vrTestFrameTarget);
        }

        // At target frame, render both eye views and save screenshots
        if (simVRFrameCount == Main_vrTestFrameTarget) {
            VR_Log("=== VR-SIM: Capturing stereo screenshots ===\n");

            // Initialize simulated VR eye data if not already set up
            // This provides fake stereo separation for testing without a headset
            if (stdVR_clientInfo.eyes[0].fovRight == 0.0f) {
                VR_Log("=== VR-SIM: Initializing simulated VR eye data ===\n");

                // Set up render dimensions (use current display size)
                stdVR_clientInfo.renderWidth = stdDisplay_pCurVideoMode->format.width;
                stdVR_clientInfo.renderHeight = stdDisplay_pCurVideoMode->format.height;

                // Set up FOV for both eyes (typical VR FOV ~90 degrees = ~0.78 radians)
                // fovLeft/Right/Up/Down are tangent values
                float fovTangent = 1.0f;  // tan(45 degrees) for ~90 degree total FOV

                for (int e = 0; e < 2; e++) {
                    // Note: fovLeft/Right/Up/Down are tangent values, all POSITIVE
                    // (OpenXR angles are signed, but we store positive tangents)
                    stdVR_clientInfo.eyes[e].fovLeft = fovTangent;   // tan(left half-angle)
                    stdVR_clientInfo.eyes[e].fovRight = fovTangent;  // tan(right half-angle)
                    stdVR_clientInfo.eyes[e].fovUp = fovTangent * 0.75f;    // tan(up half-angle)
                    stdVR_clientInfo.eyes[e].fovDown = fovTangent * 0.75f;  // tan(down half-angle)

                    // Set view matrix to identity initially (will be adjusted per-eye)
                    rdMatrix_Identity34(&stdVR_clientInfo.eyes[e].viewMatrix);
                }

                // Apply eye separation (typical IPD ~63mm = 0.063m, we use 0.032m per eye from center)
                float eyeOffset = 0.032f;  // Half IPD
                stdVR_clientInfo.eyes[0].viewMatrix.scale.x = -eyeOffset;  // Left eye
                stdVR_clientInfo.eyes[1].viewMatrix.scale.x = eyeOffset;   // Right eye

                VR_Log("=== VR-SIM: Eye data initialized - renderSize=%dx%d, fov=%.2f, IPD=%.3f ===\n",
                    stdVR_clientInfo.renderWidth, stdVR_clientInfo.renderHeight, fovTangent, eyeOffset * 2);
            }

            // Set up camera for VR frame
            sithCamera_PrepareFrameVR();

            // Initialize scene once (this sizes the framebuffer correctly)
            // We do NOT want to call rdAdvanceFrame per-eye because:
            // 1. It swaps framebuffers, and FBO regeneration during swaps can cause
            //    both framebuffers to get the same FBO ID due to OpenGL ID recycling
            // 2. We want to render both eyes to the same sized FBO, just with different views
            rdAdvanceFrame();

            // Render each eye to the same FBO (cleared between eyes)
            for (int eye = 0; eye < 2; eye++) {
                stdVR_currentEye = eye;

                // Clear the internal FBO for this eye's render
                std3D_ClearMainFbo();

                VR_Log("=== VR-SIM: Rendering eye %d ===\n", eye);

                // Set VR camera view for this eye
                sithCamera_SetVRView(eye);

                // Advance render tick so sectors aren't skipped
                sithMain_sub_4C4D80();

                // Render scene
                sithRender_Draw();
                jkPlayer_DrawPov();

                // Flush all render commands
                rdCache_Flush();
                sithCamera_RestoreVRView();
                glFinish();  // Ensure all GL commands complete

                // Save screenshot immediately (before clearing for next eye)
                char filename[32];
                snprintf(filename, sizeof(filename), "vrtest_eye%d.ppm", eye);
                std3D_DebugSaveInternalFbo(filename);
                VR_Log("=== VR-SIM: Saved %s ===\n", filename);
            }

            // Clear VR state
            stdVR_currentEye = -1;
            rdCamera_ClearVRProjection();
            stdVR_ClearCurrentEyeViewMatrix();  // Added: Clear per-eye view matrix

            VR_Log("=== VR-SIM AUTO-TEST COMPLETE ===\n");
            Main_vrTestState = 2;

            // Exit
            extern void Main_Shutdown(void);
            Main_Shutdown();
            exit(0);
        }

        // Normal single-view render for frames before screenshot
        goto non_vr_path;
    }

    // Non-test VR mode tracking for manual testing
    // If VR test is enabled but no headset, just track frames and exit after target
    if (Main_bVRTest && !Main_bVRTestNoHeadset && jkGame_isDDraw && (!stdVR_bEnabled || !stdVR_IsSessionRunning())) {
        static int nonHeadsetFrameCount = 0;
        nonHeadsetFrameCount++;

        if (nonHeadsetFrameCount == 1) {
            VR_Log("=== VR TEST: Gameplay started without headset, running for %d frames ===\n", Main_vrTestFrameTarget);
        }

        if (nonHeadsetFrameCount >= Main_vrTestFrameTarget) {
            VR_Log("=== VR TEST: Frame target reached, exiting ===\n");
            extern void Main_Shutdown(void);
            Main_Shutdown();
            exit(0);
        }
    }

    if (stdVR_bEnabled && stdVR_IsSessionRunning() && jkGame_isDDraw) {
        static int vrRenderCount = 0;
        vrRenderCount++;

        // If no frame is pending, we need to call WaitFrame ourselves
        // This happens in 3D game mode where Window_Main_Loop doesn't manage VR frames
        if (!stdVR_IsFramePending()) {
            stdVR_WaitFrame();

            // Update VR input after WaitFrame (in 3D mode, Window_Main_Loop skips this)
            stdVR_UpdateInput();
            stdVR_MapInputToGame();
        }

        // Check if we have a VR frame pending to work with
        if (!stdVR_IsFramePending()) {
            goto non_vr_path;
        }

        // VR frame timing - BeginFrame must be called first
        if (!stdVR_BeginFrame()) {
            goto non_vr_path;  // Fall back to non-VR if frame begin fails
        }

        // Update tracking
        stdVR_UpdateTracking();

        // Update screen layer state (will be false for 3D gameplay, true for menus)
        // This ensures bUseScreenLayer is updated when transitioning from menu to gameplay
        stdVR_UseScreenLayer();

        // Only render if the runtime tells us to (headset visible, etc.)
        if (stdVR_clientInfo.bShouldRender) {
            // Do per-frame setup ONCE before eye loop
            rdAdvanceFrame();  // This calls std3D_StartScene via rdCache_AdvanceFrame
            sithCamera_PrepareFrameVR();  // Updates camera position, sets rdCamera

#if defined(TARGET_ANDROID_NATIVE_GLES)  // MultiView for Quest VR
            // === MultiView Path (single-pass stereo rendering) ===
            // Uses GL_OVR_multiview2 to render both eyes in a single draw call.
            // Since engine uses CPU projection, shader applies IPD offset in screen-space.
            if (stdVR_IsMultiViewSupported()) {
                if (stdVR_PrepareMultiViewBuffer()) {
                    // Clear internal render target
                    std3D_ClearMainFbo();

                    // Upload both eye view/projection matrices to UBOs
                    // The shader will use gl_ViewID_OVR to select the correct matrices
                    stdVR_SetMultiViewMatrices(0.01f, 1000.0f);  // zNear, zFar

                    // Set up center camera (MultiView handles eye separation in shader)
                    sithCamera_SetVRViewMultiView();

                    // Advance render tick
                    sithMain_sub_4C4D80();

                    // Render scene once - GPU renders to both eye layers
                    sithRender_Draw();
                    jkPlayer_DrawPov();

                    // Flush render cache
                    rdCache_Flush();
					sithCamera_RestoreVRView();

                    // Resolve to VR swapchain (both eyes)
                    std3D_DrawSceneFbo();

                    // Reset render lists
                    rdCache_ResetRenderList();

                    // Release MultiView buffer
                    stdVR_FinishMultiViewBuffer();
                }
            } else
#endif
            {
                // === Per-Eye Path (fallback, always used for now) ===
                // Render each eye separately
                for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
                    if (!stdVR_PrepareEyeBuffer(eye)) {
                        continue;
                    }


                // Clear internal render target per-eye to avoid depth/color leakage
                std3D_ClearMainFbo();

                // Set up VR view for this eye (applies eye offset and projection)
                sithCamera_SetVRView(eye);

                // Advance render tick for each eye so sectors don't get skipped
                // The render system uses sithRender_lastRenderTick to mark sectors as "already rendered"
                // Without this, the second eye would skip all sectors because they were rendered for the first eye
                sithMain_sub_4C4D80();

                // Render scene for this eye
                sithRender_Draw();
                jkPlayer_DrawPov();

                // Flush render cache per-eye so triangles are actually drawn to internal FBO
                // before we blit to VR swapchain. Without this, both eyes get empty content.
                rdCache_Flush();
                sithCamera_RestoreVRView();

                // Save screenshot for VR auto-test mode
                {
                    extern int32_t Main_bVRTest;
                    extern int32_t Main_vrTestFrameTarget;
                    extern int32_t Main_vrTestState;

                    static int screenshotSaved[2] = {0, 0};
                    int targetFrame = Main_bVRTest ? Main_vrTestFrameTarget : 30;

                    // Mark that we're in gameplay for VR test
                    if (Main_bVRTest && Main_vrTestState == 0) {
                        Main_vrTestState = 1;
                        VR_Log("=== VR AUTO-TEST: Gameplay started, will screenshot at frame %d ===\n",
                               targetFrame);
                    }

                        if (vrRenderCount == targetFrame && eye >= 0 && eye < 2 && !screenshotSaved[eye]) {
                            screenshotSaved[eye] = 1;
                            char filename[256];
                            snprintf(filename, sizeof(filename), "vrtest_eye%d.ppm", eye);
                            std3D_DebugSaveInternalFbo(filename);
                            VR_Log("=== VR AUTO-TEST: Saved %s ===\n", filename);
                        }
                    }

                    // Resolve internal FBO to VR swapchain
                    std3D_DrawSceneFbo();

                    // Reset render lists per-eye; otherwise GL_tmpVertices accumulates and
                    // the second eye can exceed STD3D_MAX_VERTICES and render nothing.
                    rdCache_ResetRenderList();

                    // Release eye buffer
                    stdVR_FinishEyeBuffer(eye);
                }
            }
        }

        // Clear VR projection state before potentially falling through to non-VR code
        rdCamera_ClearVRProjection();
        stdVR_ClearCurrentEyeViewMatrix();  // Added: Clear per-eye view matrix

        // Flush and clear render state (equivalent to what rdFinishFrame does for non-VR)
        rdCache_Flush();
        rdCache_ClearFrameCounters();

        // Added: Render HUD to dedicated VR HUD buffer (quad layer)
        // This must happen AFTER eye rendering but BEFORE EndFrame
        if (stdVR_IsHudEnabled()) {
            int vrHudPrepared = stdVR_PrepareHudBuffer();
            if (vrHudPrepared) {
                static int vrHudRenderCount = 0;
                if (++vrHudRenderCount <= 10) {
                    VR_Log("jkGame: VR HUD rendering to quad layer, vrHudPrepared=%d\n", vrHudPrepared);
                }

                // Draw HUD elements (these render to Video overlay buffers)
                if (!Main_bMotsCompat) {
                    if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_NOHUD) == 0) {
                        jkHud_Draw();
                    }
                }
                else {
                    if (playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_SCOPEHUD) {
                        jkHudScope_Draw();
                    }
                    if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_80000000) == 0) {
                        if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_NOHUD) == 0) {
                            jkHud_Draw();
                        }
                    }
                    else {
                        jkHudCameraView_Draw();
                    }
                }
                jkHudInv_Draw();

#ifdef VR_WEAPON_ALIGNMENT_TOOL
                // Draw alignment tool overlay if active
                if (stdVR_AlignmentTool_IsActive()) {
                    stdVR_AlignmentTool_DrawOverlay();
                }
#endif

                // Flush the UI render list to the VR HUD FBO
                // HUD elements are queued via std3D_DrawUIBitmap and need to be flushed
                int hudWidth, hudHeight;
                stdVR_GetHudSize(&hudWidth, &hudHeight);
                std3D_DrawUIRenderListToCurrentFBO(hudWidth, hudHeight);

                // Also blit the overlay buffer content (crosshair, target rings, overlay map)
                // These are drawn via rdPrimit2 to Video_pCanvasOverlayMap
                std3D_DrawOverlayToCurrentFBO(hudWidth, hudHeight);

                // Debug: Save HUD FBO content to file for inspection
#if 0
                {
                    extern int32_t Main_bVRTest;
                    extern int32_t Main_vrTestFrameCount;
                    static int hudSaveCount = 0;
                    if (Main_bVRTest && Main_vrTestFrameCount == 59 && hudSaveCount == 0) {
                        hudSaveCount++;
                        int hudFbo = stdVR_GetHudFBO();
                        if (hudFbo > 0) {
                            VR_Log("Saving HUD FBO content: fbo=%d size=%dx%d\n", hudFbo, hudWidth, hudHeight);
                            // Read pixels from HUD FBO
                            glBindFramebuffer(GL_FRAMEBUFFER, hudFbo);
                            uint8_t* pixels = (uint8_t*)malloc(hudWidth * hudHeight * 4);
                            if (pixels) {
                                glReadPixels(0, 0, hudWidth, hudHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                                // Save as PPM
                                FILE* fp = fopen("vrtest_hud.ppm", "wb");
                                if (fp) {
                                    fprintf(fp, "P6\n%d %d\n255\n", hudWidth, hudHeight);
                                    for (int y = hudHeight - 1; y >= 0; y--) {
                                        for (int x = 0; x < hudWidth; x++) {
                                            int idx = (y * hudWidth + x) * 4;
                                            fputc(pixels[idx], fp);     // R
                                            fputc(pixels[idx+1], fp);   // G
                                            fputc(pixels[idx+2], fp);   // B
                                        }
                                    }
                                    fclose(fp);
                                    VR_Log("Saved vrtest_hud.ppm\n");
                                }
                                free(pixels);
                            }
                        }
                    }
                }
#endif
                stdVR_FinishHudBuffer();
            }
        }

        // End VR frame
        stdVR_EndFrame();

        // VR automated test mode - count frames and screenshot/exit
        {
            extern int32_t Main_bVRTest;
            extern int32_t Main_vrTestFrameTarget;
            extern int32_t Main_vrTestFrameCount;
            extern int32_t Main_vrTestState;

            if (Main_bVRTest && Main_vrTestState == 1) {
                Main_vrTestFrameCount++;

                // VR TEST: Auto-fire every 180 frames (~3 seconds) to test fire positions
                // This fires a projectile from the controller position
                // Using 180 frames gives 20 fire events over 60 seconds with varied positions
                if (Main_vrTestFrameCount % 180 == 90 && sithPlayer_pLocalPlayerThing) {
                    // Find a projectile template (bryar pistol bolt)
                    sithThing* pProjectile = sithTemplate_GetEntryByName("+bryarbolt");
                    if (!pProjectile) {
                        pProjectile = sithTemplate_GetEntryByName("+stlaser");
                    }
                    if (!pProjectile) {
                        pProjectile = sithTemplate_GetEntryByName("+laser");
                    }

                    if (pProjectile) {
                        rdVector3 fireOffset = {0.0f, 0.0f, 0.0f};
                        rdVector3 aimError = {0.0f, 0.0f, 0.0f};

                        VR_Log("\n=== VR TEST AUTO-FIRE frame %d ===\n", Main_vrTestFrameCount);
                        VR_Log("Player pos: (%.3f, %.3f, %.3f)\n",
                            sithPlayer_pLocalPlayerThing->position.x,
                            sithPlayer_pLocalPlayerThing->position.y,
                            sithPlayer_pLocalPlayerThing->position.z);

                        // Log controller orientation for rotation testing
                        int hand = stdVR_GetDominantHand();
                        stdVR_ControllerState* pCtrl = stdVR_GetController(hand);
                        if (pCtrl && pCtrl->bTracking) {
                            VR_Log("Controller orientation: pitch=%.1f yaw=%.1f roll=%.1f\n",
                                pCtrl->orientation.x, pCtrl->orientation.y, pCtrl->orientation.z);
                            VR_Log("Controller poseMatrix lvec (forward): (%.3f, %.3f, %.3f)\n",
                                pCtrl->poseMatrix.lvec.x, pCtrl->poseMatrix.lvec.y, pCtrl->poseMatrix.lvec.z);
                        }

                        // Fire the projectile - this should use controller position in VR
                        sithThing* pFired = sithWeapon_FireProjectile(
                            sithPlayer_pLocalPlayerThing,
                            pProjectile,
                            NULL,  // No sound
                            -1,    // Mode
                            &fireOffset,
                            &aimError,
                            1.0f,  // Scale
                            0,     // Scale flags
                            0.0f,  // No autoaim
                            0.0f,
                            0
                        );

                        if (pFired) {
                            VR_Log("Fired projectile at: (%.3f, %.3f, %.3f)\n",
                                pFired->position.x, pFired->position.y, pFired->position.z);

                            // Take screenshot showing bullet and gun position
                            static int fireScreenshotNum = 0;
                            char screenshotName[128];
                            snprintf(screenshotName, sizeof(screenshotName), "vrtest_fire_%02d.bmp", fireScreenshotNum++);
                            VR_Log("Saving screenshot: %s\n", screenshotName);
                            jkGame_Screenshot();
                        } else {
                            VR_Log("Fire FAILED - no projectile spawned\n");
                        }
                    } else {
                        VR_Log("VR TEST: No projectile template found for auto-fire\n");
                    }
                }

                // Check if we should exit after target frame count
                // Note: Per-eye screenshots are saved in the render loop above
                if (Main_vrTestFrameCount == Main_vrTestFrameTarget) {
                    VR_Log("\n=== VR AUTO-TEST: Completed frame %d ===\n", Main_vrTestFrameCount);
                    VR_Log("VR Test Complete. Check vrtest_eye0.ppm, vrtest_eye1.ppm and vr_debug.log\n");
                    VR_Log("=== VR AUTO-TEST COMPLETE ===\n");

                    Main_vrTestState = 2;  // Mark as done

                    // Keep window visible for 2 seconds so user can see the result
                    stdPlatform_Printf("VR Test Complete - exiting...\n");
                    SDL_Delay(2000);

                    // Exit the game
                    extern void Main_Shutdown(void);
                    Main_Shutdown();
                    exit(0);
                }
            }
        }

        jkGame_Update_AdvanceFrame = stdPlatform_GetTimeMsec();
        jkGame_Update_UpdateCamera = stdPlatform_GetTimeMsec();
        jkGame_Update_DrawPov = stdPlatform_GetTimeMsec();
        return 1;  // Skip non-VR rendering path when VR is active
    }
non_vr_path:
    ;  // Label needs a statement
#endif // PLATFORM_VR

    // Non-VR rendering path
    {
        rdAdvanceFrame();
        jkGame_Update_AdvanceFrame = stdPlatform_GetTimeMsec();
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
        if ( Video_modeStruct.b3DAccel )
#endif
        {
            sithMain_UpdateCamera();
        }
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
        else
        {
            stdDisplay_VBufferLock(Video_pMenuBuffer);
            stdDisplay_VBufferLock(Video_pVbufIdk);
            sithMain_UpdateCamera();
            stdDisplay_VBufferUnlock(Video_pVbufIdk);
            stdDisplay_VBufferUnlock(Video_pMenuBuffer);
        }
#endif
        jkGame_Update_UpdateCamera = stdPlatform_GetTimeMsec();
        jkPlayer_DrawPov();
        jkGame_Update_DrawPov = stdPlatform_GetTimeMsec();
    }

#if 1
    //if (Main_bMotsCompat)
    ++Video_dword_5528A0; // MOTS added
    if ( Main_bDispStats )
    {
        v2 = sithWorld_pCurrentWorld->playerThing;
        //++Video_dword_5528A0; // MOTS removed
        v3 = stdPlatform_GetTimeMsec();
        v0 = v3 - Video_lastTimeMsec;
        Video_dword_5528A8 = v3;
        if ( (unsigned int)(v3 - Video_lastTimeMsec) > 0x3E8 )
        {
            if ( Main_bDispStats )
            {
                v6 = v2->sector->id;
                Video_flt_55289C = (flex_d_t)(Video_dword_5528A0 - Video_dword_5528A4) * 1000.0 / (flex_d_t)v0;
                _sprintf(
                    std_genBuffer,
                    "%02.3f (%02d%%)f %3ds %3da %3dz %4dp %3d curSector %3d fo",
                    Video_flt_55289C,
                    (unsigned int)(__int64)((flex_d_t)(unsigned int)jkGame_updateMsecsTotal / (flex_d_t)(int)v0 * 100.0),
                    sithRender_sectorsDrawn,
                    sithRender_geoThingsDrawn,
                    sithRender_nongeoThingsDrawn,
                    rdCache_drawnFaces,
                    v6,
                    sithNet_thingsIdx);
                if ( sithNet_isMulti )
                    _sprintf(&std_genBuffer[_strlen(std_genBuffer)], " %d m %d b", stdComm_dword_8321F4, stdComm_dword_8321F0);
                jkDev_sub_41FC40(100, std_genBuffer);
                v3 = Video_dword_5528A8;
            }
            Video_lastTimeMsec = v3;
            Video_dword_5528A4 = Video_dword_5528A0;
            jkGame_dword_552B5C = 0;
            jkGame_updateMsecsTotal = 0;
            stdComm_dword_8321F0 = 0;
            stdComm_dword_8321F4 = 0;
        }
    }
    else if ( Main_bFrameRate )
    {
        //++Video_dword_5528A0; // MOTS removed
        Video_dword_5528A8 = stdPlatform_GetTimeMsec();
        if ( (unsigned int)(Video_dword_5528A8 - Video_lastTimeMsec) > 1000 )
        {
            v4 = (flex_d_t)(Video_dword_5528A0 - Video_dword_5528A4) * 1000.0 / (flex_d_t)(unsigned int)(Video_dword_5528A8 - Video_lastTimeMsec);
            Video_flt_55289C = v4;
            _sprintf(std_genBuffer, "%02.3f", v4);
            jkDev_sub_41FC40(100, std_genBuffer);
            Video_lastTimeMsec = Video_dword_5528A8;
            Video_dword_5528A4 = Video_dword_5528A0;
        }
    }
#endif

#if defined(SDL2_RENDER)
    stdVBuffer* pOverlayBuffer = Video_pCanvasOverlayMap->vbuffer;
    stdDisplay_VBufferLock(pOverlayBuffer);
    stdDisplay_VBufferFill(pOverlayBuffer, Video_fillColor, 0);
    stdDisplay_VBufferUnlock(pOverlayBuffer);
#endif

    // MOTS added: scope/security cam overlays
    // Note: VR HUD rendering is handled in the VR render path above, not here
    if (!Main_bMotsCompat) {
        if ( (playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_NOHUD) == 0 ) {
            jkHud_Draw();
        }
    }
    else {
        if (playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_SCOPEHUD) {
            jkHudScope_Draw();
        }
        if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_80000000) == 0) {
            if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_NOHUD) == 0) {
                jkHud_Draw();
            }
        }
        else {
            jkHudCameraView_Draw();
        }
    }

    jkGame_Update_HudDrawn = stdPlatform_GetTimeMsec();

    jkDev_BlitLogToScreen();
    jkHudInv_Draw();
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( Video_modeStruct.b3DAccel )
        std3D_DrawOverlay();
#endif

    // MOTS added
    /*
    if (Main_bRecord != 0) {
        jkGame_Screenshot();
    }
    */

#if defined(SDL2_RENDER)
    jkQuakeConsole_Render();
#endif

#if defined(SDL2_RENDER) || defined(TARGET_TWL)
    std3D_DrawMenu();
    rdFinishFrame();
#endif

// VR frames are ended in the VR render paths (gameplay or menu). Avoid calling
// xrEndFrame here without a matching BeginFrame.

    // MOTS removed
    if ( Video_modeStruct.b3DAccel )
        result = stdDisplay_DDrawGdiSurfaceFlip();
    else
        result = stdDisplay_VBufferCopy(Video_pOtherBuf, Video_pMenuBuffer, 0, 0, 0, 0);
    // end MOTS removed

    // MOTS added
    /*
    if ((Video_modeStruct.Video_motsNew1 != 0) && (Video_modeStruct.b3DAccel == 0)) {
        result = stdDisplay_VBufferCopy(Video_pOtherBuf,Video_pMenuBuffer,0,0,NULL,0);
        return result;
    }
    result = stdDisplay_DDrawGdiSurfaceFlip();
    */

    jkGame_Update_End = stdPlatform_GetTimeMsec();

#if defined(TARGET_TWL)
    int jkGame_Delta_Start_ClearScreen = jkGame_Update_ClearScreen - jkGame_Update_Start;
    int jkGame_Delta_ClearScreen_AdvanceFrame = jkGame_Update_AdvanceFrame - jkGame_Update_ClearScreen;
    int jkGame_Delta_AdvanceFrame_UpdateCamera = jkGame_Update_UpdateCamera - jkGame_Update_AdvanceFrame;
    int jkGame_Delta_UpdateCamera_DrawPov = jkGame_Update_DrawPov - jkGame_Update_UpdateCamera;
    int jkGame_Delta_DrawPov_HudDrawn = jkGame_Update_HudDrawn - jkGame_Update_DrawPov;
    int jkGame_Delta_HudDrawn_End = jkGame_Update_End - jkGame_Update_HudDrawn;
    
    static int last_time_ms = 0;
    int now_ms = stdPlatform_GetTimeMsec();
    int total_delta = now_ms - last_time_ms;
    last_time_ms = now_ms;
    extern int std3D_timeWastedWaitingAround;
    extern int32_t sithRender_numSectors;

    int healthNum = 0;
    int shieldsNum = 0;
    int forceNum = 0;
    int ammoNum = 0;
    int currentItemBin = 0;
    int currentForceBin = 0;
    int bHasSuperShields = 0;
    int bHasSuperWeapon = 0;
    int bHasForceSurge = 0;
    int bHasFieldLight = 0;

    if (sithWorld_pCurrentWorld) {
        sithThing* pPlayer = sithWorld_pCurrentWorld->playerThing;
        if ( pPlayer->type == SITH_THING_PLAYER ) {
            healthNum = pPlayer->actorParams.health;
            shieldsNum = (int32_t)sithInventory_GetBinAmount(pPlayer, SITHBIN_SHIELDS);
            forceNum = (int32_t)sithInventory_GetBinAmount(pPlayer, SITHBIN_FORCEMANA);
            ammoNum = jkHud_GetWeaponAmmo(pPlayer);

            bHasSuperShields = playerThings[playerThingIdx].bHasSuperShields;
            bHasSuperWeapon = playerThings[playerThingIdx].bHasSuperWeapon;
            bHasForceSurge = playerThings[playerThingIdx].bHasForceSurge;
            bHasFieldLight = sithInventory_GetActivate(pPlayer, SITHBIN_FIELDLIGHT);
        }
    }

    char resetConsole[16];
    int consoleX, consoleY;
    consoleGetCursor(NULL, &consoleX, &consoleY);
    snprintf(resetConsole, sizeof(resetConsole)-1, "\x1b[%d;%dH\x1b[97m", consoleY, consoleX);
    stdPlatform_Printf("\x1b[0;0H                                \r\x1b[0;0H\x1b[%d;1m%cHLTH %03d \x1b[%d;1m%cSHLD %03d \x1b[39;0m%c\n                               \n", (bHasSuperShields ? 33 : 31), (bHasSuperShields ? '*' : ' '), healthNum, (bHasSuperShields ? 33 : 32), (bHasSuperShields ? '*' : ' '), shieldsNum, (bHasFieldLight ? '*' : ' '));
    stdPlatform_Printf(resetConsole);
    if (ammoNum < 0) {
        stdPlatform_Printf("\x1b[1;0H                                \r\x1b[1;0H\x1b[%d;1m%cAMMO --- \x1b[%d;1m%cMANA %03d   \n                               \n\x1b[39;0m", (bHasSuperWeapon ? 33 : 39), (bHasSuperWeapon ? '*' : ' '), (bHasForceSurge ? 33 : 39), (bHasForceSurge ? '*' : ' '), forceNum);
    }
    else {
        stdPlatform_Printf("\x1b[1;0H                                \r\x1b[1;0H\x1b[33;%dm%cAMMO %03d \x1b[%d;1m%cMANA %03d   \n                               \n\x1b[39;0m", (bHasSuperWeapon ? 1 : 0), (bHasSuperWeapon ? '*' : ' '), ammoNum, (bHasForceSurge ? 33 : 36), (bHasForceSurge ? '*' : ' '), forceNum);
    }
    stdPlatform_Printf("\x1b[6;0H                                \r");
    stdPlatform_Printf("\x1b[5;0H                                \r");
    stdPlatform_Printf("\x1b[4;0H                                \r");
    stdPlatform_Printf("\x1b[3;0H                                \r");
    stdPlatform_Printf("\x1b[2;0H                                \r\x1b[2;0H");
    
    jkDev_UpdateEntries();
    jkDev_PrintfLog();
    stdPlatform_Printf("\x1b[7;0H                                \r");
    stdPlatform_Printf(resetConsole);
    stdPlatform_Printf("\x1b[10;0H                               \rdlt all=%d mn=%d %d wrld=%d\n                               \r pov=%d hud=%d drw=%d wst=%d %d \n                               \n                               \n", total_delta-std3D_timeWastedWaitingAround, sithMain_tickEndMs-sithMain_tickStartMs, jkGame_Delta_ClearScreen_AdvanceFrame, jkGame_Delta_AdvanceFrame_UpdateCamera, jkGame_Delta_UpdateCamera_DrawPov, jkGame_Delta_DrawPov_HudDrawn, jkGame_Delta_HudDrawn_End - std3D_timeWastedWaitingAround, std3D_timeWastedWaitingAround, sithRender_numSectors);
    stdPlatform_Printf(resetConsole);
    stdPlatform_Printf("\x1b[13;0H                               \r");
    stdPlatform_PrintHeapStats();
    stdPlatform_Printf(resetConsole);
    //world=28 drw=15 emu
    //world=48 drw=33 dsi, 33 down to 25 with jank phys?
#endif

    return result;
}

#ifdef SDL2_RENDER
void jkGame_Screenshot()
{
    //stdPlatform_Printf("TODO: Implement screenshots\n");
    char local_80[128];
    int bVar2 = 0;
    do {
        stdString_snprintf(local_80, sizeof(local_80), "SHOT%04d.PNG", Video_dword_5528B0);
        stdFile_t fp = pHS->fileOpen(local_80, "r");
        if (fp == 0) {
            bVar2 = 1;
        }
        else {
            pHS->fileClose(fp);
        }
        Video_dword_5528B0++;
        if (Video_dword_5528B0 > 9999) {
            bVar2 = 1;
        }
    } while (!bVar2);

    std3D_Screenshot(local_80);
}
#endif

void jkGame_Gamma()
{
    int v0; // eax
    char *v1; // eax

    v0 = ++Video_modeStruct.Video_8606A4;
    if ( Video_modeStruct.Video_8606A4 >= 0xAu )
    {
        v0 = 0;
        Video_modeStruct.Video_8606A4 = 0;
    }
    stdDisplay_GammaCorrect3(v0);
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    stdPalEffects_RefreshPalette();
    if ( Video_modeStruct.b3DAccel )
    {
        v1 = stdDisplay_GetPalette();
        sithRender_SetPalette(v1);
    }
#endif
}

void jkGame_PrecalcViewSizes(int width, int height, jkViewSize *aOut)
{
    flex_d_t v5; // st7
    flex_d_t v6; // st6
    flex_t v7; // [esp+4h] [ebp-Ch]
    flex_t v8;
    flex_t widtha; // [esp+14h] [ebp+4h]
    flex_t widthb; // [esp+14h] [ebp+4h]
    flex_t heighta; // [esp+18h] [ebp+8h]

    v5 = (flex_d_t)width;

    widtha = (flex_t)height;
    heighta = widtha;
    v6 = widtha * 0.5;
    widthb = v5 * 0.5;
    v8 = v6;
    v7 = heighta * 0.36000001;
    aOut[10].xMax = widthb;
    aOut[10].yMax = v6;
    aOut[9].xMax = widthb;
    aOut[9].yMax = v8;
    aOut[10].xMin = width;
    aOut[10].yMin = height;
    aOut[9].xMin = width;
    aOut[9].yMin = height;
    aOut[8].xMin = (__int64)(v5 * 0.9375 - -0.5);
    aOut[8].xMax = widthb;
    aOut[8].yMax = v8;
    aOut[8].yMin = (__int64)(heighta * 0.9375 - -0.5);
    aOut[7].xMin = (__int64)(v5 * 0.875 - -0.5);
    aOut[7].xMax = widthb;
    aOut[7].yMax = v8;
    aOut[7].yMin = (__int64)(heighta * 0.875 - -0.5);
    aOut[6].xMin = (__int64)(v5 * 0.8125 - -0.5);
    aOut[6].xMax = widthb;
    aOut[6].yMax = v8;
    aOut[6].yMin = (__int64)(heighta * 0.8125 - -0.5);
    aOut[5].xMin = (__int64)(v5 * 0.71875 - -0.5);
    aOut[5].xMax = widthb;
    aOut[5].yMax = v7;
    aOut[5].yMin = (__int64)(heighta * 0.71875 - -0.5);
    aOut[4].xMin = (__int64)(v5 * 0.625 - -0.5);
    aOut[4].xMax = widthb;
    aOut[4].yMax = v7;
    aOut[4].yMin = (__int64)(heighta * 0.625 - -0.5);
    aOut[3].xMin = (__int64)(v5 * 0.53125 - -0.5);
    aOut[3].xMax = widthb;
    aOut[3].yMax = v7;
    aOut[3].yMin = (__int64)(heighta * 0.53125 - -0.5);
    aOut[2].xMin = (__int64)(v5 * 0.4375 - -0.5);
    aOut[2].xMax = widthb;
    aOut[2].yMax = v7;
    aOut[2].yMin = (__int64)(heighta * 0.4375 - -0.5);
    aOut[1].xMin = (__int64)(v5 * 0.34375 - -0.5);
    aOut[1].xMax = widthb;
    aOut[1].yMax = v7;
    aOut[1].yMin = (__int64)(heighta * 0.34375 - -0.5);
    aOut->xMin = (__int64)(v5 * 0.25 - -0.5);
    aOut->yMin = (__int64)(heighta * 0.25 - -0.5);
    aOut->xMax = widthb;
    aOut->yMax = v7;
}

void jkGame_ddraw_idk_palettes()
{
    if ( Video_bOpened )
    {
        stdDisplay_VBufferFill(Video_pMenuBuffer, Video_fillColor, 0);
        stdDisplay_DDrawGdiSurfaceFlip();
        stdDisplay_ddraw_surface_flip2();
        stdDisplay_VBufferFill(Video_pMenuBuffer, Video_fillColor, 0);
        sithRender_SetPalette(stdDisplay_GetPalette());
    }
}

void jkGame_nullsub_36()
{
    ;
}
