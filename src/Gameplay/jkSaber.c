#include "jkSaber.h"

#include "World/jkPlayer.h"
#include "Engine/rdroid.h"
#include "Engine/rdPuppet.h"
#include "Engine/sithAnimClass.h"
#include "World/sithSoundClass.h"
#include "Gameplay/sithTime.h"
#include "World/sithSurface.h"
#include "Engine/sithPuppet.h"
#include "Dss/sithMulti.h"
#include "World/sithTemplate.h"
#include "World/sithModel.h"
#include "Engine/sithKeyFrame.h"
#include "World/sithSector.h"
#include "Engine/sithCollision.h"
#include "Main/jkSmack.h"
#include "General/stdString.h"

#include "jk.h"

#ifdef PLATFORM_VR
#include "Platform/VR/stdVR.h"
#include "Engine/rdCamera.h"
#include "Engine/rdClip.h"
#include "Raster/rdCache.h"
#include "Engine/rdColormap.h"
#include "Gameplay/sithInventory.h"
#include "Cog/sithCog.h"

// VR saber debug: stores the last collision ray for debug line rendering
static rdVector3 jkSaber_vrDebugRayStart;
static rdVector3 jkSaber_vrDebugRayEnd;
static int jkSaber_vrDebugRayValid = 0;

// Solid material for debug line rendering (same pattern as rdDebug.c)
static rdTexture jkSaber_vrDebugTex = {0};
static rdTexinfo jkSaber_vrDebugTexinfo = { .header = {0}, .texext_unk00 = 0, .texture_ptr = &jkSaber_vrDebugTex };
static rdMaterial jkSaber_vrDebugMat = {0};

// VR saber swing state
static int jkSaber_vrSwingActive = 0;
static uint32_t jkSaber_vrSwingEndTime = 0;
#define JKSABER_VR_SWING_LINGER_MS 200  // Keep damage active briefly after swing slows
#endif

#define JKSABER_EXTENDTIME (0.3000000)

void jkSaber_InitializeSaberInfo(sithThing *thing, char *material_side_fname, char *material_tip_fname, flex_t base_rad, flex_t tip_rad, flex_t len, sithThing *wall_sparks, sithThing *blood_sparks, sithThing *saber_sparks)
{
    if (!thing) return; // Added: Fix nullptr deref in Mots cutscenes

    flex_t length = 0.0;
    jkPlayerInfo* saberinfo = thing->playerInfo;
    if ( saberinfo->polylineThing.polyline )
    {
        length = saberinfo->polyline.length;
        rdThing_FreeEntry(&saberinfo->polylineThing);
        rdPolyLine_FreeEntry(&saberinfo->polyline);
        saberinfo->polylineThing.polyline = 0;
    }

#ifdef DEBUG_QOL_CHEATS
    if (thing == sithPlayer_pLocalPlayerThing && !sithNet_isMulti) {
        //material_tip_fname = "saberpurple0.mat";
        //material_side_fname = "saberpurple1.mat";
    }
    if (thing == sithPlayer_pLocalPlayerThing) {
        //thing->jkFlags |= JKFLAG_DUALSABERS;
    }
#endif

    rdPolyLine_FreeEntry(&saberinfo->polyline); // Added: fix memleak
    rdPolyLine_NewEntry(&saberinfo->polyline, "Saber", material_side_fname, material_tip_fname, length, base_rad, tip_rad, RD_LIGHTMODE_4_UNK, 0, 0, 0.0);
    rdThing_NewEntry(&saberinfo->polylineThing, thing);
    rdThing_SetPolyline(&saberinfo->polylineThing, &saberinfo->polyline);
    saberinfo->wall_sparks = wall_sparks;
    saberinfo->blood_sparks = blood_sparks;
    saberinfo->saber_sparks = saber_sparks;
    saberinfo->length = len;
}

void jkSaber_PolylineRand(rdThing *thing)
{
    rdPolyLine* line = thing->polyline;
    if ( line )
    {
        if ( !(jkPlayer_currentTickIdx & 0xF) )
            line->edgeFace.clipIdk.y = 0.0;
        line->edgeFace.clipIdk.y += (_frand() - 0.8) * 80.0;
    }
}

