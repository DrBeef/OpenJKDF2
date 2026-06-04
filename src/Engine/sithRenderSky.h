#ifndef _SITHRENDERSKY_H
#define _SITHRENDERSKY_H

#include "types.h"
#include "globals.h"

#define sithRenderSky_Open_ADDR (0x004F2D30)
#define sithRenderSky_Close_ADDR (0x004F2DC0)
#define sithRenderSky_Update_ADDR (0x004F2DD0)
#define sithRenderSky_TransformHorizontal_ADDR (0x004F2E30)
#define sithRenderSky_TransformVertical_ADDR (0x004F2F60)

MATH_FUNC int sithRenderSky_Open(flex_t horizontalPixelsPerRev, flex_t horizontalDist, flex_t ceilingSky);
void sithRenderSky_Close();
MATH_FUNC void sithRenderSky_Update();
MATH_FUNC void sithRenderSky_TransformHorizontal(rdProcEntry *pProcEntry, sithSurfaceInfo *pSurfaceInfo, uint32_t num_vertices);
MATH_FUNC void sithRenderSky_TransformVertical(rdProcEntry *pProcEntry, sithSurfaceInfo *pSurfaceInfo, rdVector3 *pUntransformedVerts, uint32_t num_vertices);

#ifdef PLATFORM_VR
// Renders a real world-fixed sky dome for the VR (GPU per-eye projection) path. No-op unless
// rdCamera_bGpuProjection is set. Replaces the 2D screen-space sky in VR.
void sithRenderSky_DrawVRDome(void);
#endif

#endif // _SITHRENDERSKY_H