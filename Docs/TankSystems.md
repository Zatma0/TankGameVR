# VR Tank: steering, cockpit, lock-on aiming, Aim Assist

These are the systems for my part of the game: joystick tank steering, the placeholder cockpit with two sticks and a
display, Dark Souls-style target cycling, the moving crosshair, and the Aim Assist setting.
The project is UE 5.8.3 and the VR Template is used unchanged. `BP_XRPawn` and its grab system are reused as they are.

## Try it

- **Headset:** open `Content/TankGame/Maps/LV_TankTest` and press **VR Preview**. The VR pawn boards the tank's seat
  automatically.
- **Without a headset:** press **Play**. You sit in the cockpit at eye height (about 1 s after start); drive with
  WASD, aim with the arrow keys, press **C** for the overview camera. Click into the game window first.
- **Watching on the monitor during VR:** press **Tab** in the game window. This toggles the VR Template spectator
  camera; move it with WASD/QE and the mouse.

## Controls

| Action | VR (in cockpit) | Controller fallback | Keyboard fallback |
|---|---|---|---|
| Drive: push right/left = curve right/left, forward/back = throttle/reverse | grip the **left (blue) cockpit stick** and tilt it | left thumbstick | W A S D |
| Aim: flick right/left = next target on that side; with Aim Assist off, moves the aim | grip the **right (orange) cockpit stick** | right thumbstick | arrow keys |
| Aim Assist On/Off | VR menu (B / Menu button) → **Aim Assist: On/Off** | | |
| Cockpit view ↔ overview camera | X button (Quest/PICO left), B (Index left) | gamepad Y | C |

The overview camera hangs about 15 m behind and above the tank, follows its heading with a little lag and shows a
status panel (speed, turn rate, Aim Assist, current target, targets in the area). With a headset the switch is an
instant cut; on a monitor it blends in 0.35 s (`ViewBlendTime`). The boom (`OverviewArm`) and panel class
(`OverviewHudClass`) can be changed in `BP_PlayerTank`. The status panel is a screen widget, so it shows on the
monitor, not inside the headset.

A held cockpit stick always wins. When it is not held, the thumbstick drives the stick and its visual tilt.
While seated, `IMC_Tank` (priority 10) takes over the thumbstick axes that the template uses for teleport and snap
turn, so the tank never teleports.

## Architecture

```
BP_XRPawn (template, unchanged) ──boards──> UTankSeatComponent (cockpit floor = rider's tracking origin)
BP_GrabComponent (template, Grab Type = Custom) on each stick knob
        │ bIsHeld / MotionControllerRef (read via reflection)
UCockpitJoystickComponent ×2 ──> ATankVehicle (merges stick / thumbstick, owns the parts)
        │ drive (X steer, Y throttle)                  │ aim (X switch/aim, Y pitch)
UTankMovementComponent                        UTankTargetingComponent ──settings──> UTankSettingsSubsystem (SaveGame)
  UTankMovementMath (pure)                      UTargetSelectionMath / UTankSightMath (pure)
                                                UTargetRegistrySubsystem <──register── UTargetableComponent (on enemies)
UCockpitDisplayWidget (UMG on a Widget Component) reads targeting state ──> crosshair, markers, text
UAimAssistToggleWidget (inside the template's WBP_Menu) ──> UTankSettingsSubsystem
OverviewArm/OverviewCamera on ATankVehicle + UTankStatusHudWidget (screen) ──> overview view
```

| Responsibility | Where |
|---|---|
| Reading stick input | `UCockpitJoystickComponent` (VR hand), `ATankVehicle` (Enhanced Input fallback) |
| Drive math and movement | `UTankMovementMath`, `UTankMovementComponent` |
| Valid targets | `UTargetableComponent` + `UTargetRegistrySubsystem` (event based, weak refs, no per-tick actor search) |
| Scoring and cycling | `UTargetSelectionMath` + `UTankTargetingComponent` |
| Moving the crosshair | `UTankTargetingComponent` (smoothed aim) + `UTankSightMath` (projection) |
| Showing target info | `UCockpitDisplayWidget` |
| Setting and persistence | `UTankSettingsSubsystem` / `UTankSettingsSaveGame` |

Code is in `Source/TankGameVR/{Tank,Cockpit,Targeting,Settings,UI,Testing,Tests}`. Content is in
`Content/TankGame/{Blueprints,Input,Materials,Maps}`. `Tools/TankSetup/*.py` regenerates the content.

## Steering

- **One stick.** X steers and Y is the throttle. Steering alone drives the tank forward along a curve; the tank does
  not pivot on the spot (that is opt-in via `bAllowPivotTurn`).
- **Small vs. large deflection.** A small deflection gives a slow, wide curve: at 35 % the tank turns about 13° and
  drives about 3.2 m in 6 s. Full deflection gives a slow, tight curve: about 156° in 6 s.
- **Turn rate depends on speed**, like a tracked vehicle. Velocity always points along the hull, so there is no
  sideways sliding.