void jkSaber_Draw(rdMatrix34 *posRotMat)
{
    if ( playerThings[playerThingIdx].actorThing->jkFlags & JKFLAG_SABERON
      && playerThings[playerThingIdx].povModel.model3
      && playerThings[playerThingIdx].polylineThing.model3 )
    {
        if ( playerThings[playerThingIdx].povModel.frameTrue != rdroid_frameTrue )
        {
            rdPuppet_BuildJointMatrices(&playerThings[playerThingIdx].povModel, posRotMat);
        }

        jkSaber_PolylineRand(&playerThings[playerThingIdx].polylineThing);
        rdThing_Draw(&playerThings[playerThingIdx].polylineThing, &playerThings[playerThingIdx].povModel.hierarchyNodeMatrices[5]); // aaaaa hardcoded K_Rhand
        
        // Added: Dual sabers
        if (playerThings[playerThingIdx].actorThing->jkFlags & JKFLAG_DUALSABERS)
            rdThing_Draw(&playerThings[playerThingIdx].polylineThing, &playerThings[playerThingIdx].povModel.hierarchyNodeMatrices[2]); // K_Lhand
    }
}

void jkSaber_UpdateLength(sithThing *thing)
{
    jkPlayerInfo* playerInfo = thing->playerInfo;
    if (!playerInfo )
    {
        thing->jkFlags &= ~JKFLAG_SABERON;
        return;
    }

    if (!(thing->jkFlags & JKFLAG_SABERON)) {
        playerInfo->polyline.length = 0;
        return; // Added: Wanted more logic in jkSaber_UpdateLength
    }

#if 0
    printf("Saber state: ");
    if (thing->jkFlags & JKFLAG_SABERON) {
        printf("ON ");
    }
    if (thing->jkFlags & JKFLAG_SABERDAMAGE) {
        printf("DAMAGE ");
    }
    if (thing->jkFlags & JKFLAG_SABEREXTEND) {
        printf("EXTEND ");
    }
    if (thing->jkFlags & JKFLAG_SABERRETRACT) {
        printf("RETRACT ");
    }
    if (thing->jkFlags & JKFLAG_DUALSABERS) {
        printf("DUALSABERS ");
    }
    if (thing->jkFlags & JKFLAG_SABERFORCEON) {
        printf("FORCEON ");
    }
    printf(" len=%f %f\n", playerInfo->polyline.length, thing->actorParams.timeLeftLengthChange);
#endif

    if (thing->thingflags & SITH_TF_DEAD || thing->type == SITH_THING_CORPSE)
    {
        thing->jkFlags |= JKFLAG_SABERRETRACT;
    }

    // Added: HACK fix a bug where the saber gets stuck extended.
    if ((thing->jkFlags & (JKFLAG_SABEREXTEND | JKFLAG_SABERRETRACT)) == (JKFLAG_SABEREXTEND | JKFLAG_SABERRETRACT))
    {
        thing->jkFlags &= ~JKFLAG_SABERRETRACT;
        playerInfo->polyline.length = 0;
    }

    if ( thing->jkFlags & JKFLAG_SABEREXTEND)
    {
        flex_t newLength = playerInfo->polyline.length + (sithTime_deltaSeconds * JKSABER_EXTENDTIME);
        flex_t deltaLen = newLength / playerInfo->length;

        thing->jkFlags &= ~JKFLAG_SABERRETRACT;

        playerInfo->polyline.length = newLength;
        thing->actorParams.timeLeftLengthChange = deltaLen * (1.0 - JKSABER_EXTENDTIME);
        if (newLength >= playerInfo->length) // ? verify, IDA crapped out on this comparison
        {
            playerInfo->polyline.length = playerInfo->length;
            thing->actorParams.timeLeftLengthChange = (1.0 - JKSABER_EXTENDTIME);
            thing->jkFlags &= ~(JKFLAG_SABERRETRACT | JKFLAG_SABEREXTEND);
        }
    }
    else if ( thing->jkFlags & JKFLAG_SABERRETRACT )
    {
        flex_t newLength = playerInfo->polyline.length - (sithTime_deltaSeconds * JKSABER_EXTENDTIME);
        flex_t deltaLen = newLength / playerInfo->length;

        thing->jkFlags &= ~JKFLAG_SABEREXTEND;

        playerInfo->polyline.length = newLength;
        thing->actorParams.timeLeftLengthChange = deltaLen * (1.0 - JKSABER_EXTENDTIME);
        if ( newLength <= 0.0 ) // ? verify, IDA crapped out on this comparison
        {
            playerInfo->polyline.length = 0.0;
            thing->jkFlags &= ~(JKFLAG_SABEREXTEND | JKFLAG_SABERRETRACT | JKFLAG_SABERON);
            thing->actorParams.timeLeftLengthChange = 0.0;
        }
    }
    else if (thing->jkFlags & JKFLAG_SABERFORCEON) // Used for starting a level with the saber on, ie DF2 lv4
    {
        playerInfo->polyline.length = playerInfo->length;
        thing->actorParams.timeLeftLengthChange = (1.0 - JKSABER_EXTENDTIME);
        thing->jkFlags &= ~(JKFLAG_SABERRETRACT | JKFLAG_SABEREXTEND);
        thing->jkFlags |= JKFLAG_SABERON;

        // Added? I think my RETRACT | EXTEND fix inavertently exposed a bug
        thing->jkFlags &= ~JKFLAG_SABERFORCEON;
    }

    if ( thing->animclass->bodypart_to_joint[JOINTTYPE_PRIMARYWEAP] >= 0 )
    {
        jkSaber_UpdateCollision(thing, thing->animclass->bodypart_to_joint[JOINTTYPE_PRIMARYWEAP], 0); // MOTS added: last arg
        if ( thing->jkFlags & JKFLAG_DUALSABERS )
        {
            if ( thing->animclass->bodypart_to_joint[JOINTTYPE_SECONDARYWEAP] >= 0 )
                jkSaber_UpdateCollision(thing, thing->animclass->bodypart_to_joint[JOINTTYPE_SECONDARYWEAP], 1); // MOTS added: last arg
        }
    }
}

