package com.teambeefvr.jkdf2xr;

import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.view.WindowManager;

/**
 * OpenJKDF2 VR Activity for Meta Quest
 * Extends SDL2's SDLActivity with VR-specific configuration
 */
public class VRActivity extends SDLActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Keep screen on during VR
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        // Note: No libGL.so needed - using native OpenGL ES 3 directly
        // The GLES libraries are system-provided on Quest
        return new String[] {
            "openxr_loader",  // OpenXR loader
            "SDL2",
            "SDL2_mixer",
            "openal",
            "openjkdf2-armv8a"  // Our game library
        };
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    @Override
    protected void onResume() {
        super.onResume();
        // VR apps should run at full performance
    }

    @Override
    protected void onPause() {
        super.onPause();
    }
}
