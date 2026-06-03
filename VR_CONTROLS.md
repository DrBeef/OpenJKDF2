# OpenJKDF2 VR Controls

Default layout is right-handed (dominant hand = right controller). A left-handed
layout is available — set **Dominant Hand = Left** in the VR options menu and
all dominant/off-hand actions swap controllers; the weapon model aligns to the
left controller and projectiles spawn from the left-hand barrel.

```
LEFT CONTROLLER (Off-Hand)                RIGHT CONTROLLER (Dominant Hand)
==============================            ================================

    [Y] Open Menu                             [B] Alt Fire
    [X] Activate / Use                        [A] Jump / Swim Up

    [Trigger] Use Force Power                 [Trigger] Primary Fire
    [Grip]    Force Wheel (hold)              [Grip]    Weapon Wheel (hold)

        /---\                                     /---\
       /     \  Move                             /     \  Turn
      | Stick |  (forward/back/                 | Stick |  (snap or smooth)
       \     /    strafe)                        \     /
        \---/                                     \---/
       [Click]  Toggle Walk/Run                  [Click]  Toggle 3D HoloMap

          Thumbstick Up:    (movement)            Thumbstick Up:    Next Weapon
          Thumbstick Down:  (movement)            Thumbstick Down:  Toggle Crouch

    [Menu] Short Press = Menu/Escape
           Long Press  = Recenter View
```

## Right Controller (Dominant Hand)

| Input | Action |
|-------|--------|
| **Trigger** | Primary Fire (shoot, swing saber) |
| **Grip** | Weapon Wheel (hold to open, point to select, release to confirm) |
| **A Button** | Jump / Swim Up |
| **B Button** | Alt Fire (secondary fire mode) |
| **Thumbstick Left/Right** | Turn (snap or smooth, configurable) |
| **Thumbstick Up** | Next Weapon (flick) |
| **Thumbstick Down** | Toggle Crouch / Swim Down |
| **Thumbstick Click** | Toggle 3D HoloMap |

## Left Controller (Off-Hand)

| Input | Action |
|-------|--------|
| **Trigger** | Use Force Power |
| **Grip** | Force Wheel (hold to open, point to select, release to confirm) |
| **X Button** | Activate / Use |
| **Y Button** | Open Menu / Escape |
| **Thumbstick** | Move (forward/back/strafe) |
| **Thumbstick Click** | Toggle Walk/Run |

## Menu Button

The Quest's system menu button is intercepted by the OS, so the in-game menu
can also be opened with the **Y button** at any time during gameplay.

| Input | Action |
|-------|--------|
| **Short Press (system menu button)** | Open Menu / Escape |
| **Long Press (1 sec, system menu button)** | Recenter View |
| **Y Button** | Open Menu / Escape (alternative; no recenter) |

## Weapon & Force Wheels

Hold **Grip** on either controller to open a selection wheel:
- **Dominant Grip** = Weapon Wheel (shows available weapons as 3D models)
- **Off-Hand Grip** = Force Wheel (shows available force powers)

While the wheel is open, point the controller to highlight a segment, then release grip to select. The game slows to 10% speed while a wheel is active.

## 3D HoloMap

Click **Right Thumbstick** to toggle the holographic map.

While the map is visible:
- **Left Thumbstick** rotates the map
- **Right Thumbstick Y** zooms in/out

## Cheat Combo

| Input | Action |
|-------|--------|
| **Both Grips + Both Triggers (hold 1.5 sec)** | All weapons, force powers, full health |

## Weapon Alignment Tool (Development Only)

When `VR_WEAPON_ALIGNMENT_TOOL` is enabled in `engine_config.h`:

| Input | Action |
|-------|--------|
| **Both Grips + B** | Toggle alignment mode (weapon switching disabled) |
| **A Button** | Cycle mode: Position -> Scale -> Pitch |
| **Left Stick X** | Adjust X offset (left/right) |
| **Left Stick Y** | Adjust Y offset (forward/back) |
| **Right Stick Y** | Adjust Z offset (up/down) / Scale / Pitch depending on mode |
| **Both Grips + B** | Save current weapon offsets and exit |

Saved offsets are stored in `jkdf2xr_vr_weapons.json` and apply even without the alignment tool compiled in.