// MOTS added: split into its own func
void  jkSaber_UpdateCollision2(sithThing *pPlayerThing,rdVector3 *pSaberPos,rdVector3 *pSaberDir,jkSaberCollide *pCollideInfo)
{
    sithSector *pSector;
    sithCollisionSearchEntry *searchResult;
    sithThing *resultThing;
    rdVector3 local_54;
    rdVector3 local_3c;
    jkPlayerInfo *playerInfo;
    rdMatrix34 tmpMat;
    
    playerInfo = pPlayerThing->playerInfo;
    pSector = sithCollision_GetSectorLookAt(pPlayerThing->sector,&pPlayerThing->position,pSaberPos,0.0);
    if (!pSector) {
        return;
    }
    sithCollision_SearchRadiusForThings(pSector,pPlayerThing,pSaberPos,pSaberDir,pCollideInfo->bladeLength,0.0,0);
    

    sithSector* pSectorIter = pSector;
    while (1) 
    {
        searchResult = sithCollision_NextSearchResult();
        if (!searchResult)
            break;

        if (searchResult->hitType & SITHCOLLISION_ADJOINCROSS)
        {
            pSectorIter = searchResult->surface->adjoin->sector;
        }
        else if (searchResult->hitType & SITHCOLLISION_THING) 
        {
            rdVector_Copy3(&local_54, pSaberPos);
            rdVector_MultAcc3(&local_54, pSaberDir, searchResult->distance);

            resultThing = searchResult->receiver;

            if ( resultThing->type == SITH_THING_ITEM || resultThing->type == SITH_THING_EXPLOSION || resultThing->type == SITH_THING_PARTICLE )
            {
                continue;
            }
            if (resultThing->actorParams.typeflags & SITH_AF_DROID 
                || resultThing->type != SITH_THING_ACTOR && resultThing->type != SITH_THING_PLAYER )
            {
                jkSaber_SpawnSparks(playerInfo, &local_54, pSectorIter, SPARKTYPE_WALL);
            }
            if ( pCollideInfo->numDamagedThings == 6 )
            {
                break;
            }

            int foundIdx = 0;
            for (foundIdx = 0; foundIdx < pCollideInfo->numDamagedThings; foundIdx++ )
            {
                if ( searchResult->receiver == pCollideInfo->damagedThings[foundIdx] )
                    break;
            }

            if ( foundIdx < pCollideInfo->numDamagedThings )
            {
                break;
            }

            if ( resultThing->type != SITH_THING_ACTOR 
                 && resultThing->type != SITH_THING_PLAYER 
                 || !(resultThing->actorParams.typeflags & SITH_AF_BLEEDS) )
            {
                jkSaber_SpawnSparks(playerInfo, &local_54, pSectorIter, SPARKTYPE_BLOOD);

                sithThing_Damage(searchResult->receiver, pPlayerThing, pCollideInfo->damage, SITH_DAMAGE_SABER);
                pCollideInfo->damagedThings[pCollideInfo->numDamagedThings++] = searchResult->receiver;
                break;
            }

            // TODO is this a vector func?
            rdVector_Sub3(&local_3c, &local_54, &resultThing->position);
            rdVector_Normalize3Acc(&local_3c);
            rdMatrix_Copy34(&tmpMat, &resultThing->lookOrientation);
            if ( resultThing->type == SITH_THING_ACTOR || resultThing->type == SITH_THING_PLAYER )
                rdMatrix_PreRotate34(&tmpMat, &resultThing->actorParams.eyePYR);
                
            // TODO: is this a vector func?
            rdVector3 v52 = tmpMat.lvec;
            rdVector_Normalize3Acc(&v52);
            if ( rdVector_Dot3(&v52, &local_3c) >= resultThing->actorParams.fov
              && (_frand() < resultThing->actorParams.chance) )
            {
                if (!(pPlayerThing->actorParams.typeflags & SITH_AF_INVISIBLE)) // verify
                {
                    sithSoundClass_PlayModeRandom(pPlayerThing, SITH_SC_DEFLECTED);

                    if ( _frand() >= 0.5 )
                        sithPuppet_PlayMode(resultThing, SITH_ANIM_BLOCK2, 0);
                    else
                        sithPuppet_PlayMode(resultThing, SITH_ANIM_BLOCK, 0);

                    jkSaber_SpawnSparks(playerInfo, &local_54, pSectorIter, SPARKTYPE_SABER);

                    sithCog_SendMessageFromThing(resultThing, 0, SITH_MESSAGE_BLOCKED);
                    pCollideInfo->damagedThings[pCollideInfo->numDamagedThings++] = searchResult->receiver;
                    break;
                }
            }

            jkSaber_SpawnSparks(playerInfo, &local_54, pSectorIter, SPARKTYPE_BLOOD);

            sithThing_Damage(resultThing, pPlayerThing, pCollideInfo->damage, SITH_DAMAGE_SABER);
            pCollideInfo->damagedThings[pCollideInfo->numDamagedThings++] = searchResult->receiver;
            break;
        }
        else if (searchResult->hitType & SITHCOLLISION_WORLD)
        {
            rdVector_Copy3(&local_54, pSaberPos);
            rdVector_MultAcc3(&local_54, pSaberDir, searchResult->distance - 0.001);
            
            jkSaber_SpawnSparks(playerInfo, &local_54, pSectorIter, SPARKTYPE_WALL);

            if ( pCollideInfo->numDamagedSurfaces < 6 )
            {
                int surfaceNum = 0;
                for ( surfaceNum = 0; surfaceNum < pCollideInfo->numDamagedSurfaces; surfaceNum++ )
                {
                    if ( searchResult->surface == pCollideInfo->damagedSurfaces[surfaceNum] )
                        break;
                }
                if ( surfaceNum >= pCollideInfo->numDamagedSurfaces )
                {
                    sithSurface_SendDamageToThing(searchResult->surface, pPlayerThing, pCollideInfo->damage, SITH_DAMAGE_SABER);
                    pCollideInfo->damagedSurfaces[pCollideInfo->numDamagedSurfaces++] = searchResult->surface;
                }
            }
            break;
        }
    }
    sithCollision_SearchClose();
}