- **Neutral stick** decelerates to a stop. Hitting a wall stops the tank instead of sliding along it.
- **Ground following.** The tank keeps its ride height on the ground and does not pitch or roll (VR comfort).
- **Riders.** The rider is attached to the seat. The camera is never rotated by code, and head tracking stays the
  player's own.
- **Tuning.** Everything is on `TankMovement` in `BP_PlayerTank`: `SteeringDeadZone`, `SteeringSensitivity`,
  `SteeringCurve`, `JoystickSmoothing`, `InputResponseSpeed`, `MaximumForwardSpeed`, `ReverseSpeed`, `Acceleration`,
  `Deceleration`, `BrakingStrength`, `MaximumTurnRate`, `TurnSpeedMultiplier`, `TurnRateReferenceSpeed`,
  `MinimumMovementSpeedWhileTurning`, `SmallSteerSpeedRatio`, `MaximumMovementSpeedWhileTurning`,
  `MaximumTurnAcceleration` (comfort), `bAllowPivotTurn`, `bInvertSteeringInReverse`.
- **Units.** Speeds are in cm/s and turn rates in deg/s. The defaults (max 450 cm/s ≈ 16 km/h, 35°/s) are comfort
  starting points, not measured values. Tune them in the headset.

## Target lock (Aim Assist)

- **Locking.** There is one current target. Pushing the aim stick right or left selects the nearest eligible target
  on that side of the current one. There is no wrap-around (`bWrapAround`).
- **Eligibility.** A target is eligible when it is within `MaximumTargetDistance`, within `MaximumTargetAngle` of the
  hull's forward direction, and has line of sight (`MinimumTargetVisibility`: three rays, centre and sides).
- **Stable switching.**
  - A switch happens on the push edge.
  - The stick must come back below `TargetSwitchReleaseThreshold` before the next push counts (hysteresis).
  - Holding the stick repeats only after `TargetSwitchRepeatDelay`, then every `TargetSwitchRepeatInterval`.
  - `TargetSwitchCooldown` sets the minimum time between switches.
  - Ties between equal candidates are broken by a stable ID.
- **Lost targets.** A target that is destroyed, despawned or disabled (`SetTargetable(false)`) is dropped
  immediately and the best remaining target is locked. A target that leaves range, angle or line of sight is
  dropped only after `TargetSelectionSmoothing` seconds.
- **No targets.** The lock is cleared and the stick aims manually. When the stick is idle, the aim drifts back to the
  front after `AimRecenterDelay`. A new wave is locked automatically (`bAutoAcquire`).
- **Aim Assist off.** No locking and no hidden correction. The stick turns the aim (`ManualAimSpeed`,
  `ManualYawLimit`, `ManualPitchRange`). Switching it off continues from the current crosshair position, with no jump.
- **Crosshair.** The crosshair is the projection of the actual aim direction, so it glides onto the selected target
  at `CrosshairTransitionSpeed`. `TargetLockStrength` sets how fully the aim follows the target.
- **Enemies.** Any actor with a `UTargetableComponent` is a target, and its mesh does not matter. Set
  `TargetPointOffset` to the point the crosshair should sit on, and optionally a `DisplayName`.

## Cockpit display

- **Rendering.** It is a tactical sight drawn in UMG (`UCockpitDisplayWidget`) on a world Widget Component. It is
  not a camera feed, so there is no render-target cost in VR.
- **Projection.** The display shows a virtual sight that looks along the hull (`SightFrame`, `SightFieldOfView`
  70°).
- **Contents.**
  - The crosshair: a ring, red when locked, white when idle, cyan when aiming manually.
  - The boresight: a small fixed square.
  - Markers: red = selected, yellow = selectable, grey = not selectable. Off-screen targets are shown clamped to the
    edge.
  - Two text lines: `AIM ASSIST: ON/OFF`, and the target name with distance (or `NO TARGET` / `MANUAL AIM` /
    `(OFF SCREEN)`).
- **Cost.** The widgets are created once. Each tick only moves them and changes colours; nothing is rebuilt.
- **Custom styling.** Create a Widget Blueprint whose parent is `CockpitDisplayWidget`, name the widgets
  `Crosshair` (Border), `StatusText` and `TargetText`, and assign it to the tank's `Display` component.

## Aim Assist setting

- **Where it is.** The option is inside the existing VR menu (`WBP_Menu`), as an `AimAssistToggle` widget added
  before "Real life". It copies the menu's button style.
- **Storage.** `UTankSettingsSubsystem` saves it to the `TankPlayerSettings` save slot and broadcasts the change. The
  live targeting system reacts immediately.

## Replacing placeholders with final art

- Every visual in `BP_PlayerTank` and `BP_EnemyTank_Placeholder` is a separately named Static Mesh component, for
  example `HullLower`, `TurretBody`, `Barrel`, `Dashboard`, `DriveKnob`, `DisplayFrame`. Swap the mesh or materials,
  or delete the component and add your own. No code references these meshes.
