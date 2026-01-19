// Added: OpenXR platform layer implementation
// Adapted from JKXR patterns for OpenJKDF2

#ifdef PLATFORM_VR

// Include game headers first (they have correct Windows include order)
extern "C" {
#include "stdPlatform.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdVector.h"
#include "Platform/std3D.h"  // Added: for VR FBO override
}

// Define graphics API before including OpenXR
#ifdef _WIN32
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#endif

// Include OpenGL headers (glew includes windows.h properly)
#include <GL/glew.h>
#ifdef _WIN32
#include <GL/wglew.h>
#endif

#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif

#include "stdVR_OpenXR.h"
#include "stdVR.h"
#include "stdVR_Types.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdarg>

// VR log file for debugging (console may not be visible in VR)
static FILE* vrLogFile = nullptr;
static bool vrLogInitialized = false;

// Initialize VR logging immediately
static void VR_InitLog(void)
{
    if (vrLogInitialized) return;
    vrLogInitialized = true;

    // Try multiple locations for the log file
    const char* logPaths[] = {
        "vr_debug.log",  // Current directory (usually game dir)
        "E:/Github/OpenJKDF2/vr_debug.log",
        "C:/Users/ellio/AppData/Local/OpenJKDF2/vr_debug.log",
        NULL
    };

    printf("[VR] Attempting to open log file...\n");
    for (int i = 0; logPaths[i] != NULL; i++) {
        printf("[VR] Trying: %s\n", logPaths[i]);
        vrLogFile = fopen(logPaths[i], "w");
        if (vrLogFile) {
            printf("[VR] SUCCESS: Opened %s\n", logPaths[i]);
            fprintf(vrLogFile, "VR log opened at: %s\n", logPaths[i]);
            fflush(vrLogFile);
            break;
        } else {
            printf("[VR] FAILED to open %s\n", logPaths[i]);
        }
    }

    if (vrLogFile) {
        fprintf(vrLogFile, "=== OpenJKDF2 VR Debug Log ===\n");
        fprintf(vrLogFile, "Log file created successfully\n\n");
        fflush(vrLogFile);
    }
}

extern "C" void VR_Log(const char* fmt, ...)
{
    // Ensure log is initialized
    if (!vrLogInitialized) {
        VR_InitLog();
    }

    va_list args;
    va_start(args, fmt);

    // Print to console
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    stdPlatform_Printf("%s", buffer);

    // Also write to log file
    if (vrLogFile) {
        va_list args2;
        va_start(args2, fmt);
        vfprintf(vrLogFile, fmt, args2);
        // Temporarily re-enabled fflush for debugging motion controls
        fflush(vrLogFile);
        va_end(args2);
    }

    va_end(args);
}

// OpenXR state
static XrInstance xrInstance = XR_NULL_HANDLE;
static XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
static XrSession xrSession = XR_NULL_HANDLE;
static XrSpace xrLocalSpace = XR_NULL_HANDLE;
static XrSpace xrStageSpace = XR_NULL_HANDLE;
static XrSpace xrViewSpace = XR_NULL_HANDLE;
static XrSessionState xrSessionState = XR_SESSION_STATE_UNKNOWN;
static bool xrSessionRunning = false;

// Swapchain state
static XrSwapchain xrSwapchains[STDVR_EYE_COUNT] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
static std::vector<XrSwapchainImageOpenGLKHR> xrSwapchainImages[STDVR_EYE_COUNT];
static uint32_t xrSwapchainImageIndex[STDVR_EYE_COUNT] = { 0, 0 };
static XrSwapchain xrNullSwapchains[STDVR_EYE_COUNT] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
static std::vector<XrSwapchainImageOpenGLKHR> xrNullSwapchainImages[STDVR_EYE_COUNT];
static uint32_t xrNullSwapchainImageIndex[STDVR_EYE_COUNT] = { 0, 0 };
static GLuint vrNullFBO[STDVR_EYE_COUNT] = { 0, 0 };
static XrViewConfigurationView xrConfigViews[STDVR_EYE_COUNT];
static XrView xrViews[STDVR_EYE_COUNT];

// HUD swapchain state (dedicated quad layer for in-game HUD)
static XrSwapchain xrHudSwapchain = XR_NULL_HANDLE;
static std::vector<XrSwapchainImageOpenGLKHR> xrHudSwapchainImages;
static uint32_t xrHudSwapchainImageIndex = 0;
static GLuint vrHudFBO = 0;
static GLuint vrHudDepthTex = 0;
static int vrHudWidth = 1024;
static int vrHudHeight = 768;
static bool vrHudEnabled = true;
static bool vrHudRenderActive = false;
static bool vrHudFrameStarted = false;

static int stdVR_OpenXR_WaitSwapchainImage(XrSwapchain swapchain, const char* label, int eye)
{
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = 1000000000; // 1 second

    XrResult result = xrWaitSwapchainImage(swapchain, &waitInfo);
    int retryCount = 0;
    while (result == XR_TIMEOUT_EXPIRED) {
        retryCount++;
        result = xrWaitSwapchainImage(swapchain, &waitInfo);
    }

    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrWaitSwapchainImage failed for %s eye %d: error %d\n",
            label ? label : "swapchain", eye, result);
        return 0;
    }

    if (retryCount > 0) {
        VR_Log("stdVR_OpenXR: xrWaitSwapchainImage retried %d times for %s eye %d\n",
            retryCount, label ? label : "swapchain", eye);
    }

    return 1;
}

static void stdVR_OpenXR_ReleaseSwapchainImageForSwapchain(XrSwapchain swapchain, const char* label, int eye)
{
    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XrResult result = xrReleaseSwapchainImage(swapchain, &releaseInfo);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrReleaseSwapchainImage failed for %s eye %d: error %d\n",
            label ? label : "swapchain", eye, result);
    }
}

static void stdVR_OpenXR_ReleaseSwapchainImage(int eye)
{
    stdVR_OpenXR_ReleaseSwapchainImageForSwapchain(xrSwapchains[eye], "main", eye);
}

// Track null FBO validation (only validate once)
static bool vrNullFBOValidated[STDVR_EYE_COUNT] = { false, false };

// Manual FBO/viewport state tracking to avoid glGetIntegerv (GPU stalls)
// Declared early so stdVR_OpenXR_ClearNullSwapchain can use them
static GLint trackedFBO = 0;
static GLint trackedViewport[4] = { 0, 0, 640, 480 };
static bool stateTrackingActive = false;

static int stdVR_OpenXR_ClearNullSwapchain(int eye)
{
    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        return 0;
    }
    if (xrNullSwapchains[eye] == XR_NULL_HANDLE || vrNullFBO[eye] == 0 || xrNullSwapchainImages[eye].empty()) {
        return 0;
    }

    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrResult result = xrAcquireSwapchainImage(xrNullSwapchains[eye], &acquireInfo, &xrNullSwapchainImageIndex[eye]);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrAcquireSwapchainImage failed for null eye %d: error %d\n", eye, result);
        return 0;
    }

    if (!stdVR_OpenXR_WaitSwapchainImage(xrNullSwapchains[eye], "null", eye)) {
        stdVR_OpenXR_ReleaseSwapchainImageForSwapchain(xrNullSwapchains[eye], "null", eye);
        return 0;
    }

    // OPTIMIZATION: Use tracked state instead of GPU queries
    GLint savedFBO = stateTrackingActive ? trackedFBO : 0;
    GLint savedViewport[4];
    if (stateTrackingActive) {
        memcpy(savedViewport, trackedViewport, sizeof(savedViewport));
    } else {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
        glGetIntegerv(GL_VIEWPORT, savedViewport);
    }

    GLuint texture = xrNullSwapchainImages[eye][xrNullSwapchainImageIndex[eye]].image;
    glBindFramebuffer(GL_FRAMEBUFFER, vrNullFBO[eye]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // OPTIMIZATION: Only validate FBO on first use
    if (!vrNullFBOValidated[eye]) {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            VR_Log("stdVR_OpenXR: Null FBO incomplete for eye %d, status 0x%x\n", eye, status);
            glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);
            stdVR_OpenXR_ReleaseSwapchainImageForSwapchain(xrNullSwapchains[eye], "null", eye);
            return 0;
        }
        vrNullFBOValidated[eye] = true;
    }

    int width = xrConfigViews[eye].recommendedImageRectWidth;
    int height = xrConfigViews[eye].recommendedImageRectHeight;
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_FRAMEBUFFER_SRGB);

    // Detach the texture before releasing (mirrors main swapchain handling)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

    // Update tracked state
    if (stateTrackingActive) {
        trackedFBO = savedFBO;
        memcpy(trackedViewport, savedViewport, sizeof(trackedViewport));
    }

    stdVR_OpenXR_ReleaseSwapchainImageForSwapchain(xrNullSwapchains[eye], "null", eye);
    return 1;
}

// Frame state
static XrFrameState xrFrameState = {};
static bool xrFrameInProgress = false;
static int vrFrameCount = 0;  // Track frame count for debugging

// VR FBO state for rendering (per-swapchain-image, like JKXR)
static std::vector<GLuint> vrFBO[STDVR_EYE_COUNT];
static std::vector<GLuint> vrDepthTex[STDVR_EYE_COUNT];
static std::vector<bool> vrFBOValidated[STDVR_EYE_COUNT];  // Added: track which FBOs have been validated
static GLuint vrCurrentFBO[STDVR_EYE_COUNT] = { 0, 0 };
static GLint previousFBO = 0;
static GLint previousViewport[4] = { 0, 0, 0, 0 };

// Track current eye being rendered (for debugging)
// Using extern "C" so C code can reference this variable
extern "C" int stdVR_currentEye = -1;  // -1 = not in eye rendering, 0 = left, 1 = right

// Input state
static XrActionSet xrActionSet = XR_NULL_HANDLE;
static XrAction xrPoseAction = XR_NULL_HANDLE;
static XrAction xrTriggerAction = XR_NULL_HANDLE;
static XrAction xrGripAction = XR_NULL_HANDLE;
static XrAction xrThumbstickAction = XR_NULL_HANDLE;
static XrAction xrButtonAAction = XR_NULL_HANDLE;
static XrAction xrButtonBAction = XR_NULL_HANDLE;
static XrAction xrButtonXAction = XR_NULL_HANDLE;
static XrAction xrButtonYAction = XR_NULL_HANDLE;
static XrAction xrMenuAction = XR_NULL_HANDLE;
static XrAction xrHapticAction = XR_NULL_HANDLE;
static XrSpace xrControllerSpaces[STDVR_CONTROLLER_COUNT] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
static XrPath xrHandPaths[STDVR_CONTROLLER_COUNT];