// MOTS altered: interpolation and multiple blades
void jkSaber_UpdateCollision(sithThing *player, int joint, int bSecondary)
{
    jkPlayerInfo *playerInfo; // ebx
    rdVector3 a2a;
    rdMatrix34 jointMat;
    rdVector3 jointPos;
    rdMatrix34 matrix;
    rdMatrix34 tmpMat;
    rdMatrix34 lastJointMat;
    rdVector3 lerpSaberDir;
    rdVector3 lerpSaberPos;
    rdVector3 lerpPosDelta;
    rdVector3 lerpDirDelta;
    rdMatrix34 *pWhichLastJointMat;

    playerInfo = player->playerInfo;

#ifdef PLATFORM_VR
    // VR Motion Saber: use controller position/orientation instead of animation joint
    if (stdVR_bEnabled && stdVR_motionConfig.bMotionSaberEnabled
        && player == sithPlayer_pLocalPlayerThing && !bSecondary)
    {
        int hand = stdVR_GetDominantHand();
        stdVR_ControllerState* pCtrl = stdVR_GetController(hand);
        if (pCtrl && pCtrl->bTracking)
        {
            // Get controller world matrix as the model base
            // (same orientation as stdVR_GetControllerViewMatrix, no -90 pitch offset)
            rdMatrix34 controllerMat;
            if (!stdVR_GetSaberWorldMatrix(hand, &controllerMat)) return;

            // Build POV model joint matrices from controller matrix, then use
            // K_Rhand joint (5) - this matches exactly where jkSaber_Draw
            // renders the blade visual.
            rdPuppet_BuildJointMatrices(&playerThings[playerThingIdx].povModel, &controllerMat);
            rdMatrix_Copy34(&jointMat, &playerThings[playerThingIdx].povModel.hierarchyNodeMatrices[5]);
            // Normalize orientation (joint matrices may carry model scale)
            rdVector_Normalize3Acc(&jointMat.rvec);
            rdVector_Normalize3Acc(&jointMat.lvec);
            rdVector_Normalize3Acc(&jointMat.uvec);

            // Store blade tip position for game systems
            rdVector_Copy3(&player->actorParams.saberBladePos, &jointMat.scale);
            rdVector_MultAcc3(&player->actorParams.saberBladePos, &jointMat.lvec, playerInfo->polyline.length);

            // Store debug ray
            rdVector_Copy3(&jkSaber_vrDebugRayStart, &jointMat.scale);
            rdVector_Copy3(&jkSaber_vrDebugRayEnd, &player->actorParams.saberBladePos);
            jkSaber_vrDebugRayValid = 1;

            // Handle swing-based damage activation
            float swingSpeed = stdVR_GetSwingSpeed();
            if (swingSpeed >= stdVR_motionConfig.saberVelocityTrigger)
            {
                if (!jkSaber_vrSwingActive)
                {
                    // Swing just started - reset damage tracking
                    playerInfo->saberCollideInfo.numDamagedThings = 0;
                    playerInfo->saberCollideInfo.numDamagedSurfaces = 0;

                    // Trigger the weapon COG's fire handler to play swing sound + animation
                    // The COG handles PlaySoundThing + PlayMode internally
                    int curWeapon = sithInventory_GetCurWeapon(player);
                    sithItemDescriptor* pWeaponDesc = sithInventory_GetBinByIdx(curWeapon);
                    if (pWeaponDesc && pWeaponDesc->cog) {
                        sithCog_SendMessageEx(pWeaponDesc->cog, SITH_MESSAGE_FIRE,
                            SENDERTYPE_SYSTEM, 0,
                            SENDERTYPE_THING, player->thingIdx,
                            0, 0.0, 0.0, 0.0, 0.0);
                    }
                }
                jkSaber_vrSwingActive = 1;
                jkSaber_vrSwingEndTime = sithTime_curMs + JKSABER_VR_SWING_LINGER_MS;
            }
            else if (jkSaber_vrSwingActive && sithTime_curMs >= jkSaber_vrSwingEndTime)
            {
                // Swing ended
                jkSaber_vrSwingActive = 0;
            }

            // Only do collision if swing is active
            if (jkSaber_vrSwingActive)
            {
                // Use COG-set damage if available, otherwise default saber damage
                flex_t damage = playerInfo->saberCollideInfo.damage;
                if (damage <= 0.0f) damage = 100.0f;
                playerInfo->saberCollideInfo.damage = damage;
                playerInfo->saberCollideInfo.bladeLength = playerInfo->polyline.length;
                playerInfo->saberCollideInfo.field_1A4 = 1;

                // Temporal interpolation (reuse existing MOTS logic)
                rdVector_Copy3(&jointPos, &jointMat.scale);
                rdVector_Copy3(&a2a, &jointMat.lvec);

                if (sithTime_deltaSeconds > 0.032f && playerInfo->bHasLastJointMat)
                {
                    pWhichLastJointMat = &playerInfo->lastSaberJointMat;
                    flex_t fVar1 = sithTime_TickHz * 0.032f;
                    rdMatrix_Copy34(&lastJointMat, pWhichLastJointMat);
                    rdVector_Sub3(&lerpPosDelta, &jointMat.scale, &lastJointMat.scale);
                    rdVector_Sub3(&lerpDirDelta, &jointMat.lvec, &lastJointMat.lvec);
                    flex_t stepAmount = fVar1;
                    for (; fVar1 < 1.0f; fVar1 += stepAmount) {
                        rdVector_Copy3(&lerpSaberPos, &lastJointMat.scale);
                        rdVector_MultAcc3(&lerpSaberPos, &lerpPosDelta, fVar1);
                        rdVector_Copy3(&lerpSaberDir, &lastJointMat.lvec);
                        rdVector_MultAcc3(&lerpSaberDir, &lerpDirDelta, fVar1);
                        jkSaber_UpdateCollision2(player, &lerpSaberPos, &lerpSaberDir, &playerInfo->saberCollideInfo);
                    }
                }
                jkSaber_UpdateCollision2(player, &jointPos, &a2a, &playerInfo->saberCollideInfo);
            }

            // Store for next frame interpolation
            rdMatrix_Copy34(&playerInfo->lastSaberJointMat, &jointMat);
            playerInfo->bHasLastJointMat = 1;
            return;  // Skip the animation-based path below
        }
    }
#endif

    rdMatrix_Copy34(&matrix, &player->lookOrientation);
    rdVector_Copy3(&matrix.scale, &player->position);
    if ( jkSmack_GetCurrentGuiState() == 6 ) {
        rdPuppet_BuildJointMatrices(&player->rdthing, &matrix);
    }

    if ( !rdModel3_GetMeshMatrix(&player->rdthing, &matrix, joint, &jointMat) )
        return;

    rdVector_Copy3(&player->actorParams.saberBladePos, &jointMat.scale);
    rdVector_MultAcc3(&player->actorParams.saberBladePos, &jointMat.lvec, playerInfo->polyline.length);

    if ( player->jkFlags & JKFLAG_40 )
    {
        player->jkFlags &= ~JKFLAG_40;
        playerInfo->saberCollideInfo.numDamagedThings = 0;
        playerInfo->saberCollideInfo.numDamagedSurfaces = 0;
    }
    if ( !(player->jkFlags & JKFLAG_SABERDAMAGE) )
        return;
    if ( !playerInfo->saberCollideInfo.field_1A4 )
        return;
    
    // Always do ticked saber collision
#ifndef QOL_IMPROVEMENTS
    if (!Main_bMotsCompat) {
        jkSaber_UpdateCollision2(player,&jointMat.scale, &jointMat.lvec, &playerInfo->saberCollideInfo);
        return;
    }
#endif
    
    // MOTS added: interpolation at low FPS
    // This ensures that saber collision is *at least* 20fps-quality,
    // Added: QoL modified to be at least 30fps-quality collisions for DSi
#ifdef QOL_IMPROVEMENTS
    const flex_t saberMinDelta = 0.032; // 30FPS-ish
#else
    const flex_t saberMinDelta = 0.05; // 20FPS
#endif
    rdVector_Copy3(&jointPos, &jointMat.scale);
    rdVector_Copy3(&a2a, &jointMat.lvec);
    if (sithTime_deltaSeconds > saberMinDelta && playerInfo->bHasLastJointMat) 
    {
        pWhichLastJointMat = &playerInfo->lastSaberJointMat;
        if (bSecondary != 0) {
            pWhichLastJointMat = &playerInfo->lastSecondarySaberJointMat;
        }
        flex_t fVar1 = sithTime_TickHz * saberMinDelta;
        rdMatrix_Copy34(&lastJointMat, pWhichLastJointMat);

        rdVector_Sub3(&lerpPosDelta, &jointMat.scale, &lastJointMat.scale);
        rdVector_Sub3(&lerpDirDelta, &jointMat.lvec, &lastJointMat.lvec);
        flex_t stepAmount = fVar1;

        // This will step 0 times at 20fps, once at 10fps, twice at 5fps, etc
        for (; fVar1 < 1.0; fVar1 += stepAmount) {
            rdVector_Copy3(&lerpSaberPos, &lastJointMat.scale);
            rdVector_MultAcc3(&lerpSaberPos, &lerpPosDelta, fVar1);

            rdVector_Copy3(&lerpSaberDir, &lastJointMat.lvec);
            rdVector_MultAcc3(&lerpSaberDir, &lerpDirDelta, fVar1);

            jkSaber_UpdateCollision2(player,&lerpSaberPos,&lerpSaberDir,&playerInfo->saberCollideInfo);
        }
    }
    jkSaber_UpdateCollision2(player,&jointPos,&a2a,&playerInfo->saberCollideInfo);
    pWhichLastJointMat = &playerInfo->lastSaberJointMat;
    if (bSecondary != 0) {
        pWhichLastJointMat = &playerInfo->lastSecondarySaberJointMat;
    }

    // Store the joint matrix so we can get a delta for the next frame
    rdMatrix_Copy34(pWhichLastJointMat, &jointMat);
    playerInfo->bHasLastJointMat = 1;
}

