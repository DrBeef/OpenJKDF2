#include "rdCamera.h"

#include "Engine/rdLight.h"
#include "jk.h"
#include "Engine/rdroid.h"
#include "General/stdMath.h"
#include "Win95/stdDisplay.h"
#include "Platform/std3D.h"
#include "Engine/sithRender.h"
#include "World/jkPlayer.h"

static rdVector3 rdCamera_camRotation;
static flex_t rdCamera_mipmapScalar = 1.0; // MOTS added

#ifdef PLATFORM_VR
static float rdCamera_vrProjection[16];
static int rdCamera_bUsingVRProjection = 0;
static float rdCamera_vrTanLeft = 0.0f;
static float rdCamera_vrTanRight = 0.0f;
static float rdCamera_vrTanUp = 0.0f;
static float rdCamera_vrTanDown = 0.0f;
// Added: VR render target dimensions (replaces canvas size for CPU projection)
static int rdCamera_vrRenderWidth = 0;
static int rdCamera_vrRenderHeight = 0;
#endif

#ifdef TARGET_TWL
int rdCamera_bForceRealProj = 0;
#endif

rdCamera* rdCamera_New(flex_t fov, flex_t x, flex_t y, flex_t z, flex_t aspectRatio)
{
    rdCamera* out = (rdCamera *)rdroid_pHS->alloc(sizeof(rdCamera));
    if ( !out ) {
        return 0;
    }
    
    // Added: zero out alloc
    memset(out, 0, sizeof(*out));

    rdCamera_NewEntry(out, fov, x, y, z, aspectRatio);    
    
    return out;
}

int rdCamera_NewEntry(rdCamera *camera, flex_t fov, BOOL bClipFar, flex_t zNear, flex_t zFar, flex_t aspectRatio)
{
    if (!camera)
        return 0;

#ifdef TARGET_TWL
    bClipFar = 1;
#endif

    // Added: Don't double-alloc
    if (!camera->pClipFrustum)
    {
        camera->pClipFrustum = (rdClipFrustum *)rdroid_pHS->alloc(sizeof(rdClipFrustum));
    }

    if ( camera->pClipFrustum )
    {
        camera->canvas = 0;
        rdCamera_SetFOV(camera, fov);
        rdCamera_SetOrthoScale(camera, 1.0);

        camera->pClipFrustum->bClipFar = bClipFar;
        camera->pClipFrustum->zNear = zNear;
        camera->pClipFrustum->zFar = zFar;
        camera->screenAspectRatio = aspectRatio;
        camera->ambientLight = 0.0;
        camera->numLights = 0;
        camera->attenuationMin = 0.2;
        camera->attenuationMax = 0.1;
        
        rdCamera_SetProjectType(camera, rdCameraProjectType_Perspective);

        return 1;
    }
    return 0;
}

void rdCamera_Free(rdCamera *camera)
{
    if (camera)
    {
        rdCamera_FreeEntry(camera);
        rdroid_pHS->free(camera);
    }
}

void rdCamera_FreeEntry(rdCamera *camera)
{
    if ( camera->pClipFrustum ) {
        rdroid_pHS->free(camera->pClipFrustum);
        camera->pClipFrustum = NULL; // Added: no UAF
    }
}

int rdCamera_SetCanvas(rdCamera *camera, rdCanvas *canvas)
{
    camera->canvas = canvas;
    rdCamera_BuildFOV(camera);
    return 1;
}

int rdCamera_SetCurrent(rdCamera *camera)
{
    if ( rdCamera_pCurCamera != camera )
        rdCamera_pCurCamera = camera;
    return 1;
}

extern int jkGuiBuildMulti_bRendering;
int rdCamera_SetFOV(rdCamera *camera, flex_t fovVal)
{
    if ( fovVal < 5.0 )
    {
        fovVal = 5.0;
    }
    else if ( fovVal > 179.0 )
    {
        fovVal = 179.0;
    }

#ifdef QOL_IMPROVEMENTS
    if (!jkGuiBuildMulti_bRendering && jkPlayer_fovIsVertical && camera->screenAspectRatio != 0.0) {
        camera->fov = stdMath_ArcTan3(1.0, stdMath_Tan(fovVal * 0.5) / camera->screenAspectRatio) * -2.0;

        if ( camera->fov < 5.0 )
        {
            camera->fov = 5.0;
        }
        else if ( camera->fov > 179.0 )
        {
            camera->fov = 179.0;
        }
    }
    else
#endif
    {
        camera->fov = fovVal;
    }     
    
    rdCamera_BuildFOV(camera);
    return 1;
}