- Keep the functional components:
  - `HullCollision` (resize to fit the new hull)
  - `TurretPivot`, `BarrelPivot` (the turret and barrel meshes go under these; they rotate with the aim)
  - `AimOrigin`, `SightFrame`
  - `CockpitRoot`, `Seat` (the seat floor is the rider's tracking origin; design eye height is 120 cm, set by
    `DesignEyeHeight`)
  - `DriveStick`, `AimStick` (stick pivots: +Z up the stick, +X forward)
  - `Display`
- The stick knob must stay a child of its stick component and keep a `BP_GrabComponent` with Grab Type **Custom**.
  The code keeps the knob grab-traceable and stops it from colliding.
- Materials are instances of `M_TankPlaceholder` (Color / Roughness / Metallic / EmissiveStrength). Replace them
  freely; nothing in code refers to them.
- Enemy tanks from teammates only need a `UTargetableComponent`.

## Debugging

- Console:
  - `Tank.Debug.Movement 1`: raw and processed input, speed, turn rate, heading, movement direction.
  - `Tank.Debug.Targeting 1`: candidates with yaw, pitch, distance, visibility and reject reason, the current lock,
    the switch decision and the aim ray.
  - Set either to `0` to force it off. Each component also has a `bDebugDraw` flag.
- Test commands:
  - `Tank.DestroyTarget`: destroys the locked target.
  - `Tank.NextWave`: clears the current wave and spawns the next one.

## Automated tests (21, all passing)

Run them from *Session Frontend → Automation*, filter `Tank`, or from the command line:
`UnrealEditor-Cmd.exe TankGameVR.uproject -ExecCmds="Automation RunTests Tank;Quit" -unattended -nullrhi`

| Area | What the tests check |
|---|---|
| Movement | Input shaping, small vs. large curves left and right, no sideways slip, no pivoting, throttle, reverse, neutral stop, ground height, 30/90/144 fps consistency, blocked by a wall |
| Targeting | Projection (centre, edges, behind, NaN), registry lifecycle, left/right cycling with no wrap, no jitter and controlled repeat, destroyed targets and new waves, range and angle limits, line of sight, aim recentring, Aim Assist on/off (live and persisted), crosshair following the target |
| Cockpit | Stick from a hand (no jump on grab, deflection, spring back, priority over the thumbstick) |
| Content | `IMC_Tank` overrides the template's teleport and turn keys; `BP_PlayerTank` setup; the real template `TryGrab` on a stick steers the tank; the real `BP_XRPawn` rides in the seat; waves keep getting locked; the menu toggle works; the overview camera sits behind and above the tank, follows it, shows the status panel and switches back |

## Manual VR checklist (needs a headset)

1. Start **VR Preview** in `LV_TankTest`.
   - You sit in the cockpit with both sticks and the display in front of you.
   - The display is readable, and the UI does not clip into the frame.
   - Looking around never turns the tank.
2. Grip the blue stick and tilt it slightly right: the tank drives a gentle right curve.
   - Tilt it fully right: a tight right curve.
   - Repeat to the left.
   - Push forward or pull back: the tank speeds up or reverses.
   - Release: it slows to a stop.
   - Check that turning feels heavy and comfortable, with no sudden jerks.
3. Without gripping, use the left thumbstick. It drives the tank and the cockpit stick moves with it. There is no
   teleport arc.
4. When the enemies arrive, the display locks the nearest one in front.
   - Flick the orange stick right and left: the crosshair glides to the next tank on that side.
   - Holding the stick steps slowly through targets, not every frame.
5. Wait for enemies to expire (or use `Tank.DestroyTarget`).
   - The lock moves to a remaining enemy.
   - When the wave is gone, the display shows `NO TARGET`.
   - The next wave is locked again.
6. Open the menu (B / Menu button), point at **Aim Assist: On** and click.
   - It changes to Off, the lock clears, and the orange stick now moves the crosshair freely.
   - Restart the game: the setting is still Off.
7. Press X (left controller): the view cuts to the overview camera above the tank; press again to return to the
   cockpit. Check that the cut is comfortable.
8. If the eye height feels wrong, use **Reset Orientation** in the menu. Seat calibration runs about 1 s after
   boarding.

## Implementation log

- **Project changes.**
  - Git was initialised: commit `6ef5ce2` is the untouched template.
  - The project was converted to Blueprint + C++ (module `TankGameVR`).
  - `XRBase` was listed explicitly in the `.uproject`.
- **New code.** `Source/TankGameVR/**`: 15 classes and 4 test files. The game build compiles without editor code.
- **New content.**
  - `Content/TankGame/**`: input, materials, `BP_PlayerTank`, enemy placeholders, the test spawner, `LV_TankTest`.
  - The engine sky sphere is used in the test map.
- **Changed template asset.** `WBP_Menu`: the Aim Assist option was added. Nothing else in the template was modified.
- **Not verified yet.** Real headset and controller feel (comfort values, stick reach, eye-height calibration) and
  hand-tracking use. See the checklist above.