void jkSaber_SpawnSparks(jkPlayerInfo *pPlayerInfo, rdVector3 *pPos, sithSector *psector, int sparkType)
{
    sithThing *pTemplate; // eax
    sithThing *pSpawned; // eax

    if ( sithTime_curMs < pPlayerInfo->lastSparkSpawnMs + 200 )
        return;

    if ( sparkType == SPARKTYPE_BLOOD )
    {
        pTemplate = pPlayerInfo->blood_sparks;
    }
    else if ( sparkType == SPARKTYPE_SABER )
    {
        pTemplate = pPlayerInfo->saber_sparks;
    }
    else // SPARKTYPE_WALL
    {
        pTemplate = pPlayerInfo->wall_sparks;
    }
    if ( pTemplate )
    {
        pSpawned = sithThing_Create(pTemplate, pPos, &rdroid_identMatrix34, psector, 0);
        if ( pSpawned )
        {
            pSpawned->prev_thing = pPlayerInfo->actorThing;
            pPlayerInfo->lastSparkSpawnMs = sithTime_curMs;
            pSpawned->child_signature = pPlayerInfo->actorThing->signature;
        }
    }
}

// MOTS altered
void jkSaber_Enable(sithThing *pThing, flex_t damage, flex_t bladeLength, flex_t stunDelay)
{
    if (!pThing || !pThing->playerInfo) return; // MOTS added

    pThing->playerInfo->saberCollideInfo.damage = damage;
    pThing->playerInfo->saberCollideInfo.bladeLength = bladeLength;
    pThing->playerInfo->saberCollideInfo.stunDelay = stunDelay;
    pThing->playerInfo->saberCollideInfo.field_1A4 = 1;
    pThing->playerInfo->saberCollideInfo.numDamagedThings = 0;
    pThing->playerInfo->saberCollideInfo.numDamagedSurfaces = 0;

    _memset(pThing->playerInfo->saberCollideInfo.damagedThings, 0, sizeof(pThing->playerInfo->saberCollideInfo.damagedThings));
    _memset(pThing->playerInfo->saberCollideInfo.damagedSurfaces, 0, sizeof(pThing->playerInfo->saberCollideInfo.damagedSurfaces));
    
    pThing->playerInfo->lastSparkSpawnMs = 0;

#ifdef JKM_SABER
    pThing->playerInfo->bHasLastJointMat = 0; // MOTS added
#endif
}