int rdCamera_SetProjectType(rdCamera *camera, int type)
{
    camera->projectType = type;
    
    switch (type)
    {
        case rdCameraProjectType_Ortho:
        {
            if (camera->screenAspectRatio == 1.0 )
            {
                camera->fnProject = rdCamera_OrthoProjectSquare;
                camera->fnProjectLst = rdCamera_OrthoProjectSquareLst;
#ifdef TARGET_TWL
                camera->fnProjectLstClip = rdCamera_OrthoProjectSquareLst;
#endif
            }
            else
            {
                camera->fnProject = rdCamera_OrthoProject;
                camera->fnProjectLst = rdCamera_OrthoProjectLst;
#ifdef TARGET_TWL
                camera->fnProjectLstClip = rdCamera_OrthoProjectLst;
#endif
            }
            break;
        }
        case rdCameraProjectType_Perspective:
        {
            if (camera->screenAspectRatio == 1.0)
            {
                camera->fnProject = rdCamera_PerspProjectSquare;
                camera->fnProjectLst = rdCamera_PerspProjectSquareLst;
#ifdef TARGET_TWL
                camera->fnProjectLstClip = rdCamera_PerspProjectLstClip;
                if (rdCamera_bForceRealProj) {
                    camera->fnProject = rdCamera_PerspProjectClip;
                    camera->fnProjectLst = rdCamera_PerspProjectLstClip;
                }
#endif
            }
            else
            {
                camera->fnProject = rdCamera_PerspProject;
                camera->fnProjectLst = rdCamera_PerspProjectLst;
#ifdef TARGET_TWL
                camera->fnProjectLstClip = rdCamera_PerspProjectLstClip;
                if (rdCamera_bForceRealProj) {
                    camera->fnProject = rdCamera_PerspProjectClip;
                    camera->fnProjectLst = rdCamera_PerspProjectLstClip;
                }
#endif
            }
            break;
        }
        
    }

    if ( camera->canvas )
        rdCamera_BuildFOV(camera);

    return 1;
}

int rdCamera_SetOrthoScale(rdCamera *camera, flex_t scale)
{
    camera->orthoScale = scale;
    rdCamera_BuildFOV(camera);
    return 1;
}

int rdCamera_SetAspectRatio(rdCamera *camera, flex_t ratio)
{
#ifdef QOL_IMPROVEMENTS
    if (jkPlayer_enableOrigAspect) ratio = 1.0;
#endif

    camera->screenAspectRatio = ratio;
    return rdCamera_SetProjectType(camera, camera->projectType);
}

