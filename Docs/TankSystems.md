# VR Tank: steering, cockpit, lock-on aiming, Aim Assist

My part of the game: joystick tank steering, the placeholder cockpit with two sticks and a display, Dark Souls-style
target cycling with a moving crosshair, the Aim Assist setting and an overview camera. UE 5.8.3, built on the VR
Template (`BP_XRPawn` and its grab system are used as they are).

## Try it

- **Headset:** open `Content/TankGame/Maps/LV_TankTest` and press **VR Preview**. You board the tank automatically.
- **Without a headset:** press **Play** and click into the game window. WASD drives, arrow keys aim, **C** switches
  to the overview camera.
- **Spectating during VR:** press **Tab** in the game window for the template's spectator camera (WASD/QE + mouse).

## Controls

| Action | VR (in cockpit) | Controller | Keyboard |
|---|---|---|---|
| Drive: right/left = curve, forward/back = throttle/reverse | grip the **left (blue) stick** | left thumbstick | W A S D |
| Aim: flick right/left = next target on that side (free aim with Aim Assist off) | grip the **right (orange) stick** | right thumbstick | arrow keys |
| Aim Assist On/Off | VR menu (B / Menu) → **Aim Assist** | | |
| Cockpit ↔ overview camera | X (Quest/PICO left), B (Index left) | gamepad Y | C |

A held cockpit stick wins over the thumbstick; otherwise the thumbstick moves the stick. While seated, `IMC_Tank`
(priority 10) takes over the thumbsticks the template uses for teleport and snap turn.

The overview camera sits about 15 m behind and above the tank and shows a status panel on the monitor (speed, turn
rate, Aim Assist, target). In the headset the switch is an instant cut; on a monitor it blends (`ViewBlendTime`).

Code: `Source/TankGameVR/{Tank,Cockpit,Targeting,Settings,UI,Testing,Tests}`. Content: `Content/TankGame/`.
`Tools/TankSetup/*.py` regenerates the content.

## Steering

- One stick: X steers, Y is throttle. Steering alone drives forward along a curve; no pivot turns unless
  `bAllowPivotTurn` is set.
- A small deflection gives a wide curve, full deflection a tight one. Turn rate depends on speed like a tracked
  vehicle, and the tank never slides sideways.
- Neutral stick slows to a stop. Walls stop the tank. It follows the ground without pitching or rolling (comfort).
- The rider is attached to the seat; the camera is never rotated by code.
- All tuning values are on `TankMovement` in `BP_PlayerTank` (dead zone, sensitivity, curve, speeds, acceleration,
  braking, turn rate, comfort limits). Speeds are cm/s, turn rates deg/s. Defaults are starting points for headset
  tuning.

## Target lock (Aim Assist)

- One current target. Pushing the aim stick right/left picks the nearest eligible target on that side, no wrap-around.
- Eligible = within `MaximumTargetDistance`, within `MaximumTargetAngle` of the hull's forward, and visible
  (`MinimumTargetVisibility`, three line-of-sight rays).
- No jitter: switches happen on the push edge, need the stick to return first, and repeat slowly when held
  (`TargetSwitchRepeatDelay`/`Interval`, `TargetSwitchCooldown`).
- Destroyed or disabled targets are dropped at once; targets that leave view are dropped after
  `TargetSelectionSmoothing`. New waves are locked automatically. With no targets the aim drifts back to the front.
- With Aim Assist off there is no locking: the stick aims freely, starting from where the crosshair was.
- The crosshair is the projection of the real aim direction, so it glides onto the target
  (`CrosshairTransitionSpeed`).
- Any actor with a `UTargetableComponent` is a target. Use `TargetPointOffset` for where the crosshair sits and
  `DisplayName` for the label.

## Cockpit display

A UMG sight (`UCockpitDisplayWidget`) on a world Widget Component, not a camera feed. It shows the crosshair (red
locked, white idle, cyan manual), a fixed boresight, target markers (red selected, yellow selectable, grey not
selectable, clamped to the edge when off screen) and two text lines (Aim Assist state, target name and distance).
Widgets are created once and only moved each tick.

To restyle it, make a Widget Blueprint with parent `CockpitDisplayWidget`, name the widgets `Crosshair` (Border),
`StatusText` and `TargetText`, and assign it to the tank's `Display` component.

## Aim Assist setting

An `AimAssistToggle` button in the VR menu (`WBP_Menu`), styled like the other buttons. `UTankSettingsSubsystem`
saves it (slot `TankPlayerSettings`) and the targeting reacts immediately.

## Replacing placeholders with final art

- Every visual in `BP_PlayerTank` and `BP_EnemyTank_Placeholder` is a named Static Mesh component (`HullLower`,
  `TurretBody`, `Barrel`, `Dashboard`, ...). Swap or delete them freely; no code references them.
- Keep the functional components: `HullCollision` (resize it), `TurretPivot`/`BarrelPivot` (turret and barrel meshes
  go under these), `AimOrigin`, `SightFrame`, `CockpitRoot`, `Seat` (eye height 120 cm, `DesignEyeHeight`),
  `DriveStick`, `AimStick`, `Display`, `OverviewArm`/`OverviewCamera`.
- Stick knobs stay children of their stick and keep a `BP_GrabComponent` with Grab Type **Custom**.
- Materials are instances of `M_TankPlaceholder` and can be replaced.
- Enemy tanks only need a `UTargetableComponent`.

## Debugging

- `Tank.Debug.Movement 1` / `Tank.Debug.Targeting 1`: on-screen and in-world debug info (0 forces it off).
- `Tank.DestroyTarget` destroys the locked target, `Tank.NextWave` spawns the next wave.

## Tests

*Session Frontend → Automation*, filter `Tank`, or:
`UnrealEditor-Cmd.exe TankGameVR.uproject -ExecCmds="Automation RunTests Tank;Quit" -unattended -nullrhi`

They cover steering behaviour and frame-rate independence, target selection and cycling, the crosshair projection,
the Aim Assist setting, grabbing the cockpit sticks, boarding, input mappings and the overview camera.

## VR checklist (headset)

1. VR Preview in `LV_TankTest`: sticks and display in front of you, display readable, looking around never turns the
   tank.
2. Blue stick: slight tilt = gentle curve, full tilt = tight curve, forward/back = throttle/reverse, release = stop.
   Turning should feel heavy and comfortable.
3. Left thumbstick without gripping: drives the tank, no teleport arc.
4. Enemies arrive and get locked; flicking the orange stick moves the crosshair to the next tank on that side.
5. Enemies expire (or `Tank.DestroyTarget`): the lock moves on; the next wave is locked again.
6. Menu → Aim Assist Off: lock clears, the orange stick aims freely; the setting survives a restart.
7. X button: overview camera and back. The cut should feel fine.
8. Eye height wrong? Use **Reset Orientation** in the menu (calibration runs about 1 s after boarding).

Not tested yet on a real headset: comfort values, stick reach, eye-height calibration.