// Runtime info
static char xrRuntimeName[XR_MAX_RUNTIME_NAME_SIZE] = "Unknown";

// Helper macros
#define XR_CHECK(call) \
    do { \
        XrResult result = (call); \
        if (XR_FAILED(result)) { \
            VR_Log("OpenXR error: %s returned %d at %s:%d\n", #call, result, __FILE__, __LINE__); \
            return 0; \
        } \
    } while (0)

#define XR_CHECK_VOID(call) \
    do { \
        XrResult result = (call); \
        if (XR_FAILED(result)) { \
            VR_Log("OpenXR error: %s returned %d at %s:%d\n", #call, result, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

// Convert XrQuaternionf to Euler angles (Pitch, Yaw, Roll in degrees)
static void QuatToEuler(const XrQuaternionf* q, rdVector3* euler)
{
    // Convert quaternion to Euler angles
    float sinr_cosp = 2.0f * (q->w * q->x + q->y * q->z);
    float cosr_cosp = 1.0f - 2.0f * (q->x * q->x + q->y * q->y);
    euler->z = std::atan2(sinr_cosp, cosr_cosp) * (180.0f / 3.14159265f); // Roll

    float sinp = 2.0f * (q->w * q->y - q->z * q->x);
    if (std::abs(sinp) >= 1.0f) {
        euler->x = std::copysign(90.0f, sinp); // Pitch
    } else {
        euler->x = std::asin(sinp) * (180.0f / 3.14159265f);
    }

    float siny_cosp = 2.0f * (q->w * q->z + q->x * q->y);
    float cosy_cosp = 1.0f - 2.0f * (q->y * q->y + q->z * q->z);
    euler->y = std::atan2(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f); // Yaw
}

// Convert XrPosef to rdMatrix34
// OpenXR coordinate system: +X=right, +Y=up, +Z=backward (toward user)
// JKDF2 coordinate system:  +X=right, +Y=forward, +Z=up
// Conversion: JKDF2.x = OpenXR.x, JKDF2.y = -OpenXR.z, JKDF2.z = OpenXR.y
static void PoseToMatrix(const XrPosef* pose, rdMatrix34* mat)
{
    // Convert quaternion to rotation matrix in OpenXR space first
    const XrQuaternionf* q = &pose->orientation;
    float xx = q->x * q->x;
    float yy = q->y * q->y;
    float zz = q->z * q->z;
    float xy = q->x * q->y;
    float xz = q->x * q->z;
    float yz = q->y * q->z;
    float wx = q->w * q->x;
    float wy = q->w * q->y;
    float wz = q->w * q->z;

    // OpenXR rotation matrix (column vectors: right, up, back)
    float xr_rvec_x = 1.0f - 2.0f * (yy + zz);
    float xr_rvec_y = 2.0f * (xy + wz);
    float xr_rvec_z = 2.0f * (xz - wy);

    float xr_uvec_x = 2.0f * (xy - wz);
    float xr_uvec_y = 1.0f - 2.0f * (xx + zz);
    float xr_uvec_z = 2.0f * (yz + wx);

    float xr_bvec_x = 2.0f * (xz + wy);
    float xr_bvec_y = 2.0f * (yz - wx);
    float xr_bvec_z = 1.0f - 2.0f * (xx + yy);

    // Convert to JKDF2 coordinate system
    // JKDF2 rvec (right) = OpenXR rvec with Y/Z swapped and Z negated
    mat->rvec.x = xr_rvec_x;
    mat->rvec.y = -xr_rvec_z;
    mat->rvec.z = xr_rvec_y;

    // JKDF2 lvec (forward) = OpenXR -bvec (negative backward = forward) with Y/Z swapped
    mat->lvec.x = -xr_bvec_x;
    mat->lvec.y = xr_bvec_z;
    mat->lvec.z = -xr_bvec_y;

    // JKDF2 uvec (up) = OpenXR uvec with Y/Z swapped and Z negated
    mat->uvec.x = xr_uvec_x;
    mat->uvec.y = -xr_uvec_z;
    mat->uvec.z = xr_uvec_y;

    // Convert position: JKDF2.x = OpenXR.x, JKDF2.y = -OpenXR.z, JKDF2.z = OpenXR.y
    mat->scale.x = pose->position.x;
    mat->scale.y = -pose->position.z;
    mat->scale.z = pose->position.y;
}

// Create OpenXR instance
extern "C" int stdVR_OpenXR_Init(void)
{
    // Initialize log file immediately
    VR_InitLog();
    VR_Log("stdVR_OpenXR: Initializing OpenXR...\n");
    VR_Log("stdVR_OpenXR: Build timestamp: %s %s\n", __DATE__, __TIME__);

    // Get available extensions
    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);

    std::vector<XrExtensionProperties> extensions(extensionCount, { XR_TYPE_EXTENSION_PROPERTIES });
    xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());

    // Check for OpenGL extension
    bool hasOpenGL = false;
    VR_Log("stdVR_OpenXR: Available extensions:\n");
    for (const auto& ext : extensions) {
        // Print graphics-related extensions for debugging
        if (strstr(ext.extensionName, "enable") || strstr(ext.extensionName, "graphics")) {
            stdPlatform_Printf("  - %s\n", ext.extensionName);
        }
        if (strcmp(ext.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) {
            hasOpenGL = true;
        }
    }

    if (!hasOpenGL) {
        VR_Log("stdVR_OpenXR: ERROR - %s not supported by this runtime\n", XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
        VR_Log("stdVR_OpenXR: OpenJKDF2 VR requires an OpenXR runtime with OpenGL support.\n");
        VR_Log("stdVR_OpenXR: Compatible runtimes:\n");
        stdPlatform_Printf("  - SteamVR (recommended)\n");
        stdPlatform_Printf("  - Oculus runtime (with actual headset)\n");
        stdPlatform_Printf("  - Monado\n");
        VR_Log("stdVR_OpenXR: Note: Meta XR Simulator does not support OpenGL.\n");
        return 0;
    }

    // Create instance
    const char* enabledExtensions[] = {
        XR_KHR_OPENGL_ENABLE_EXTENSION_NAME
    };

    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy(createInfo.applicationInfo.applicationName, "OpenJKDF2");
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy(createInfo.applicationInfo.engineName, "OpenJKDF2");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = 1;
    createInfo.enabledExtensionNames = enabledExtensions;

    XrResult result = xrCreateInstance(&createInfo, &xrInstance);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: Failed to create instance (error %d)\n", result);
        VR_Log("stdVR_OpenXR: Make sure SteamVR or another compatible OpenXR runtime is running.\n");
        return 0;
    }

    // Get instance properties
    XrInstanceProperties instanceProps = { XR_TYPE_INSTANCE_PROPERTIES };
    xrGetInstanceProperties(xrInstance, &instanceProps);
    strncpy(xrRuntimeName, instanceProps.runtimeName, sizeof(xrRuntimeName) - 1);
    VR_Log("stdVR_OpenXR: Runtime: %s\n", xrRuntimeName);

    // Get system
    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK(xrGetSystem(xrInstance, &systemInfo, &xrSystemId));

    // Get view configuration
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    if (viewCount != STDVR_EYE_COUNT) {
        VR_Log("stdVR_OpenXR: Unexpected view count: %d\n", viewCount);
        return 0;
    }

    for (int i = 0; i < STDVR_EYE_COUNT; i++) {
        xrConfigViews[i] = { XR_TYPE_VIEW_CONFIGURATION_VIEW };
    }
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, STDVR_EYE_COUNT, &viewCount, xrConfigViews);

    // Store recommended render size
    stdVR_clientInfo.renderWidth = xrConfigViews[0].recommendedImageRectWidth;
    stdVR_clientInfo.renderHeight = xrConfigViews[0].recommendedImageRectHeight;
    VR_Log("stdVR_OpenXR: Recommended render size: %dx%d\n", stdVR_clientInfo.renderWidth, stdVR_clientInfo.renderHeight);
    std3D_SetVRTargetSize(stdVR_clientInfo.renderWidth, stdVR_clientInfo.renderHeight);

    return 1;
}

extern "C" void stdVR_OpenXR_Shutdown(void)
{
    VR_Log("stdVR_OpenXR: Shutting down...\n");
    if (xrInstance != XR_NULL_HANDLE) {
        xrDestroyInstance(xrInstance);
        xrInstance = XR_NULL_HANDLE;
    }
    xrSystemId = XR_NULL_SYSTEM_ID;

    // Close log file
    if (vrLogFile) {
        VR_Log("stdVR_OpenXR: Shutdown complete, closing log\n");
        fclose(vrLogFile);
        vrLogFile = nullptr;
    }
}

static int CreateActionSet(void)
{
    // Create action set
    XrActionSetCreateInfo actionSetInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy(actionSetInfo.actionSetName, "gameplay");
    strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    XR_CHECK(xrCreateActionSet(xrInstance, &actionSetInfo, &xrActionSet));

    // Get hand paths
    xrStringToPath(xrInstance, "/user/hand/left", &xrHandPaths[STDVR_CONTROLLER_LEFT]);
    xrStringToPath(xrInstance, "/user/hand/right", &xrHandPaths[STDVR_CONTROLLER_RIGHT]);

    // Create pose action
    XrActionCreateInfo actionInfo = { XR_TYPE_ACTION_CREATE_INFO };
    strcpy(actionInfo.actionName, "hand_pose");
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    actionInfo.countSubactionPaths = STDVR_CONTROLLER_COUNT;
    actionInfo.subactionPaths = xrHandPaths;
    strcpy(actionInfo.localizedActionName, "Hand Pose");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrPoseAction));

    // Create trigger action
    strcpy(actionInfo.actionName, "trigger");
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy(actionInfo.localizedActionName, "Trigger");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrTriggerAction));

    // Create grip action
    strcpy(actionInfo.actionName, "grip");
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy(actionInfo.localizedActionName, "Grip");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrGripAction));

    // Create thumbstick action
    strcpy(actionInfo.actionName, "thumbstick");
    actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    strcpy(actionInfo.localizedActionName, "Thumbstick");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrThumbstickAction));

    // Create button actions (no subaction paths for these)
    actionInfo.countSubactionPaths = 0;
    actionInfo.subactionPaths = nullptr;
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

    strcpy(actionInfo.actionName, "button_a");
    strcpy(actionInfo.localizedActionName, "A Button");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrButtonAAction));

    strcpy(actionInfo.actionName, "button_b");
    strcpy(actionInfo.localizedActionName, "B Button");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrButtonBAction));

    strcpy(actionInfo.actionName, "button_x");
    strcpy(actionInfo.localizedActionName, "X Button");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrButtonXAction));

    strcpy(actionInfo.actionName, "button_y");
    strcpy(actionInfo.localizedActionName, "Y Button");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrButtonYAction));

    strcpy(actionInfo.actionName, "menu");
    strcpy(actionInfo.localizedActionName, "Menu");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrMenuAction));

    // Create haptic action
    actionInfo.countSubactionPaths = STDVR_CONTROLLER_COUNT;
    actionInfo.subactionPaths = xrHandPaths;
    strcpy(actionInfo.actionName, "haptic");
    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy(actionInfo.localizedActionName, "Haptic");
    XR_CHECK(xrCreateAction(xrActionSet, &actionInfo, &xrHapticAction));

    // Suggest bindings for Oculus Touch
    XrPath oculusTouchPath;
    xrStringToPath(xrInstance, "/interaction_profiles/oculus/touch_controller", &oculusTouchPath);

    std::vector<XrActionSuggestedBinding> bindings;
    XrPath path;

    // Left controller
    xrStringToPath(xrInstance, "/user/hand/left/input/grip/pose", &path);
    bindings.push_back({ xrPoseAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/input/trigger/value", &path);
    bindings.push_back({ xrTriggerAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/input/squeeze/value", &path);
    bindings.push_back({ xrGripAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/input/thumbstick", &path);
    bindings.push_back({ xrThumbstickAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/input/x/click", &path);
    bindings.push_back({ xrButtonXAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/input/y/click", &path);
    bindings.push_back({ xrButtonYAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/input/menu/click", &path);
    bindings.push_back({ xrMenuAction, path });
    xrStringToPath(xrInstance, "/user/hand/left/output/haptic", &path);
    bindings.push_back({ xrHapticAction, path });

    // Right controller
    xrStringToPath(xrInstance, "/user/hand/right/input/grip/pose", &path);
    bindings.push_back({ xrPoseAction, path });
    xrStringToPath(xrInstance, "/user/hand/right/input/trigger/value", &path);
    bindings.push_back({ xrTriggerAction, path });
    xrStringToPath(xrInstance, "/user/hand/right/input/squeeze/value", &path);
    bindings.push_back({ xrGripAction, path });
    xrStringToPath(xrInstance, "/user/hand/right/input/thumbstick", &path);
    bindings.push_back({ xrThumbstickAction, path });
    xrStringToPath(xrInstance, "/user/hand/right/input/a/click", &path);
    bindings.push_back({ xrButtonAAction, path });
    xrStringToPath(xrInstance, "/user/hand/right/input/b/click", &path);
    bindings.push_back({ xrButtonBAction, path });
    xrStringToPath(xrInstance, "/user/hand/right/output/haptic", &path);
    bindings.push_back({ xrHapticAction, path });

    XrInteractionProfileSuggestedBinding suggestedBindings = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    suggestedBindings.interactionProfile = oculusTouchPath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);

    // Valve Index Controller bindings
    XrPath indexPath;
    if (XR_SUCCEEDED(xrStringToPath(xrInstance, "/interaction_profiles/valve/index_controller", &indexPath))) {
        bindings.clear();

        // Left controller
        xrStringToPath(xrInstance, "/user/hand/left/input/grip/pose", &path);
        bindings.push_back({ xrPoseAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/trigger/value", &path);
        bindings.push_back({ xrTriggerAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/squeeze/value", &path);
        bindings.push_back({ xrGripAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/thumbstick", &path);
        bindings.push_back({ xrThumbstickAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/a/click", &path);  // Index has A on left
        bindings.push_back({ xrButtonXAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/b/click", &path);  // Index has B on left
        bindings.push_back({ xrButtonYAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/output/haptic", &path);
        bindings.push_back({ xrHapticAction, path });

        // Right controller
        xrStringToPath(xrInstance, "/user/hand/right/input/grip/pose", &path);
        bindings.push_back({ xrPoseAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/trigger/value", &path);
        bindings.push_back({ xrTriggerAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/squeeze/value", &path);
        bindings.push_back({ xrGripAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/thumbstick", &path);
        bindings.push_back({ xrThumbstickAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/a/click", &path);
        bindings.push_back({ xrButtonAAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/b/click", &path);
        bindings.push_back({ xrButtonBAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/output/haptic", &path);
        bindings.push_back({ xrHapticAction, path });

        suggestedBindings.interactionProfile = indexPath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
    }

    // HTC Vive Controller bindings
    XrPath vivePath;
    if (XR_SUCCEEDED(xrStringToPath(xrInstance, "/interaction_profiles/htc/vive_controller", &vivePath))) {
        bindings.clear();

        // Left controller - Vive uses trackpad instead of thumbstick
        xrStringToPath(xrInstance, "/user/hand/left/input/grip/pose", &path);
        bindings.push_back({ xrPoseAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/trigger/value", &path);
        bindings.push_back({ xrTriggerAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/squeeze/click", &path);  // Vive grip is a button
        bindings.push_back({ xrGripAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/trackpad", &path);  // Use trackpad as thumbstick
        bindings.push_back({ xrThumbstickAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/trackpad/click", &path);
        bindings.push_back({ xrButtonXAction, path });  // Trackpad click = X
        xrStringToPath(xrInstance, "/user/hand/left/input/menu/click", &path);
        bindings.push_back({ xrMenuAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/output/haptic", &path);
        bindings.push_back({ xrHapticAction, path });

        // Right controller
        xrStringToPath(xrInstance, "/user/hand/right/input/grip/pose", &path);
        bindings.push_back({ xrPoseAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/trigger/value", &path);
        bindings.push_back({ xrTriggerAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/squeeze/click", &path);
        bindings.push_back({ xrGripAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/trackpad", &path);
        bindings.push_back({ xrThumbstickAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/trackpad/click", &path);
        bindings.push_back({ xrButtonAAction, path });  // Trackpad click = A
        xrStringToPath(xrInstance, "/user/hand/right/input/menu/click", &path);
        bindings.push_back({ xrMenuAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/output/haptic", &path);
        bindings.push_back({ xrHapticAction, path });

        suggestedBindings.interactionProfile = vivePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
    }

    // Simple controller fallback (Khronos simple controller)
    XrPath simplePath;
    if (XR_SUCCEEDED(xrStringToPath(xrInstance, "/interaction_profiles/khr/simple_controller", &simplePath))) {
        bindings.clear();

        // Left controller - only has select (trigger) and menu
        xrStringToPath(xrInstance, "/user/hand/left/input/grip/pose", &path);
        bindings.push_back({ xrPoseAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/select/click", &path);
        bindings.push_back({ xrTriggerAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/input/menu/click", &path);
        bindings.push_back({ xrMenuAction, path });
        xrStringToPath(xrInstance, "/user/hand/left/output/haptic", &path);
        bindings.push_back({ xrHapticAction, path });

        // Right controller
        xrStringToPath(xrInstance, "/user/hand/right/input/grip/pose", &path);
        bindings.push_back({ xrPoseAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/select/click", &path);
        bindings.push_back({ xrTriggerAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/input/menu/click", &path);
        bindings.push_back({ xrMenuAction, path });
        xrStringToPath(xrInstance, "/user/hand/right/output/haptic", &path);
        bindings.push_back({ xrHapticAction, path });

        suggestedBindings.interactionProfile = simplePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
    }

    VR_Log("stdVR_OpenXR: Controller bindings configured for Oculus Touch, Valve Index, HTC Vive, and simple controllers\n");
    return 1;
}

static void DestroyActionSet(void)
{
    for (int i = 0; i < STDVR_CONTROLLER_COUNT; i++) {
        if (xrControllerSpaces[i] != XR_NULL_HANDLE) {
            xrDestroySpace(xrControllerSpaces[i]);
            xrControllerSpaces[i] = XR_NULL_HANDLE;
        }
    }

    if (xrActionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(xrActionSet);
        xrActionSet = XR_NULL_HANDLE;
    }
}

extern "C" int stdVR_OpenXR_CreateSession(void* pGLContext)
{
    if (xrSession != XR_NULL_HANDLE) {
        return 1; // Already created
    }

    VR_Log("stdVR_OpenXR: Creating session...\n");

    // Check OpenGL requirements
    XrGraphicsRequirementsOpenGLKHR glReqs = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
    PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(xrInstance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetOpenGLGraphicsRequirementsKHR);
    if (xrGetOpenGLGraphicsRequirementsKHR) {
        XrResult reqResult = xrGetOpenGLGraphicsRequirementsKHR(xrInstance, xrSystemId, &glReqs);
        if (XR_SUCCEEDED(reqResult)) {
            VR_Log("stdVR_OpenXR: GL requirements - min version: %d.%d.%d, max version: %d.%d.%d\n",
                XR_VERSION_MAJOR(glReqs.minApiVersionSupported),
                XR_VERSION_MINOR(glReqs.minApiVersionSupported),
                XR_VERSION_PATCH(glReqs.minApiVersionSupported),
                XR_VERSION_MAJOR(glReqs.maxApiVersionSupported),
                XR_VERSION_MINOR(glReqs.maxApiVersionSupported),
                XR_VERSION_PATCH(glReqs.maxApiVersionSupported));
        }
    }

    // Create session with OpenGL binding
    XrGraphicsBindingOpenGLWin32KHR glBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
    glBinding.hDC = wglGetCurrentDC();
    glBinding.hGLRC = (HGLRC)pGLContext;

    XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionInfo.next = &glBinding;
    sessionInfo.systemId = xrSystemId;

    XrResult sessionResult = xrCreateSession(xrInstance, &sessionInfo, &xrSession);
    if (XR_FAILED(sessionResult)) {
        VR_Log("OpenXR error: xrCreateSession returned %d\n", sessionResult);
        return 0;
    }

    // From here on, if anything fails we need to clean up the session
    XrResult result;

    // Create reference spaces
    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;

    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    result = xrCreateReferenceSpace(xrSession, &spaceInfo, &xrLocalSpace);
    if (XR_FAILED(result)) {
        VR_Log("OpenXR error: xrCreateReferenceSpace(LOCAL) returned %d\n", result);
        goto cleanup_session;
    }

    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    result = xrCreateReferenceSpace(xrSession, &spaceInfo, &xrViewSpace);
    if (XR_FAILED(result)) {
        VR_Log("OpenXR error: xrCreateReferenceSpace(VIEW) returned %d\n", result);
        goto cleanup_session;
    }

    // Try to create stage space, fall back to local if not available
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    if (XR_FAILED(xrCreateReferenceSpace(xrSession, &spaceInfo, &xrStageSpace))) {
        xrStageSpace = xrLocalSpace;
    }

    {
        // Enumerate supported swapchain formats
        uint32_t formatCount = 0;
        xrEnumerateSwapchainFormats(xrSession, 0, &formatCount, nullptr);
        std::vector<int64_t> formats(formatCount);
        xrEnumerateSwapchainFormats(xrSession, formatCount, &formatCount, formats.data());

        // Find the best format - prefer sRGB
        int64_t selectedFormat = GL_RGBA8;  // Fallback
        VR_Log("stdVR_OpenXR: Available swapchain formats:\n");
        for (uint32_t i = 0; i < formatCount; i++) {
            VR_Log("  - 0x%llX\n", (long long)formats[i]);
            // Prefer GL_SRGB8_ALPHA8 for correct color handling
            if (formats[i] == GL_SRGB8_ALPHA8) {
                selectedFormat = GL_SRGB8_ALPHA8;
            }
            // GL_RGBA8 is acceptable if sRGB not available
            else if (formats[i] == GL_RGBA8 && selectedFormat != GL_SRGB8_ALPHA8) {
                selectedFormat = GL_RGBA8;
            }
        }
        VR_Log("stdVR_OpenXR: Selected swapchain format: 0x%llX\n", (long long)selectedFormat);

        // Create swapchains
        for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
            XrSwapchainCreateInfo swapchainInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
            swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            swapchainInfo.format = selectedFormat;
            swapchainInfo.sampleCount = 1;
            swapchainInfo.width = xrConfigViews[eye].recommendedImageRectWidth;
            swapchainInfo.height = xrConfigViews[eye].recommendedImageRectHeight;
            swapchainInfo.faceCount = 1;
            swapchainInfo.arraySize = 1;
            swapchainInfo.mipCount = 1;

            result = xrCreateSwapchain(xrSession, &swapchainInfo, &xrSwapchains[eye]);
            if (XR_FAILED(result)) {
                VR_Log("OpenXR error: xrCreateSwapchain(%d) returned %d\n", eye, result);
                goto cleanup_session;
            }

            // Get swapchain images
            uint32_t imageCount = 0;
            xrEnumerateSwapchainImages(xrSwapchains[eye], 0, &imageCount, nullptr);
            xrSwapchainImages[eye].resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
            xrEnumerateSwapchainImages(xrSwapchains[eye], imageCount, &imageCount, (XrSwapchainImageBaseHeader*)xrSwapchainImages[eye].data());

            // Create a null swapchain per-eye for screen-layer projection
            result = xrCreateSwapchain(xrSession, &swapchainInfo, &xrNullSwapchains[eye]);
            if (XR_FAILED(result)) {
                VR_Log("OpenXR error: xrCreateSwapchain(null %d) returned %d\n", eye, result);
                goto cleanup_session;
            }

            uint32_t nullImageCount = 0;
            xrEnumerateSwapchainImages(xrNullSwapchains[eye], 0, &nullImageCount, nullptr);
            xrNullSwapchainImages[eye].resize(nullImageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
            xrEnumerateSwapchainImages(xrNullSwapchains[eye], nullImageCount, &nullImageCount, (XrSwapchainImageBaseHeader*)xrNullSwapchainImages[eye].data());
        }
    }

    // Initialize views
    for (int i = 0; i < STDVR_EYE_COUNT; i++) {
        xrViews[i] = { XR_TYPE_VIEW };
    }

    // Create per-swapchain-image FBOs and depth textures (mirrors JKXR approach)
    // OPTIMIZATION: Keep depth textures permanently attached, only swap color per-frame
    for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
        const uint32_t imageCount = (uint32_t)xrSwapchainImages[eye].size();
        const int width = xrConfigViews[eye].recommendedImageRectWidth;
        const int height = xrConfigViews[eye].recommendedImageRectHeight;

        vrFBO[eye].resize(imageCount);
        vrDepthTex[eye].resize(imageCount);
        vrFBOValidated[eye].resize(imageCount, false);  // Added: track validation state

        if (imageCount > 0) {
            glGenFramebuffers(imageCount, vrFBO[eye].data());
            glGenTextures(imageCount, vrDepthTex[eye].data());
        }

        for (uint32_t i = 0; i < imageCount; i++) {
            // Create depth texture for this swapchain image
            glBindTexture(GL_TEXTURE_2D, vrDepthTex[eye][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32,
                width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            // Bind FBO and attach depth permanently (color is swapped per-frame)
            glBindFramebuffer(GL_FRAMEBUFFER, vrFBO[eye][i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                vrDepthTex[eye][i], 0);

            // Note: Color attachment validation happens on first use per swapchain image
            // We don't detach color here - it will be attached when PrepareEyeBuffer is called
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Create a lightweight FBO for null swapchain clears
        glGenFramebuffers(1, &vrNullFBO[eye]);
    }

    VR_Log("stdVR_OpenXR: VR FBOs created - eye0 images=%zu, eye1 images=%zu, nullFBO[0]=%u, nullFBO[1]=%u\n",
        vrFBO[0].size(), vrFBO[1].size(), vrNullFBO[0], vrNullFBO[1]);

    // Create HUD swapchain (dedicated quad layer for in-game HUD)
    {
        XrSwapchainCreateInfo hudSwapchainInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        hudSwapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        hudSwapchainInfo.format = GL_RGBA8;  // Use RGBA8 for HUD (non-sRGB for clean UI)
        hudSwapchainInfo.sampleCount = 1;
        hudSwapchainInfo.width = vrHudWidth;
        hudSwapchainInfo.height = vrHudHeight;
        hudSwapchainInfo.faceCount = 1;
        hudSwapchainInfo.arraySize = 1;
        hudSwapchainInfo.mipCount = 1;

        result = xrCreateSwapchain(xrSession, &hudSwapchainInfo, &xrHudSwapchain);
        if (XR_FAILED(result)) {
            VR_Log("stdVR_OpenXR: WARNING - xrCreateSwapchain(HUD) returned %d, HUD disabled\n", result);
            vrHudEnabled = false;
        } else {
            uint32_t hudImageCount = 0;
            xrEnumerateSwapchainImages(xrHudSwapchain, 0, &hudImageCount, nullptr);
            xrHudSwapchainImages.resize(hudImageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
            xrEnumerateSwapchainImages(xrHudSwapchain, hudImageCount, &hudImageCount,
                (XrSwapchainImageBaseHeader*)xrHudSwapchainImages.data());

            // Create HUD FBO
            glGenFramebuffers(1, &vrHudFBO);
            glGenTextures(1, &vrHudDepthTex);

            // Create depth texture for HUD FBO
            glBindTexture(GL_TEXTURE_2D, vrHudDepthTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, vrHudWidth, vrHudHeight, 0,
                GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

            glBindFramebuffer(GL_FRAMEBUFFER, vrHudFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, vrHudDepthTex, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);

            VR_Log("stdVR_OpenXR: HUD swapchain created - %dx%d, %u images, FBO=%u\n",
                vrHudWidth, vrHudHeight, hudImageCount, vrHudFBO);
            vrHudEnabled = true;
        }
    }

    // Create action set
    if (!CreateActionSet()) {
        VR_Log("stdVR_OpenXR: Failed to create action set\n");
        goto cleanup_session;
    }

    // Attach action set to session
    {
        XrSessionActionSetsAttachInfo attachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &xrActionSet;
        result = xrAttachSessionActionSets(xrSession, &attachInfo);
        if (XR_FAILED(result)) {
            VR_Log("OpenXR error: xrAttachSessionActionSets returned %d\n", result);
            goto cleanup_session;
        }
    }

    // Create controller spaces
    for (int i = 0; i < STDVR_CONTROLLER_COUNT; i++) {
        XrActionSpaceCreateInfo spaceCreateInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
        spaceCreateInfo.action = xrPoseAction;
        spaceCreateInfo.poseInActionSpace.orientation.w = 1.0f;
        spaceCreateInfo.subactionPath = xrHandPaths[i];
        result = xrCreateActionSpace(xrSession, &spaceCreateInfo, &xrControllerSpaces[i]);
        if (XR_FAILED(result)) {
            VR_Log("OpenXR error: xrCreateActionSpace(%d) returned %d\n", i, result);
            goto cleanup_session;
        }
    }

    VR_Log("stdVR_OpenXR: Session created successfully\n");
    return 1;

cleanup_session:
    VR_Log("stdVR_OpenXR: Session creation failed, cleaning up\n");
    stdVR_OpenXR_DestroySession();
    return 0;
}

extern "C" void stdVR_OpenXR_DestroySession(void)
{
    xrSessionRunning = false;
    stdVR_clientInfo.bSessionRunning = 0;

    DestroyActionSet();

    // Destroy VR FBOs and reset validation state
    for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
        if (!vrFBO[eye].empty()) {
            glDeleteFramebuffers((GLsizei)vrFBO[eye].size(), vrFBO[eye].data());
            vrFBO[eye].clear();
        }
        if (!vrDepthTex[eye].empty()) {
            glDeleteTextures((GLsizei)vrDepthTex[eye].size(), vrDepthTex[eye].data());
            vrDepthTex[eye].clear();
        }
        vrFBOValidated[eye].clear();  // Reset validation flags
        if (vrNullFBO[eye] != 0) {
            glDeleteFramebuffers(1, &vrNullFBO[eye]);
            vrNullFBO[eye] = 0;
        }
        vrNullFBOValidated[eye] = false;  // Reset null FBO validation
        vrCurrentFBO[eye] = 0;
    }
    stateTrackingActive = false;  // Reset state tracking

    for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
        if (xrSwapchains[eye] != XR_NULL_HANDLE) {
            xrDestroySwapchain(xrSwapchains[eye]);
            xrSwapchains[eye] = XR_NULL_HANDLE;
        }
        xrSwapchainImages[eye].clear();

        if (xrNullSwapchains[eye] != XR_NULL_HANDLE) {
            xrDestroySwapchain(xrNullSwapchains[eye]);
            xrNullSwapchains[eye] = XR_NULL_HANDLE;
        }
        xrNullSwapchainImages[eye].clear();
    }

    // Destroy HUD swapchain
    if (vrHudFBO != 0) {
        glDeleteFramebuffers(1, &vrHudFBO);
        vrHudFBO = 0;
    }
    if (vrHudDepthTex != 0) {
        glDeleteTextures(1, &vrHudDepthTex);
        vrHudDepthTex = 0;
    }
    if (xrHudSwapchain != XR_NULL_HANDLE) {
        xrDestroySwapchain(xrHudSwapchain);
        xrHudSwapchain = XR_NULL_HANDLE;
    }
    xrHudSwapchainImages.clear();
    vrHudEnabled = false;
    vrHudRenderActive = false;
    vrHudFrameStarted = false;

    if (xrStageSpace != XR_NULL_HANDLE && xrStageSpace != xrLocalSpace) {
        xrDestroySpace(xrStageSpace);
    }
    xrStageSpace = XR_NULL_HANDLE;

    if (xrViewSpace != XR_NULL_HANDLE) {
        xrDestroySpace(xrViewSpace);
        xrViewSpace = XR_NULL_HANDLE;
    }

    if (xrLocalSpace != XR_NULL_HANDLE) {
        xrDestroySpace(xrLocalSpace);
        xrLocalSpace = XR_NULL_HANDLE;
    }

    if (xrSession != XR_NULL_HANDLE) {
        xrDestroySession(xrSession);
        xrSession = XR_NULL_HANDLE;
    }
}

static const char* SessionStateToString(XrSessionState state)
{
    switch (state) {
        case XR_SESSION_STATE_UNKNOWN: return "UNKNOWN";
        case XR_SESSION_STATE_IDLE: return "IDLE";
        case XR_SESSION_STATE_READY: return "READY";
        case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
        case XR_SESSION_STATE_VISIBLE: return "VISIBLE";
        case XR_SESSION_STATE_FOCUSED: return "FOCUSED";
        case XR_SESSION_STATE_STOPPING: return "STOPPING";
        case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
        case XR_SESSION_STATE_EXITING: return "EXITING";
        default: return "INVALID";
    }
}

static void HandleSessionStateChange(XrSessionState newState)
{
    VR_Log("stdVR_OpenXR: Session state change: %s -> %s\n",
        SessionStateToString(xrSessionState), SessionStateToString(newState));
    xrSessionState = newState;

    switch (newState) {
        case XR_SESSION_STATE_READY: {
            VR_Log("stdVR_OpenXR: Calling xrBeginSession...\n");
            XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
            beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            XrResult result = xrBeginSession(xrSession, &beginInfo);
            if (XR_SUCCEEDED(result)) {
                xrSessionRunning = true;
                stdVR_clientInfo.bSessionRunning = 1;
                VR_Log("stdVR_OpenXR: Session started successfully! VR rendering active.\n");
            } else {
                VR_Log("stdVR_OpenXR: xrBeginSession failed with error %d\n", result);
            }
            break;
        }
        case XR_SESSION_STATE_SYNCHRONIZED:
            VR_Log("stdVR_OpenXR: Session synchronized (waiting for visibility)\n");
            break;
        case XR_SESSION_STATE_VISIBLE:
            VR_Log("stdVR_OpenXR: Session visible (app in background)\n");
            break;
        case XR_SESSION_STATE_FOCUSED:
            VR_Log("stdVR_OpenXR: Session focused (app has input focus)\n");
            break;
        case XR_SESSION_STATE_STOPPING:
            VR_Log("stdVR_OpenXR: Session stopping, calling xrEndSession...\n");
            xrEndSession(xrSession);
            xrSessionRunning = false;
            stdVR_clientInfo.bSessionRunning = 0;
            VR_Log("stdVR_OpenXR: Session stopped\n");
            break;
        case XR_SESSION_STATE_EXITING:
        case XR_SESSION_STATE_LOSS_PENDING:
            VR_Log("stdVR_OpenXR: Session exiting or loss pending\n");
            xrSessionRunning = false;
            stdVR_clientInfo.bSessionRunning = 0;
            break;
        case XR_SESSION_STATE_IDLE:
            VR_Log("stdVR_OpenXR: Session idle\n");
            break;
        default:
            break;
    }
}

static void PollEvents(void)
{
    if (xrInstance == XR_NULL_HANDLE) {
        return;
    }

    XrEventDataBuffer eventData = { XR_TYPE_EVENT_DATA_BUFFER };
    XrResult result;
    int eventsThisFrame = 0;

    while ((result = xrPollEvent(xrInstance, &eventData)) == XR_SUCCESS) {
        eventsThisFrame++;
        switch (eventData.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged* stateChanged = (XrEventDataSessionStateChanged*)&eventData;
                HandleSessionStateChange(stateChanged->state);
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                VR_Log("stdVR_OpenXR: Instance loss pending\n");
                break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                VR_Log("stdVR_OpenXR: Interaction profile changed\n");
                break;
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                VR_Log("stdVR_OpenXR: Reference space change pending\n");
                break;
            default:
                VR_Log("stdVR_OpenXR: Unknown event type %d\n", eventData.type);
                break;
        }
        eventData = { XR_TYPE_EVENT_DATA_BUFFER };
    }

}

extern "C" void stdVR_OpenXR_PollEvents(void)
{
    if (xrInstance == XR_NULL_HANDLE) {
        return;
    }

    PollEvents();
}

extern "C" int stdVR_OpenXR_WaitFrame(void)
{
    PollEvents();

    if (!xrSessionRunning) {
        return 0;
    }

    xrFrameState = { XR_TYPE_FRAME_STATE };
    XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    XrResult result = xrWaitFrame(xrSession, &waitInfo, &xrFrameState);

    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrWaitFrame failed with error %d\n", result);
        return 0;
    }

    stdVR_clientInfo.bShouldRender = xrFrameState.shouldRender;
    stdVR_clientInfo.predictedDisplayTime = xrFrameState.predictedDisplayTime;

    vrFrameCount++;
    // Only log first 3 frames
    if (vrFrameCount <= 3) {
        VR_Log("stdVR_OpenXR: WaitFrame #%d - shouldRender=%d\n",
            vrFrameCount, xrFrameState.shouldRender ? 1 : 0);
    }

    return 1;
}

extern "C" int stdVR_OpenXR_BeginFrame(void)
{
    if (!xrSessionRunning) {
        return 0;
    }

    XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    XrResult result = xrBeginFrame(xrSession, &beginInfo);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrBeginFrame failed with error %d\n", result);
        return 0;
    }

    xrFrameInProgress = true;
    return 1;
}

// Helper to create a quaternion from axis-angle
static XrQuaternionf QuaternionFromAxisAngle(const XrVector3f& axis, float angleRadians)
{
    float halfAngle = angleRadians * 0.5f;
    float sinHalf = std::sin(halfAngle);
    float cosHalf = std::cos(halfAngle);
    return { axis.x * sinHalf, axis.y * sinHalf, axis.z * sinHalf, cosHalf };
}

// Convert degrees to radians
#define DEG2RAD(x) ((x) * 3.14159265358979323846f / 180.0f)

extern "C" int stdVR_OpenXR_EndFrame(void)
{
    if (!xrSessionRunning || !xrFrameInProgress) {
        return 0;
    }

    xrFrameInProgress = false;

    std::vector<XrCompositionLayerBaseHeader*> layers;
    XrCompositionLayerProjection projectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    projectionLayer.next = nullptr;
    projectionLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
        XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    std::vector<XrCompositionLayerProjectionView> projectionViews(STDVR_EYE_COUNT, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });
    XrCompositionLayerQuad quadLayer = { XR_TYPE_COMPOSITION_LAYER_QUAD };

    if (stdVR_clientInfo.bShouldRender) {
        // Check if we should use screen layer mode (for menus)
        if (stdVR_clientInfo.bUseScreenLayer) {
            // Screen layer mode: render menu as a 2D quad floating in front of player

            // Ensure null swapchains are valid and cleared for a black projection layer
            for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
                if (!stdVR_OpenXR_ClearNullSwapchain(eye)) {
                    VR_Log("stdVR_OpenXR: Warning - failed to clear null swapchain for eye %d\n", eye);
                }
            }

            // Build a black projection layer using the null swapchains
            for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
                projectionViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                projectionViews[eye].next = nullptr;
                projectionViews[eye].pose = xrViews[eye].pose;
                projectionViews[eye].fov = xrViews[eye].fov;
                projectionViews[eye].subImage.swapchain = xrNullSwapchains[eye];
                projectionViews[eye].subImage.imageRect.offset = { 0, 0 };
                projectionViews[eye].subImage.imageRect.extent = {
                    (int32_t)xrConfigViews[eye].recommendedImageRectWidth,
                    (int32_t)xrConfigViews[eye].recommendedImageRectHeight
                };
                projectionViews[eye].subImage.imageArrayIndex = 0;
            }

            projectionLayer.space = xrLocalSpace;
            projectionLayer.viewCount = STDVR_EYE_COUNT;
            projectionLayer.views = projectionViews.data();
            layers.push_back((XrCompositionLayerBaseHeader*)&projectionLayer);

            // Get swapchain dimensions
            int32_t width = (int32_t)xrConfigViews[0].recommendedImageRectWidth;
            int32_t height = (int32_t)xrConfigViews[0].recommendedImageRectHeight;

            // Configure the quad layer
            quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            quadLayer.space = xrStageSpace;  // Use stage space so it's fixed relative to world
            quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;  // Show to both eyes

            // Use eye 0's swapchain for the menu content
            quadLayer.subImage.swapchain = xrSwapchains[0];
            quadLayer.subImage.imageRect.offset = { 0, 0 };
            quadLayer.subImage.imageRect.extent = { width, height };
            quadLayer.subImage.imageArrayIndex = 0;

            // Position the quad in front of where the player was looking when entering menu
            float distance = stdVR_clientInfo.screenLayerDistance;
            if (distance <= 0.0f) distance = 4.0f;

            float yawRad = DEG2RAD(stdVR_clientInfo.screenLayerSnapYaw);

            // Position: player's snap position + forward offset based on snap yaw
            XrVector3f pos = {
                stdVR_clientInfo.screenLayerSnapPos.x - std::sin(yawRad) * distance,
                1.2f,  // Fixed height (comfortable viewing height)
                stdVR_clientInfo.screenLayerSnapPos.z - std::cos(yawRad) * distance
            };
            quadLayer.pose.position = pos;

            // Orientation: face the player's snap position (rotate around Y axis)
            XrVector3f yAxis = { 0.0f, 1.0f, 0.0f };
            quadLayer.pose.orientation = QuaternionFromAxisAngle(yAxis, yawRad);

            // Screen size in meters
            float screenWidth = stdVR_clientInfo.screenLayerWidth;
            float screenHeight = stdVR_clientInfo.screenLayerHeight;
            if (screenWidth <= 0.0f) screenWidth = 6.0f;
            if (screenHeight <= 0.0f) screenHeight = 4.5f;
            quadLayer.size = { screenWidth, screenHeight };

            layers.push_back((XrCompositionLayerBaseHeader*)&quadLayer);
        } else {
            // Normal stereo projection mode for 3D gameplay
            for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
                // Ensure proper initialization
                projectionViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                projectionViews[eye].next = nullptr;
                projectionViews[eye].pose = xrViews[eye].pose;
                projectionViews[eye].fov = xrViews[eye].fov;
                projectionViews[eye].subImage.swapchain = xrSwapchains[eye];
                projectionViews[eye].subImage.imageRect.offset = { 0, 0 };
                projectionViews[eye].subImage.imageRect.extent = {
                    (int32_t)xrConfigViews[eye].recommendedImageRectWidth,
                    (int32_t)xrConfigViews[eye].recommendedImageRectHeight
                };
                projectionViews[eye].subImage.imageArrayIndex = 0;
            }

            projectionLayer.space = xrLocalSpace;
            projectionLayer.viewCount = STDVR_EYE_COUNT;
            projectionLayer.views = projectionViews.data();
            layers.push_back((XrCompositionLayerBaseHeader*)&projectionLayer);

            // Add HUD quad layer on top of 3D projection (during gameplay)
            if (vrHudEnabled && vrHudFrameStarted && xrHudSwapchain != XR_NULL_HANDLE) {
                static XrCompositionLayerQuad hudQuadLayer = { XR_TYPE_COMPOSITION_LAYER_QUAD };
                hudQuadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                hudQuadLayer.space = xrViewSpace;  // Head-locked HUD
                hudQuadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

                hudQuadLayer.subImage.swapchain = xrHudSwapchain;
                hudQuadLayer.subImage.imageRect.offset = { 0, 0 };
                hudQuadLayer.subImage.imageRect.extent = { vrHudWidth, vrHudHeight };
                hudQuadLayer.subImage.imageArrayIndex = 0;

                // Position HUD in front of player (head-locked at comfortable distance)
                float hudDistance = 2.0f;  // 2 meters in front
                hudQuadLayer.pose.position = { 0.0f, -0.3f, -hudDistance };  // Slightly below eye level
                hudQuadLayer.pose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };  // Face player

                // HUD size in meters (maintain ~4:3 aspect ratio)
                hudQuadLayer.size = { hudDistance * 1.2f, hudDistance * 0.9f };

                layers.push_back((XrCompositionLayerBaseHeader*)&hudQuadLayer);
                vrHudFrameStarted = false;  // Reset for next frame
            }
        }
    }

    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = xrFrameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = (uint32_t)layers.size();
    endInfo.layers = layers.data();

    XrResult result = xrEndFrame(xrSession, &endInfo);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrEndFrame failed with error %d\n", result);
        stateTrackingActive = false;  // Reset state tracking for next frame
        return 0;
    }

    // OPTIMIZATION: Reset state tracking at end of frame
    // Next frame will re-query GPU state once if needed
    stateTrackingActive = false;

    return 1;
}

static int vrEmptyFrameCount = 0;

extern "C" int stdVR_OpenXR_EndFrameEmpty(void)
{
    if (!xrSessionRunning || !xrFrameInProgress) {
        return 0;
    }

    xrFrameInProgress = false;
    vrEmptyFrameCount++;

    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = xrFrameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 0;
    endInfo.layers = nullptr;

    XrResult result = xrEndFrame(xrSession, &endInfo);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrEndFrame (empty) failed with error %d\n", result);
        stateTrackingActive = false;  // Reset state tracking for next frame
        return 0;
    }

    // OPTIMIZATION: Reset state tracking at end of frame
    stateTrackingActive = false;

    return 1;
}

extern "C" int stdVR_OpenXR_PrepareEyeBuffer(int eye)
{
    if (!xrSessionRunning || eye < 0 || eye >= STDVR_EYE_COUNT) {
        if (eye >= 0 && eye < STDVR_EYE_COUNT) {
            vrCurrentFBO[eye] = 0;
        }
        return 0;
    }

    // Track which eye is being rendered (set early for debug logging)
    stdVR_currentEye = eye;

    bool swapchainAcquired = false;
    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrResult result = xrAcquireSwapchainImage(xrSwapchains[eye], &acquireInfo, &xrSwapchainImageIndex[eye]);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrAcquireSwapchainImage failed for eye %d: error %d\n", eye, result);
        vrCurrentFBO[eye] = 0;
        return 0;
    }
    swapchainAcquired = true;

    if (!stdVR_OpenXR_WaitSwapchainImage(xrSwapchains[eye], "main", eye)) {
        if (swapchainAcquired) {
            stdVR_OpenXR_ReleaseSwapchainImage(eye);
        }
        return 0;
    }

    // OPTIMIZATION: Track state manually instead of GPU queries (avoids pipeline stalls)
    // Only query once at start of VR frame, then track manually
    if (!stateTrackingActive) {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &trackedFBO);
        glGetIntegerv(GL_VIEWPORT, trackedViewport);
        stateTrackingActive = true;
    }
    previousFBO = trackedFBO;
    memcpy(previousViewport, trackedViewport, sizeof(previousViewport));

    // Get the swapchain texture for this frame
    GLuint texture = xrSwapchainImages[eye][xrSwapchainImageIndex[eye]].image;
    uint32_t imageIdx = xrSwapchainImageIndex[eye];

    if (vrFBO[eye].empty() || vrDepthTex[eye].empty()) {
        VR_Log("stdVR_OpenXR: PrepareEyeBuffer(%d) no FBOs/depth textures allocated (images=%zu)\n",
            eye, vrFBO[eye].size());
        if (swapchainAcquired) {
            stdVR_OpenXR_ReleaseSwapchainImage(eye);
        }
        vrCurrentFBO[eye] = 0;
        return 0;
    }
    if (imageIdx >= vrFBO[eye].size()) {
        VR_Log("stdVR_OpenXR: PrepareEyeBuffer(%d) image index out of range: %u (images=%zu)\n",
            eye, imageIdx, vrFBO[eye].size());
        if (swapchainAcquired) {
            stdVR_OpenXR_ReleaseSwapchainImage(eye);
        }
        vrCurrentFBO[eye] = 0;
        return 0;
    }

    GLuint fbo = vrFBO[eye][imageIdx];
    vrCurrentFBO[eye] = fbo;

    // DEBUG: Log texture and FBO info including GL context
    static int prepareLogCount = 0;
    if (++prepareLogCount <= 30 || prepareLogCount % 300 == 0) {
        // Check current GL context
        void* currentRC = wglGetCurrentContext();
        void* currentDC = wglGetCurrentDC();
        VR_Log("stdVR_OpenXR: PrepareEyeBuffer(%d) idx=%u tex=%u fbo=%u swapchain=%llu RC=%p DC=%p\n",
               eye, imageIdx, texture, fbo, (unsigned long long)xrSwapchains[eye], currentRC, currentDC);
    }

    // Bind our VR FBO and attach the swapchain color texture
    // OPTIMIZATION: Depth is permanently attached, only need to swap color
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // DEBUG: Verify FBO is actually bound after attaching texture
    GLint boundFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFbo);
    if (prepareLogCount <= 30 || prepareLogCount % 300 == 0) {
        VR_Log("stdVR_OpenXR:   After bind: boundFbo=%d (expected %u)\n", boundFbo, fbo);
    }

    // OPTIMIZATION: Only validate FBO on first use of each swapchain image
    // This avoids expensive glCheckFramebufferStatus every frame
    if (!vrFBOValidated[eye][imageIdx]) {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            VR_Log("stdVR_OpenXR: FBO incomplete for eye %d image %u, status 0x%x\n", eye, imageIdx, status);
            glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
            if (swapchainAcquired) {
                stdVR_OpenXR_ReleaseSwapchainImage(eye);
            }
            vrCurrentFBO[eye] = 0;
            return 0;
        }
        vrFBOValidated[eye][imageIdx] = true;
    }

    // Set viewport to VR render target size
    int width = xrConfigViews[eye].recommendedImageRectWidth;
    int height = xrConfigViews[eye].recommendedImageRectHeight;
    glViewport(0, 0, width, height);

    // Update tracked state
    trackedFBO = fbo;
    trackedViewport[0] = 0;
    trackedViewport[1] = 0;
    trackedViewport[2] = width;
    trackedViewport[3] = height;

    // Tell std3D to route all "window" FBO bindings to our VR FBO
    std3D_SetVRTargetFBO(fbo, width, height);

    // Clear the buffer - DEBUG: use a visible color to verify texture binding works
    // Use different colors for left/right eye to confirm proper eye routing
    if (eye == 0) {
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);  // BRIGHT RED for left eye
    } else {
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);  // BRIGHT BLUE for right eye
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // DEBUG: Read a pixel back to verify the clear worked
    if (prepareLogCount <= 10) {
        // Method 1: Read from currently bound FBO
        uint8_t pixel1[4] = {0};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel1);

        // Method 2: Create a new FBO and read from the texture directly
        uint8_t pixel2[4] = {0};
        GLuint verifyFBO;
        glGenFramebuffers(1, &verifyFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, verifyFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel2);
        }
        glDeleteFramebuffers(1, &verifyFBO);

        // Rebind original FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        VR_Log("stdVR_OpenXR:   After clear eye=%d: fbo_pixel=[%d,%d,%d,%d] tex_pixel=[%d,%d,%d,%d] tex=%u\n",
               eye, pixel1[0], pixel1[1], pixel1[2], pixel1[3],
               pixel2[0], pixel2[1], pixel2[2], pixel2[3], texture);
    }

    glDisable(GL_FRAMEBUFFER_SRGB);

    return 1;
}

extern "C" int stdVR_OpenXR_FinishEyeBuffer(int eye)
{
    if (!xrSessionRunning || eye < 0 || eye >= STDVR_EYE_COUNT) {
        return 0;
    }

    GLuint fbo = vrCurrentFBO[eye];
    if (fbo == 0 && !vrFBO[eye].empty()) {
        fbo = vrFBO[eye][xrSwapchainImageIndex[eye]];
    }

    // Clear alpha channel to 1.0 before releasing swapchain (like JKXR does)
    // Some OpenXR runtimes treat alpha=0 as "discard" which can cause missing content
    if (fbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);  // Only write alpha
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);  // Restore full color mask

        // DEBUG: Read right after alpha clear (FBO still has texture attached)
        static int postAlphaLogCount = 0;
        if (++postAlphaLogCount <= 10) {
            uint8_t pixel[4] = {0};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            VR_Log("stdVR_OpenXR: PostAlpha eye=%d fbo=%u pixel=[%d,%d,%d,%d]\n",
                   eye, fbo, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }

    // Restore std3D's window FBO routing
    std3D_ClearVRTargetFBO();

    // Detach color texture from FBO (required for some OpenXR runtimes to release the swapchain image)
    // Note: Depth remains attached permanently for performance
    if (fbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }

    // Restore previous FBO and viewport, update tracked state
    glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    trackedFBO = previousFBO;
    memcpy(trackedViewport, previousViewport, sizeof(trackedViewport));

    // DEBUG: Read texture content right before release to verify it still has content
    static int preReleaseLogCount = 0;
    if (++preReleaseLogCount <= 10) {
        uint32_t imgIdx = xrSwapchainImageIndex[eye];
        if (imgIdx < xrSwapchainImages[eye].size()) {
            GLuint texture = xrSwapchainImages[eye][imgIdx].image;

            // Re-attach texture to read it
            GLuint verifyFBO;
            glGenFramebuffers(1, &verifyFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, verifyFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

            uint8_t pixel[4] = {0};
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &verifyFBO);

            VR_Log("stdVR_OpenXR: PreRelease eye=%d tex=%u pixel=[%d,%d,%d,%d]\n",
                   eye, texture, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }

    // Release the swapchain image
    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XrResult result = xrReleaseSwapchainImage(xrSwapchains[eye], &releaseInfo);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrReleaseSwapchainImage failed for eye %d: error %d\n", eye, result);
        return 0;
    }

    // Note: Don't reset stdVR_currentEye here - it needs to persist until the next PrepareEyeBuffer
    vrCurrentFBO[eye] = 0;

    return 1;
}

extern "C" int stdVR_OpenXR_GetCurrentEyeFBO(int eye)
{
    if (eye < 0 || eye >= STDVR_EYE_COUNT) {
        return 0;
    }
    return (int)vrCurrentFBO[eye];
}

extern "C" int stdVR_OpenXR_GetCurrentEye(void)
{
    return stdVR_currentEye;
}

// ============================================================================
// HUD Buffer Functions - Dedicated quad layer for in-game HUD rendering
// ============================================================================

extern "C" int stdVR_OpenXR_PrepareHudBuffer(void)
{
    if (!xrSessionRunning || !vrHudEnabled || xrHudSwapchain == XR_NULL_HANDLE) {
        return 0;
    }

    // If HUD render is already active, finish it first
    if (vrHudRenderActive) {
        stdVR_OpenXR_FinishHudBuffer();
    }

    // Acquire swapchain image
    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrResult result = xrAcquireSwapchainImage(xrHudSwapchain, &acquireInfo, &xrHudSwapchainImageIndex);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrAcquireSwapchainImage(HUD) failed: error %d\n", result);
        return 0;
    }

    // Wait for the image to be available
    if (!stdVR_OpenXR_WaitSwapchainImage(xrHudSwapchain, "HUD", -1)) {
        XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        xrReleaseSwapchainImage(xrHudSwapchain, &releaseInfo);
        return 0;
    }

    // Get the swapchain texture
    if (xrHudSwapchainImageIndex >= xrHudSwapchainImages.size()) {
        VR_Log("stdVR_OpenXR: HUD image index out of range: %u\n", xrHudSwapchainImageIndex);
        XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        xrReleaseSwapchainImage(xrHudSwapchain, &releaseInfo);
        return 0;
    }

    GLuint texture = xrHudSwapchainImages[xrHudSwapchainImageIndex].image;

    // Bind HUD FBO and attach the swapchain color texture
    glBindFramebuffer(GL_FRAMEBUFFER, vrHudFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Validate FBO
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        VR_Log("stdVR_OpenXR: HUD FBO incomplete, status 0x%x\n", status);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        xrReleaseSwapchainImage(xrHudSwapchain, &releaseInfo);
        return 0;
    }

    // Set viewport to HUD size
    glViewport(0, 0, vrHudWidth, vrHudHeight);

    // Clear HUD buffer with transparent black
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vrHudRenderActive = true;
    vrHudFrameStarted = true;  // Signal that HUD was rendered this frame

    static int hudPrepareLogCount = 0;
    if (++hudPrepareLogCount <= 10) {
        VR_Log("stdVR_OpenXR: PrepareHudBuffer - FBO=%u tex=%u size=%dx%d\n",
               vrHudFBO, texture, vrHudWidth, vrHudHeight);
    }

    return 1;
}

extern "C" int stdVR_OpenXR_FinishHudBuffer(void)
{
    if (!vrHudRenderActive) {
        return 0;
    }

    // Detach color texture from FBO
    glBindFramebuffer(GL_FRAMEBUFFER, vrHudFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Release swapchain image
    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XrResult result = xrReleaseSwapchainImage(xrHudSwapchain, &releaseInfo);
    if (XR_FAILED(result)) {
        VR_Log("stdVR_OpenXR: xrReleaseSwapchainImage(HUD) failed: error %d\n", result);
    }

    vrHudRenderActive = false;

    static int hudFinishLogCount = 0;
    if (++hudFinishLogCount <= 10) {
        VR_Log("stdVR_OpenXR: FinishHudBuffer complete\n");
    }

    return 1;
}

extern "C" int stdVR_OpenXR_GetHudFBO(void)
{
    if (!vrHudEnabled || !vrHudRenderActive) {
        return 0;
    }
    return (int)vrHudFBO;
}

extern "C" void stdVR_OpenXR_GetHudSize(int* pWidth, int* pHeight)
{
    if (pWidth) *pWidth = vrHudWidth;
    if (pHeight) *pHeight = vrHudHeight;
}

extern "C" int stdVR_OpenXR_IsHudEnabled(void)
{
    return vrHudEnabled ? 1 : 0;
}

// Helper to build asymmetric projection matrix from FOV tangents
static void BuildProjectionMatrix(float* out16, float tanLeft, float tanRight, float tanUp, float tanDown, float zNear, float zFar)
{
    // Column-major 4x4 perspective projection matrix
    // Uses OpenXR convention where tangents are already computed from angles
    float left = -tanLeft * zNear;
    float right = tanRight * zNear;
    float bottom = -tanDown * zNear;
    float top = tanUp * zNear;

    float width = right - left;
    float height = top - bottom;

    memset(out16, 0, 16 * sizeof(float));
    out16[0] = 2.0f * zNear / width;
    out16[5] = 2.0f * zNear / height;
    out16[8] = (right + left) / width;
    out16[9] = (top + bottom) / height;
    out16[10] = -(zFar + zNear) / (zFar - zNear);
    out16[11] = -1.0f;
    out16[14] = -2.0f * zFar * zNear / (zFar - zNear);
    out16[15] = 0.0f;
}

extern "C" void stdVR_OpenXR_UpdateTracking(void)
{
    if (!xrSessionRunning) {
        return;
    }

    // Locate views (HMD)
    XrViewLocateInfo viewLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = xrFrameState.predictedDisplayTime;
    viewLocateInfo.space = xrLocalSpace;

    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    uint32_t viewCount = 0;
    XrResult result = xrLocateViews(xrSession, &viewLocateInfo, &viewState, STDVR_EYE_COUNT, &viewCount, xrViews);

    if (XR_FAILED(result) || viewCount < STDVR_EYE_COUNT) {
        return;
    }

    // Check tracking validity - only update if both position and orientation are valid
    bool positionValid = (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0;
    bool orientationValid = (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0;

    if (!positionValid || !orientationValid) {
        // Tracking lost - keep last known pose, don't update
        static int trackingLostCount = 0;
        trackingLostCount++;
        if (trackingLostCount <= 5) {
            VR_Log("stdVR_OpenXR: Tracking invalid (pos=%d, ori=%d) - using last known pose\n",
                positionValid ? 1 : 0, orientationValid ? 1 : 0);
        }
        return;
    }

    // Update HMD pose (use center of both eyes)
    XrPosef centerPose;
    centerPose.position.x = (xrViews[0].pose.position.x + xrViews[1].pose.position.x) * 0.5f;
    centerPose.position.y = (xrViews[0].pose.position.y + xrViews[1].pose.position.y) * 0.5f;
    centerPose.position.z = (xrViews[0].pose.position.z + xrViews[1].pose.position.z) * 0.5f;
    centerPose.orientation = xrViews[0].pose.orientation; // Use left eye orientation for simplicity

    // Log raw HMD position before conversion
    static int hmdLogCounter = 0;
    hmdLogCounter++;
    if (hmdLogCounter % 60 == 1) {
        VR_Log("HMD OpenXR raw=(%.4f, %.4f, %.4f)\n",
            centerPose.position.x, centerPose.position.y, centerPose.position.z);
    }

    // Convert from OpenXR coords to JKDF2 coords:
    // OpenXR: X=right, Y=up, Z=back
    // JKDF2:  X=right, Y=forward, Z=up
    // JKDF2.x = OpenXR.x, JKDF2.y = -OpenXR.z, JKDF2.z = OpenXR.y
    stdVR_clientInfo.hmdPosition.x = centerPose.position.x;
    stdVR_clientInfo.hmdPosition.y = -centerPose.position.z;  // Forward = -back
    stdVR_clientInfo.hmdPosition.z = centerPose.position.y;   // Up = up

    if (hmdLogCounter % 60 == 1) {
        VR_Log("HMD JKDF2 pos=(%.4f, %.4f, %.4f)\n",
            stdVR_clientInfo.hmdPosition.x, stdVR_clientInfo.hmdPosition.y, stdVR_clientInfo.hmdPosition.z);
    }

    QuatToEuler(&centerPose.orientation, &stdVR_clientInfo.hmdOrientation);
    PoseToMatrix(&centerPose, &stdVR_clientInfo.hmdPoseMatrix);

    // Default near/far planes for projection matrix
    const float zNear = 0.1f;
    const float zFar = 1000.0f;

    // Update per-eye views
    for (int eye = 0; eye < STDVR_EYE_COUNT; eye++) {
        stdVR_EyeView* pEye = &stdVR_clientInfo.eyes[eye];

        // Store FOV tangents (OpenXR gives angles, we need tangents)
        // Note: angleLeft and angleDown are already negative in OpenXR
        pEye->fovLeft = std::tan(-xrViews[eye].fov.angleLeft);
        pEye->fovRight = std::tan(xrViews[eye].fov.angleRight);
        pEye->fovUp = std::tan(xrViews[eye].fov.angleUp);
        pEye->fovDown = std::tan(-xrViews[eye].fov.angleDown);

        // Build the full projection matrix
        BuildProjectionMatrix(pEye->projectionMatrix,
            pEye->fovLeft, pEye->fovRight, pEye->fovUp, pEye->fovDown,
            zNear, zFar);

        // Build eye view matrix directly from the per-eye pose (includes full transform)
        // This is the complete eye pose, not just an offset from center
        PoseToMatrix(&xrViews[eye].pose, &pEye->viewMatrix);
    }

    // Locate controllers with velocity tracking
    for (int hand = 0; hand < STDVR_CONTROLLER_COUNT; hand++) {
        // Chain velocity query to location query
        XrSpaceVelocity velocity = { XR_TYPE_SPACE_VELOCITY };
        XrSpaceLocation location = { XR_TYPE_SPACE_LOCATION };
        location.next = &velocity;  // Chain velocity struct

        if (xrControllerSpaces[hand] != XR_NULL_HANDLE) {
            xrLocateSpace(xrControllerSpaces[hand], xrLocalSpace, xrFrameState.predictedDisplayTime, &location);

            stdVR_ControllerState* pCtrl = &stdVR_clientInfo.controllers[hand];

            if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
                // Log raw OpenXR position before conversion
                static int ctrlLogCounter = 0;
                ctrlLogCounter++;
                if (ctrlLogCounter % 60 == 1) {
                    VR_Log("Controller[%d] OpenXR raw=(%.4f, %.4f, %.4f)\n",
                        hand, location.pose.position.x, location.pose.position.y, location.pose.position.z);
                }

                // Convert from OpenXR coords to JKDF2 coords:
                // OpenXR: X=right, Y=up, Z=back
                // JKDF2:  X=right, Y=forward, Z=up
                // JKDF2.x = OpenXR.x, JKDF2.y = -OpenXR.z, JKDF2.z = OpenXR.y
                pCtrl->position.x = location.pose.position.x;
                pCtrl->position.y = -location.pose.position.z;  // Forward = -back
                pCtrl->position.z = location.pose.position.y;   // Up = up
                pCtrl->bTracking = 1;

                if (ctrlLogCounter % 60 == 1) {
                    VR_Log("Controller[%d] JKDF2 pos=(%.4f, %.4f, %.4f) tracking=%d\n",
                        hand, pCtrl->position.x, pCtrl->position.y, pCtrl->position.z, pCtrl->bTracking);
                }
            } else {
                pCtrl->bTracking = 0;
            }

            if (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
                QuatToEuler(&location.pose.orientation, &pCtrl->orientation);
                PoseToMatrix(&location.pose, &pCtrl->poseMatrix);
                // Use grip pose as the grip matrix too (same pose currently)
                PoseToMatrix(&location.pose, &pCtrl->gripPoseMatrix);
            }

            // Store velocity data for motion controls
            if (velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
                // Convert from OpenXR coords to JKDF2 coords (same as pose conversion)
                pCtrl->motion.linearVelocity.x = velocity.linearVelocity.x;
                pCtrl->motion.linearVelocity.y = -velocity.linearVelocity.z;  // OpenXR Z -> JKDF2 -Y
                pCtrl->motion.linearVelocity.z = velocity.linearVelocity.y;   // OpenXR Y -> JKDF2 Z

                // Calculate swing speed (velocity magnitude)
                float vx = pCtrl->motion.linearVelocity.x;
                float vy = pCtrl->motion.linearVelocity.y;
                float vz = pCtrl->motion.linearVelocity.z;
                pCtrl->motion.swingSpeed = sqrtf(vx*vx + vy*vy + vz*vz);
            } else {
                pCtrl->motion.linearVelocity.x = 0.0f;
                pCtrl->motion.linearVelocity.y = 0.0f;
                pCtrl->motion.linearVelocity.z = 0.0f;
                pCtrl->motion.swingSpeed = 0.0f;
            }

            if (velocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
                pCtrl->motion.angularVelocity.x = velocity.angularVelocity.x;
                pCtrl->motion.angularVelocity.y = -velocity.angularVelocity.z;
                pCtrl->motion.angularVelocity.z = velocity.angularVelocity.y;
            } else {
                pCtrl->motion.angularVelocity.x = 0.0f;
                pCtrl->motion.angularVelocity.y = 0.0f;
                pCtrl->motion.angularVelocity.z = 0.0f;
            }
        }
    }
}

extern "C" void stdVR_OpenXR_UpdateInput(void)
{
    if (!xrSessionRunning) {
        return;
    }

    stdVR_clientInfo.analogMove[0] = 0.0f;
    stdVR_clientInfo.analogMove[1] = 0.0f;
    stdVR_clientInfo.analogTurn[0] = 0.0f;
    stdVR_clientInfo.analogTurn[1] = 0.0f;
    stdVR_clientInfo.triggerLeft = 0.0f;
    stdVR_clientInfo.triggerRight = 0.0f;
    stdVR_clientInfo.gripLeft = 0.0f;
    stdVR_clientInfo.gripRight = 0.0f;

    // Sync actions
    XrActiveActionSet activeActionSet = {};
    activeActionSet.actionSet = xrActionSet;
    activeActionSet.subactionPath = XR_NULL_PATH;

    XrActionsSyncInfo syncInfo = { XR_TYPE_ACTIONS_SYNC_INFO };
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    xrSyncActions(xrSession, &syncInfo);

    uint32_t prevButtonState = stdVR_clientInfo.buttonState;
    stdVR_clientInfo.buttonState = 0;

    // Get thumbstick values
    for (int hand = 0; hand < STDVR_CONTROLLER_COUNT; hand++) {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.subactionPath = xrHandPaths[hand];

        // Thumbstick
        getInfo.action = xrThumbstickAction;
        XrActionStateVector2f vec2State = { XR_TYPE_ACTION_STATE_VECTOR2F };
        xrGetActionStateVector2f(xrSession, &getInfo, &vec2State);
        if (vec2State.isActive) {
            if (hand == STDVR_CONTROLLER_LEFT) {
                // Map thumbstick axes to game movement
                // Based on user testing: X and Y axes are swapped
                // Stick left/right (X) -> forward/back
                // Stick up/down (Y) -> strafe left/right
                stdVR_clientInfo.analogMove[0] = vec2State.currentState.y;   // Y -> strafe
                stdVR_clientInfo.analogMove[1] = vec2State.currentState.x;   // X -> forward/back
            } else {
                stdVR_clientInfo.analogTurn[0] = vec2State.currentState.x;
                stdVR_clientInfo.analogTurn[1] = vec2State.currentState.y;
            }
        }

        // Trigger
        getInfo.action = xrTriggerAction;
        XrActionStateFloat floatState = { XR_TYPE_ACTION_STATE_FLOAT };
        xrGetActionStateFloat(xrSession, &getInfo, &floatState);
        if (floatState.isActive) {
            if (hand == STDVR_CONTROLLER_LEFT) {
                stdVR_clientInfo.triggerLeft = floatState.currentState;
                if (floatState.currentState > 0.5f) {
                    stdVR_clientInfo.buttonState |= STDVR_BTN_TRIGGER_L;
                }
            } else {
                stdVR_clientInfo.triggerRight = floatState.currentState;
                if (floatState.currentState > 0.5f) {
                    stdVR_clientInfo.buttonState |= STDVR_BTN_TRIGGER_R;
                }
            }
        }

        // Grip
        getInfo.action = xrGripAction;
        xrGetActionStateFloat(xrSession, &getInfo, &floatState);
        if (floatState.isActive) {
            if (hand == STDVR_CONTROLLER_LEFT) {
                stdVR_clientInfo.gripLeft = floatState.currentState;
                if (floatState.currentState > 0.5f) {
                    stdVR_clientInfo.buttonState |= STDVR_BTN_GRIP_L;
                }
            } else {
                stdVR_clientInfo.gripRight = floatState.currentState;
                if (floatState.currentState > 0.5f) {
                    stdVR_clientInfo.buttonState |= STDVR_BTN_GRIP_R;
                }
            }
        }
    }

    // Get button states (no subaction)
    XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.subactionPath = XR_NULL_PATH;
    XrActionStateBoolean boolState = { XR_TYPE_ACTION_STATE_BOOLEAN };

    getInfo.action = xrButtonAAction;
    xrGetActionStateBoolean(xrSession, &getInfo, &boolState);
    if (boolState.isActive && boolState.currentState) {
        stdVR_clientInfo.buttonState |= STDVR_BTN_A;
    }

    getInfo.action = xrButtonBAction;
    xrGetActionStateBoolean(xrSession, &getInfo, &boolState);
    if (boolState.isActive && boolState.currentState) {
        stdVR_clientInfo.buttonState |= STDVR_BTN_B;
    }

    getInfo.action = xrButtonXAction;
    xrGetActionStateBoolean(xrSession, &getInfo, &boolState);
    if (boolState.isActive && boolState.currentState) {
        stdVR_clientInfo.buttonState |= STDVR_BTN_X;
    }

    getInfo.action = xrButtonYAction;
    xrGetActionStateBoolean(xrSession, &getInfo, &boolState);
    if (boolState.isActive && boolState.currentState) {
        stdVR_clientInfo.buttonState |= STDVR_BTN_Y;
    }

    getInfo.action = xrMenuAction;
    xrGetActionStateBoolean(xrSession, &getInfo, &boolState);
    if (boolState.isActive && boolState.currentState) {
        stdVR_clientInfo.buttonState |= STDVR_BTN_MENU;
    }

    // Compute pressed/released
    stdVR_clientInfo.buttonPressed = stdVR_clientInfo.buttonState & ~prevButtonState;
    stdVR_clientInfo.buttonReleased = prevButtonState & ~stdVR_clientInfo.buttonState;
}

extern "C" void stdVR_OpenXR_TriggerHaptic(int hand, float amplitude, float duration, float frequency)
{
    if (!xrSessionRunning || hand < 0 || hand >= STDVR_CONTROLLER_COUNT) {
        return;
    }

    XrHapticActionInfo hapticInfo = { XR_TYPE_HAPTIC_ACTION_INFO };
    hapticInfo.action = xrHapticAction;
    hapticInfo.subactionPath = xrHandPaths[hand];

    XrHapticVibration vibration = { XR_TYPE_HAPTIC_VIBRATION };
    vibration.amplitude = amplitude;
    vibration.duration = (int64_t)(duration * 1000000000.0f); // Convert to nanoseconds
    vibration.frequency = frequency;

    xrApplyHapticFeedback(xrSession, &hapticInfo, (XrHapticBaseHeader*)&vibration);
}

extern "C" void stdVR_OpenXR_StopHaptic(int hand)
{
    if (!xrSessionRunning || hand < 0 || hand >= STDVR_CONTROLLER_COUNT) {
        return;
    }

    XrHapticActionInfo hapticInfo = { XR_TYPE_HAPTIC_ACTION_INFO };
    hapticInfo.action = xrHapticAction;
    hapticInfo.subactionPath = xrHandPaths[hand];

    xrStopHapticFeedback(xrSession, &hapticInfo);
}

extern "C" void stdVR_OpenXR_RecenterView(void)
{
    // Recreate local space to recenter
    if (xrSession != XR_NULL_HANDLE && xrLocalSpace != XR_NULL_HANDLE) {
        xrDestroySpace(xrLocalSpace);

        XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
        xrCreateReferenceSpace(xrSession, &spaceInfo, &xrLocalSpace);
    }
}

extern "C" const char* stdVR_OpenXR_GetRuntimeName(void)
{
    return xrRuntimeName;
}

#endif // PLATFORM_VR