int rdCamera_BuildFOV(rdCamera *camera)
{
    flex_d_t v10; // st3
    flex_d_t v15; // st4
    flex_t camerac; // [esp+1Ch] [ebp+4h]

    rdClipFrustum* clipFrustum = camera->pClipFrustum;
    rdCanvas* canvas = camera->canvas;
    if ( !canvas )
        return 0;

    switch (camera->projectType)
    {
        case rdCameraProjectType_Ortho:
        {
            camera->fovDx = 0.0;
            camerac = ((flex_d_t)(canvas->heightMinusOne - canvas->yStart) * 0.5) / camera->orthoScale;
            v15 = ((flex_d_t)(canvas->widthMinusOne - canvas->xStart) * 0.5) / camera->orthoScale;
            clipFrustum->orthoLeft = -v15;
            clipFrustum->orthoTop = camerac / camera->screenAspectRatio;
            clipFrustum->orthoRight = v15;
            clipFrustum->orthoBottom = -camerac / camera->screenAspectRatio;
            clipFrustum->farTop = 0.0;
            clipFrustum->bottom = 0.0;
            clipFrustum->farLeft = 0.0;
            clipFrustum->right = 0.0;
            return 1;
        }
        
        case rdCameraProjectType_Perspective:
        {
#if defined(QOL_IMPROVEMENTS)
            flex_t overdraw = 1.0; // Added: HACK for 1px off on the bottom of the screen
#else
            flex_t overdraw = 0.0;
#endif
            flex_t width = canvas->xStart;
            flex_t height = canvas->yStart;
            flex_t project_width_half = overdraw + (canvas->widthMinusOne - (flex_d_t)width) * 0.5;
            flex_t project_height_half = overdraw + (canvas->heightMinusOne - (flex_d_t)height) * 0.5;
            
            flex_t project_width_half_2 = project_width_half;
            flex_t project_height_half_2 = project_height_half;
            
            flex_t tangent = stdMath_Tan(camera->fov * 0.5);
            camera->fovDx = project_width_half / tangent;

            flex_t fovDx = camera->fovDx;
            flex_t fovDy = camera->fovDx;

            // UBSAN fixes
            if (fovDy == 0) {
                fovDy = 0.000001;
            }
            if (fovDx == 0) {
                fovDx = 0.000001;
            }

            // This area is very susceptible to fixed-point error 
#ifdef EXPERIMENTAL_FIXED_POINT
            clipFrustum->bClipFar = 1;
            flex_t aspect = project_height_half_2/project_width_half;
            clipFrustum->farTop = tangent * aspect; // far top
            clipFrustum->farLeft = -tangent; // far left
            clipFrustum->bottom = -clipFrustum->farTop;
            clipFrustum->right = tangent; // right
            clipFrustum->nearTop = ((project_height_half - -1.0) / project_width_half) * tangent; // near top
            clipFrustum->nearLeft = (-(project_width_half - -1.0) / project_width_half) * tangent; // near left
#else
            clipFrustum->farTop = project_height_half / fovDy; // far top
            clipFrustum->farLeft = -project_width_half / fovDx; // far left
            clipFrustum->bottom = -project_height_half_2 / fovDy; // bottom
            clipFrustum->right = project_width_half_2 / fovDx; // right
            clipFrustum->nearTop = (project_height_half - -1.0) / fovDy; // near top
            clipFrustum->nearLeft = -(project_width_half - -1.0) / fovDx; // near left
#endif
            return 1;
        }
    }

    return 1;
}

int rdCamera_BuildClipFrustum(rdCamera *camera, rdClipFrustum *outClip, signed int minX, signed int minY, signed int maxX, signed int maxY)
{
    //jk_printf("%u %u %u %u\n", height, width, height2, width2);

    rdClipFrustum* cameraClip = camera->pClipFrustum;
    rdCanvas* canvas = camera->canvas;
    if ( !canvas )
        return 0;

    outClip->bClipFar = cameraClip->bClipFar;
    outClip->zNear = cameraClip->zNear;
    outClip->zFar = cameraClip->zFar;

#ifdef PLATFORM_VR
    // When VR projection is active, use asymmetric VR tangents for frustum
    // Note: OpenXR tanLeft is typically negative, tanDown is typically negative
    // So we check that tangents are set (non-zero) rather than > 0
    if (rdCamera_bUsingVRProjection
        && (rdCamera_vrTanLeft != 0.0f || rdCamera_vrTanRight != 0.0f
        ||  rdCamera_vrTanUp != 0.0f || rdCamera_vrTanDown != 0.0f))
    {
        // VR frustum uses asymmetric tangents directly
        // tanLeft is negative (pointing left), so -tanLeft gives positive value for farLeft
        // tanDown is negative (pointing down), so -tanDown gives positive value for bottom
        outClip->farLeft = -rdCamera_vrTanLeft;
        outClip->right = rdCamera_vrTanRight;
        outClip->farTop = rdCamera_vrTanUp;
        outClip->bottom = -rdCamera_vrTanDown;
        outClip->nearLeft = outClip->farLeft;
        outClip->nearTop = outClip->farTop;
        return 1;
    }
#endif

#if defined(QOL_IMPROVEMENTS)
    flex_t overdraw = 1.0; // Added: HACK for 1px off on the bottom of the screen
#else
    flex_t overdraw = 0.0;
#endif
    flex_t project_width_half = overdraw + canvas->half_screen_height - ((flex_d_t)minY - 0.5);
    flex_t project_height_half = overdraw + canvas->half_screen_width - ((flex_d_t)minX - 0.5);

    flex_t project_width_half_2 = -canvas->half_screen_height + ((flex_d_t)maxY - 0.5);
    flex_t project_height_half_2 = -canvas->half_screen_width + ((flex_d_t)maxX - 0.5);

    flex_t fovDx = camera->fovDx;
    flex_t fovDy = camera->fovDx;

    // UBSAN fixes
    if (fovDy == 0) {
        fovDy = 0.000001;
    }
    if (fovDx == 0) {
        fovDx = 0.000001;
    }

#if 0 //def EXPERIMENTAL_FIXED_POINT
    flex_t tangent = stdMath_Tan(camera->fov * 0.5);
    flex_t aspect = project_height_half_2/project_width_half;
    clipFrustum->farTop = tangent * aspect; // far top
    clipFrustum->farLeft = -tangent; // far left
    clipFrustum->bottom = -clipFrustum->farTop;
    clipFrustum->right = tangent; // right
    clipFrustum->nearTop = ((project_height_half - -1.0) / project_width_half) * tangent; // near top
    clipFrustum->nearLeft = (-(project_width_half - -1.0) / project_width_half) * tangent; // near left
#else
    outClip->farTop = project_width_half / fovDy;
    outClip->farLeft = -project_height_half / fovDx;
    outClip->bottom = -project_width_half_2 / fovDy;
    outClip->right = project_height_half_2 / fovDx;
    outClip->nearTop = (project_width_half - -1.0) / fovDy;
    outClip->nearLeft = -(project_height_half - -1.0) / fovDx;
#endif

    return 1;
}