// MOTS altered
void jkSaber_Disable(sithThing *player)
{
    //MOTS added:
    if (!player || !player->playerInfo) return;

    player->playerInfo->saberCollideInfo.field_1A4 = 0;
#ifdef JKM_SABER
    player->playerInfo->bHasLastJointMat = 0; // MOTS added
#endif
}

#ifdef PLATFORM_VR
// Draw VR saber collision debug ray as a visible thin quad through rdCache.
// Uses camera-space quad (4 verts) instead of GL_LINES (2 verts) to avoid
// the disabled GL_LINES rendering path and MultiView line issues.
void jkSaber_DrawVRDebugLine(void)
{
#ifdef VR_SABER_DEBUG_LINE
    if (!jkSaber_vrDebugRayValid) return;
    if (!rdCamera_pCurCamera || !rdCamera_pCurCamera->pClipFrustum) return;

    // Transform world-space ray endpoints to camera space
    rdVector3 verts[2];
    rdMatrix_TransformPoint34(&verts[0], &jkSaber_vrDebugRayStart, &rdCamera_pCurCamera->view_matrix);
    rdMatrix_TransformPoint34(&verts[1], &jkSaber_vrDebugRayEnd, &rdCamera_pCurCamera->view_matrix);

    // Clip line to view frustum (modifies verts in-place)
    int out1, out2;
    if (!rdClip_Line3Project(rdCamera_pCurCamera->pClipFrustum, &verts[0], &verts[1], &out1, &out2))
        return;

    // Project to screen coordinates
    rdVector3 screenVerts[2];
    rdCamera_pCurCamera->fnProjectLst(screenVerts, verts, 2);

    // Build a thin screen-space quad from the 2 projected endpoints
    // Compute perpendicular offset in screen space for line width
    float dx = screenVerts[1].x - screenVerts[0].x;
    float dy = screenVerts[1].y - screenVerts[0].y;
    float len = stdMath_Sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;

    // Perpendicular direction, scaled to ~1.5 pixels half-width
    float halfWidth = 1.5f;
    float px = (-dy / len) * halfWidth;
    float py = (dx / len) * halfWidth;

    // Average Z for depth sorting
    float avgZ = (screenVerts[0].z + screenVerts[1].z) * 0.5f;

    rdProcEntry* procEntry = rdCache_GetProcEntry();
    if (!procEntry) return;

    // 4 vertices forming a thin quad
    procEntry->vertices[0].x = screenVerts[0].x - px;
    procEntry->vertices[0].y = screenVerts[0].y - py;
    procEntry->vertices[0].z = avgZ;

    procEntry->vertices[1].x = screenVerts[0].x + px;
    procEntry->vertices[1].y = screenVerts[0].y + py;
    procEntry->vertices[1].z = avgZ;

    procEntry->vertices[2].x = screenVerts[1].x + px;
    procEntry->vertices[2].y = screenVerts[1].y + py;
    procEntry->vertices[2].z = avgZ;

    procEntry->vertices[3].x = screenVerts[1].x - px;
    procEntry->vertices[3].y = screenVerts[1].y - py;
    procEntry->vertices[3].z = avgZ;

    // Setup solid material (same pattern as rdDebug_DrawScreenLine3)
    jkSaber_vrDebugMat.num_texinfo = 8;
    jkSaber_vrDebugMat.celIdx = 0;
    for (int i = 0; i < 8; i++) {
        jkSaber_vrDebugMat.texinfos[i] = &jkSaber_vrDebugTexinfo;
    }

    // Configure as unlit, solid color green quad
    procEntry->textureMode = 0;
    procEntry->geometryMode = RD_GEOMODE_SOLIDCOLOR;
    procEntry->lightingMode = RD_LIGHTMODE_FULLYLIT;
    procEntry->light_flags = 0;
    procEntry->wallCel = 0;
    procEntry->type = 0;
    procEntry->extralight = 1.0f;
    procEntry->material = &jkSaber_vrDebugMat;
    procEntry->colormap = rdColormap_pCurMap;

    // Set vertex intensities and UVs for 4 vertices
    for (int i = 0; i < 4; i++) {
        procEntry->vertexIntensities[i] = 1.0f;
        procEntry->vertexUVs[i].x = 0.0f;
        procEntry->vertexUVs[i].y = 0.0f;
    }

    // Green color: 0x00FF00 with full alpha
    rdCache_AddProcFace(0xFF00FF00, 4, 0x7);
#endif
}
#endif