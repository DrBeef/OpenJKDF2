#ifndef _STDVR_PROMPTS_H
#define _STDVR_PROMPTS_H

// Added: VR onboarding prompts. Teaches the motion/controller bindings that a player cannot
// discover by pressing buttons, phrased with whatever control scheme is actually configured
// (handedness and the swap-thumbsticks option). Each prompt is shown once per profile - see
// the STDVR_PROMPT_* ids and jkPlayer_vrPromptsShown.

#ifdef PLATFORM_VR

// Drive the prompt state machines. Call once per input tick; bInGameplay gates everything so
// prompts cannot appear over menus, briefings or cutscenes.
void stdVR_Prompts_Tick(int bInGameplay);

// Restart the in-session sequencing (prompts already taught stay taught).
void stdVR_Prompts_Reset(void);

#else

static inline void stdVR_Prompts_Tick(int bInGameplay) { (void)bInGameplay; }
static inline void stdVR_Prompts_Reset(void) {}

#endif // PLATFORM_VR

#endif // _STDVR_PROMPTS_H