void rdCamera_Update(rdMatrix34 *orthoProj)
{
    rdMatrix_InvertOrtho34(&rdCamera_pCurCamera->view_matrix, orthoProj);
    rdMatrix_Copy34(&rdCamera_camMatrix, orthoProj);
    rdMatrix_ExtractAngles34(&rdCamera_camMatrix, &rdCamera_camRotation);
}

// Added: Update camera matrix globals without changing view_matrix
// Used by VR path which sets view_matrix separately
void rdCamera_UpdateCamMatrix(rdMatrix34 *cameraWorldMatrix)
{
    rdMatrix_Copy34(&rdCamera_camMatrix, cameraWorldMatrix);
    rdMatrix_ExtractAngles34(&rdCamera_camMatrix, &rdCamera_camRotation);
}

void rdCamera_OrthoProject(rdVector3* out, const rdVector3* v)
{
    //rdCamera_pCurCamera->orthoScale = 200.0;

    out->x = rdCamera_pCurCamera->orthoScale * v->x + rdCamera_pCurCamera->canvas->half_screen_width;
    out->y = -(v->z * rdCamera_pCurCamera->orthoScale) * rdCamera_pCurCamera->screenAspectRatio + rdCamera_pCurCamera->canvas->half_screen_height;
    out->z = v->y * rdCamera_pCurCamera->orthoScale;

    //printf("%f %f %f -> %f %f %f\n", v->x, v->y, v->z, out->x, out->y, out->z);
}

void rdCamera_OrthoProjectLst(rdVector3 *vertices_out, const rdVector3 *vertices_in, unsigned int num_vertices)
{
    for (int i = 0; i < num_vertices; i++)
    {
        rdCamera_OrthoProject(vertices_out, vertices_in);
        ++vertices_in;
        ++vertices_out;
    }
}

void rdCamera_OrthoProjectSquare(rdVector3 *out, const rdVector3 *v)
{
    out->x = rdCamera_pCurCamera->orthoScale * v->x + rdCamera_pCurCamera->canvas->half_screen_width;
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - v->z * rdCamera_pCurCamera->orthoScale;
    out->z = v->y;
}

void rdCamera_OrthoProjectSquareLst(rdVector3 *vertices_out, const rdVector3 *vertices_in, unsigned int num_vertices)
{
    for (int i = 0; i < num_vertices; i++)
    {
        rdCamera_OrthoProjectSquare(vertices_out, vertices_in);
        ++vertices_in;
        ++vertices_out;
    }
}

