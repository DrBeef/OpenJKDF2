package org.openjkdf2.vr;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Launcher Activity that checks for storage permission before starting the VR app.
 * This is needed because SDL2 initialization happens in onCreate() and we need
 * to ensure permission is granted before that.
 */
public class LauncherActivity extends Activity {

    private static final String TAG = "OpenJKDF2";
    private static final int REQUEST_MANAGE_ALL_FILES = 2296;
    private static final String GAME_FOLDER = "/sdcard/OpenJKDF2";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.v(TAG, "LauncherActivity::onCreate()");
        super.onCreate(savedInstanceState);

        // Keep screen on during VR
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        checkPermissionAndLaunch();
    }

    private void checkPermissionAndLaunch() {
        if (!Environment.isExternalStorageManager()) {
            Log.v(TAG, "Requesting MANAGE_EXTERNAL_STORAGE permission...");
            // Request for the permission
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            Uri uri = Uri.fromParts("package", getPackageName(), null);
            intent.setData(uri);
            startActivityForResult(intent, REQUEST_MANAGE_ALL_FILES);
        } else {
            Log.v(TAG, "Storage permission granted, launching VR activity...");
            launchVRActivity();
        }
    }

    private void launchVRActivity() {
        // Create game folder and copy assets if needed
        copyAssetsIfNeeded();

        Intent intent = new Intent(this, VRActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(intent);
        finish();
    }

    private void copyAssetsIfNeeded() {
        File gameFolder = new File(GAME_FOLDER);

        // Create the game folder if it doesn't exist
        if (!gameFolder.exists()) {
            Log.v(TAG, "Creating game folder: " + GAME_FOLDER);
            if (!gameFolder.mkdirs()) {
                Log.e(TAG, "Failed to create game folder");
                return;
            }
        }

        // Copy asset folders if they don't already exist
        copyAssetFolderIfNeeded("shaders", GAME_FOLDER + "/shaders");
        copyAssetFolderIfNeeded("resource", GAME_FOLDER + "/resource");
        copyAssetFolderIfNeeded("episode", GAME_FOLDER + "/episode");
    }

    private void copyAssetFolderIfNeeded(String assetFolder, String destPath) {
        File destFolder = new File(destPath);
        if (!destFolder.exists()) {
            Log.v(TAG, "Copying " + assetFolder + " to: " + destPath);
            copyAssetFolder(assetFolder, destPath);
        } else {
            Log.v(TAG, assetFolder + " folder already exists, skipping copy");
        }
    }

    private void copyAssetFolder(String assetFolder, String destPath) {
        AssetManager assetManager = getAssets();
        try {
            String[] files = assetManager.list(assetFolder);
            if (files == null || files.length == 0) {
                Log.e(TAG, "No files found in asset folder: " + assetFolder);
                return;
            }

            // Create destination folder
            File destDir = new File(destPath);
            if (!destDir.exists()) {
                destDir.mkdirs();
            }

            for (String filename : files) {
                String assetPath = assetFolder + "/" + filename;
                String destFilePath = destPath + "/" + filename;
                copyAssetFile(assetPath, destFilePath);
            }
            Log.v(TAG, "Copied " + files.length + " files from " + assetFolder);
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy asset folder: " + assetFolder, e);
        }
    }

    private void copyAssetFile(String assetPath, String destPath) {
        AssetManager assetManager = getAssets();
        InputStream in = null;
        OutputStream out = null;
        try {
            in = assetManager.open(assetPath);
            out = new FileOutputStream(destPath);

            byte[] buffer = new byte[4096];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            Log.v(TAG, "Copied: " + assetPath + " -> " + destPath);
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy asset: " + assetPath, e);
        } finally {
            try {
                if (in != null) in.close();
                if (out != null) out.close();
            } catch (IOException e) {
                // Ignore close errors
            }
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_MANAGE_ALL_FILES) {
            Log.v(TAG, "Returned from permission screen, permission granted: " + Environment.isExternalStorageManager());
            if (Environment.isExternalStorageManager()) {
                launchVRActivity();
            } else {
                // Permission not granted, exit
                Log.v(TAG, "Permission not granted, exiting...");
                finishAffinity();
            }
        }
    }
}