#ifdef PLATFORM_VR
static inline int rdCamera_UseVRProjection(void)
{
    // Note: OpenXR tanLeft and tanDown are typically negative
    // So check for non-zero values, not positive values
    return rdCamera_bUsingVRProjection
        && rdCamera_pCurCamera
        && rdCamera_pCurCamera->canvas
        && (rdCamera_vrTanLeft != 0.0f || rdCamera_vrTanRight != 0.0f
        ||  rdCamera_vrTanUp != 0.0f || rdCamera_vrTanDown != 0.0f);
}

static void rdCamera_PerspProjectVR(rdVector3 *out, const rdVector3 *v)
{
    // Use VR render dimensions if set, otherwise fall back to canvas
    float half_w, half_h;
    if (rdCamera_vrRenderWidth > 0 && rdCamera_vrRenderHeight > 0) {
        half_w = rdCamera_vrRenderWidth * 0.5f;
        half_h = rdCamera_vrRenderHeight * 0.5f;
    } else {
        rdCanvas* canvas = rdCamera_pCurCamera->canvas;
        half_w = canvas->half_screen_width;
        half_h = canvas->half_screen_height;
    }
    float y = v->y;
    if (y == 0.0f) {
        y = 0.000001f;
    }

    // Convert stored tangents into signed frustum bounds
    float left = -rdCamera_vrTanLeft;
    float right = rdCamera_vrTanRight;
    float top = rdCamera_vrTanUp;
    float bottom = -rdCamera_vrTanDown;

    float invW = 1.0f / (right - left);
    float invH = 1.0f / (top - bottom);

    float scaleX = (2.0f * half_w * invW) / y;
    float scaleY = (2.0f * half_h * invH) / y;
    float offsetX = -2.0f * half_w * left * invW;
    float offsetY = 2.0f * half_h * top * invH;

    out->x = offsetX + v->x * scaleX;
    out->y = offsetY - v->z * scaleY;
    out->z = v->y;

    // DEBUG: Log projection details for first few vertices
    {
        extern int stdVR_currentEye;
        extern void VR_Log(const char* fmt, ...);
        static int logCount[2] = {0, 0};
        int eye = stdVR_currentEye;
        if (eye >= 0 && eye < 2) {
            logCount[eye]++;
            if (logCount[eye] <= 5 || logCount[eye] == 100 || logCount[eye] == 1000) {
                VR_Log("ProjVR[eye%d] #%d: in=(%.1f,%.1f,%.1f) out=(%.1f,%.1f,%.4f) half=(%.0f,%.0f) tan=(%.3f,%.3f,%.3f,%.3f)\n",
                    eye, logCount[eye], v->x, v->y, v->z, out->x, out->y, out->z,
                    half_w, half_h, left, right, top, bottom);
            }
        }
    }
}
#endif

// TODO: The original game had an aspect ratio multiply here, 
// DSi needs an aspect divide, OpenGL wants nothing??
void rdCamera_PerspProject(rdVector3 *out, const rdVector3 *v)
{
#ifdef TARGET_TWL
    // DSi does HW projection
    out->x = v->x;
    out->y = v->y;
    out->z = v->z;
#else
#ifdef PLATFORM_VR
    if (rdCamera_UseVRProjection()) {
        rdCamera_PerspProjectVR(out, v);
        return;
    }
#endif
    flex_t fov_y_calc = (rdCamera_pCurCamera->fovDx / v->y);
    flex_t fov_x_calc = fov_y_calc; // This is the same because the clipping is what actually handles the aspect change
    out->x = rdCamera_pCurCamera->canvas->half_screen_width + (v->x * fov_x_calc);
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - (v->z * fov_y_calc);
    out->z = v->y;
#endif
    //printf("%f %f %f -> %f %f %f\n", v->x, v->y, v->z, out->x, out->y, out->z);
}

void rdCamera_PerspProjectLst(rdVector3 *pVerticesOut, const rdVector3 *pVerticesIn, unsigned int numVertices)
{
#ifdef TARGET_TWL
    // DSi does HW projection
    memcpy(pVerticesOut, pVerticesIn, numVertices * sizeof(rdVector3));
    return;
#endif

#ifdef PLATFORM_VR
    // DEBUG: Log whether VR projection is being used
    {
        extern int stdVR_currentEye;
        extern int stdVR_bEnabled;
        extern void VR_Log(const char* fmt, ...);
        static int checkCount[2] = {0, 0};
        int eye = stdVR_currentEye;
        if (stdVR_bEnabled && eye >= 0 && eye < 2) {
            checkCount[eye]++;
            int useVR = rdCamera_UseVRProjection();
            if (checkCount[eye] <= 3 || checkCount[eye] == 50 || checkCount[eye] == 500) {
                VR_Log("ProjLst[eye%d] #%d: UseVR=%d, numVerts=%d, bUsingVR=%d, tanL=%.3f tanR=%.3f tanU=%.3f tanD=%.3f\n",
                    eye, checkCount[eye], useVR, numVertices,
                    rdCamera_bUsingVRProjection, rdCamera_vrTanLeft, rdCamera_vrTanRight, rdCamera_vrTanUp, rdCamera_vrTanDown);
            }
        }
    }
    if (rdCamera_UseVRProjection()) {
        for (unsigned int i = 0; i < numVertices; i++)
        {
            rdCamera_PerspProjectVR(pVerticesOut, pVerticesIn);
            ++pVerticesIn;
            ++pVerticesOut;
        }
        return;
    }
#endif

    for (unsigned int i = 0; i < numVertices; i++)
    {
        rdCamera_PerspProject(pVerticesOut, pVerticesIn);
        ++pVerticesIn;
        ++pVerticesOut;
    }
}

#ifdef TARGET_TWL
void rdCamera_PerspProjectClip(rdVector3 *out, const rdVector3 *v)
{
    flex_t fov_y_calc = (rdCamera_pCurCamera->fovDx / v->y);
    flex_t fov_x_calc = fov_y_calc; // This is the same because the clipping is what actually handles the aspect change
    
    out->x = rdCamera_pCurCamera->canvas->half_screen_width + (v->x * fov_x_calc);
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - (v->z * fov_y_calc);
    out->z = v->y;
}

void rdCamera_PerspProjectLstClip(rdVector3 *pVerticesOut, const rdVector3 *pVerticesIn, unsigned int numVertices)
{
    for (unsigned int i = 0; i < numVertices; i++)
    {
        rdCamera_PerspProjectClip(pVerticesOut, pVerticesIn);
        ++pVerticesIn;
        ++pVerticesOut;
    }
}
#endif

void rdCamera_PerspProjectSquare(rdVector3 *out, const rdVector3 *v)
{
#ifdef TARGET_TWL
    // DSi does HW projection
    out->x = v->x;
    out->y = v->y;
    out->z = v->z;
#else
#ifdef PLATFORM_VR
    if (rdCamera_UseVRProjection()) {
        rdCamera_PerspProjectVR(out, v);
        return;
    }
#endif
    flex_t fov_y_calc = (rdCamera_pCurCamera->fovDx / v->y);
    out->x = rdCamera_pCurCamera->canvas->half_screen_width + (v->x * fov_y_calc);
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - (v->z * fov_y_calc);
    out->z = v->y;
#endif
}

void rdCamera_PerspProjectSquareLst(rdVector3 *pVerticesOut, const rdVector3 *pVerticesIn, unsigned int numVertices)
{
#ifdef TARGET_TWL
    memcpy(pVerticesOut, pVerticesIn, numVertices * sizeof(rdVector3));
    return;
#endif
#ifdef PLATFORM_VR
    if (rdCamera_UseVRProjection()) {
        for (unsigned int i = 0; i < numVertices; i++)
        {
            rdCamera_PerspProjectVR(pVerticesOut, pVerticesIn);
            ++pVerticesIn;
            ++pVerticesOut;
        }
        return;
    }
#endif
    for (unsigned int i = 0; i < numVertices; i++)
    {
        rdCamera_PerspProjectSquare(pVerticesOut, pVerticesIn);
        ++pVerticesIn;
        ++pVerticesOut;
    }
}

void rdCamera_SetAmbientLight(rdCamera *camera, flex_t amt)
{
    camera->ambientLight = amt;
}

void rdCamera_SetAttenuation(rdCamera *camera, flex_t minVal, flex_t maxVal)
{
    int numLights; // edx
    rdLight **v4; // ecx
    rdLight *v5; // eax

    numLights = camera->numLights;
    camera->attenuationMax = maxVal;
    camera->attenuationMin = minVal;
    if ( numLights )
    {
        v4 = camera->lights;
        do
        {
            v5 = *v4++;
            --numLights;
            v5->falloffMin = v5->intensity / minVal;
            v5->falloffMax = v5->intensity / maxVal;
        }
        while ( numLights );
    }
}

int rdCamera_AddLight(rdCamera *camera, rdLight *light, rdVector3 *lightPos)
{
    //sithRender_RenderDebugLight(light->intensity * 10.0, lightPos);
    if ( camera->numLights >= RDCAMERA_MAX_LIGHTS ) // Added: > to >=
        return 0;

    camera->lights[camera->numLights] = light;

    light->id = camera->numLights;
    rdVector_Copy3(&camera->lightPositions[camera->numLights], lightPos);
    light->falloffMin = light->intensity / camera->attenuationMin;
    light->falloffMax = light->intensity / camera->attenuationMax;

    ++camera->numLights;
    return 1;
}

int rdCamera_ClearLights(rdCamera *camera)
{
    camera->numLights = 0;
    return 1;
}

void rdCamera_AdvanceFrame()
{
    rdCanvas *v0; // eax
    rdRect a4; // [esp+0h] [ebp-10h] BYREF

    v0 = rdCamera_pCurCamera->canvas;
    if ( (rdroid_curRenderOptions & 0x100) != 0 && (v0->bIdk & 2) != 0 )
    {
        if ( rdroid_curAcceleration <= 0 )
        {
            if ( (v0->bIdk & 1) != 0 )
            {
                a4.x = v0->xStart;
                a4.y = v0->yStart;
                a4.width = v0->widthMinusOne - v0->xStart + 1;
                a4.height = v0->heightMinusOne - v0->yStart + 1;
                stdDisplay_VBufferFill(v0->d3d_vbuf, 0, &a4);
            }
            else
            {
                stdDisplay_VBufferFill(v0->d3d_vbuf, 0, 0);
            }
        }
        else
        {
            std3D_ClearZBuffer();
        }
    }
}

// MOTS added
flex_t rdCamera_GetMipmapScalar()
{
    return rdCamera_mipmapScalar;
}

// MOTS added
void rdCamera_SetMipmapScalar(flex_t val)
{
    rdCamera_mipmapScalar = val;
}

// Added: VR asymmetric projection support
#ifdef PLATFORM_VR
void rdCamera_SetVRProjection(float* proj16)
{
    if (proj16) {
        memcpy(rdCamera_vrProjection, proj16, 16 * sizeof(float));
        rdCamera_bUsingVRProjection = 1;
    }
}

void rdCamera_SetVRTangents(float tanLeft, float tanRight, float tanUp, float tanDown)
{
    rdCamera_vrTanLeft = tanLeft;
    rdCamera_vrTanRight = tanRight;
    rdCamera_vrTanUp = tanUp;
    rdCamera_vrTanDown = tanDown;
    rdCamera_bUsingVRProjection = 1;

    if (rdCamera_pCurCamera && rdCamera_pCurCamera->pClipFrustum) {
        rdClipFrustum* frustum = rdCamera_pCurCamera->pClipFrustum;
        frustum->farLeft = -tanLeft;
        frustum->right = tanRight;
        frustum->farTop = tanUp;
        frustum->bottom = -tanDown;
        frustum->nearLeft = frustum->farLeft;
        frustum->nearTop = frustum->farTop;
    }
}

void rdCamera_SetVRRenderDimensions(int width, int height)
{
    rdCamera_vrRenderWidth = width;
    rdCamera_vrRenderHeight = height;
}

void rdCamera_ClearVRProjection(void)
{
    rdCamera_bUsingVRProjection = 0;
    rdCamera_vrTanLeft = 0.0f;
    rdCamera_vrTanRight = 0.0f;
    rdCamera_vrTanUp = 0.0f;
    rdCamera_vrTanDown = 0.0f;
    rdCamera_vrRenderWidth = 0;
    rdCamera_vrRenderHeight = 0;
}

int rdCamera_IsVRProjectionActive(void)
{
    return rdCamera_bUsingVRProjection;
}

float* rdCamera_GetVRProjection(void)
{
    return rdCamera_vrProjection;
}
#endif // PLATFORM_VR
