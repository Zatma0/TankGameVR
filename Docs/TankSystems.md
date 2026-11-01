# VR Tank: steering, cockpit, lock-on aiming, loading and firing

My part of the game: joystick tank steering, the placeholder cockpit with two sticks and a display, Dark Souls-style
target cycling with a moving crosshair, the Aim Assist setting, an overview camera, a hand-loaded main gun and a
cigarette. UE 5.8.3, built on the VR
Template (`BP_XRPawn` and its grab system are used as they are).

## Try it

- **Headset:** open `Content/TankGame/Maps/LV_TankTest` and press **VR Preview**. You board the tank automatically.
- **Without a headset:** press **Play** and click into the game window. WASD drives, arrow keys aim, **C** switches
  to the overview camera, **R** loads a round, **Space** fires.
- **Spectating during VR:** press **Tab** in the game window for the template's spectator camera (WASD/QE + mouse).

## Controls

| Action | VR (in cockpit) | Controller | Keyboard |
|---|---|---|---|
| Drive: right/left = curve, forward/back = throttle/reverse | grip the **left (blue) stick** | left thumbstick | W A S D |
| Aim: flick right/left = next target on that side (free aim with Aim Assist off) | grip the **right (orange) stick** | right thumbstick | arrow keys |
| Aim Assist On/Off | VR menu (B / Menu) → **Aim Assist** | | |
| Cockpit ↔ overview camera | X (Quest/PICO left), B (Index left) | gamepad Y | C |
| Load the gun | grip a **round** in the rack (left of the seat), push it nose-first into the **breech** | gamepad X (auto) | R (auto) |
| Fire | **trigger** (either hand) | RT | Space / F |
| Smoke | grip the **cigarette** on the ashtray (dashboard, right), bring it to your mouth, then away | | |

A held cockpit stick wins over the thumbstick; otherwise the thumbstick moves the stick. While seated, `IMC_Tank`
(priority 10) takes over the thumbsticks the template uses for teleport and snap turn.

The overview camera sits about 15 m behind and above the tank and shows a status panel on the monitor (speed, turn
rate, Aim Assist, target). In the headset the switch is an instant cut; on a monitor it blends (`ViewBlendTime`).

The fire action does not consume the trigger, so the template menu laser and hand poses still work.

Code: `Source/TankGameVR/{Tank,Cockpit,Targeting,Weapons,Combat,Settings,UI,Testing,Tests}`. Content: `Content/TankGame/`.
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
- Eligible = within the lock range `MaximumTargetDistance` (60 m), visible (`MinimumTargetVisibility`, three
  line-of-sight rays) and within `MaximumTargetAngle` of the hull's forward (180 = all around, since the turret
  turns 360). Enemies beside or behind the tank can be locked; the turret swings round at its own speed and the
  cockpit display shows them at its edge.
- Nothing lockable (e.g. enemies farther than 60 m): the right stick turns the turret by hand, just like with Aim
  Assist off. While you aim by hand nothing gets locked; once the stick has been still for
  `AutoAcquireAfterManualDelay` the nearest eligible target is locked.
- No jitter: switches happen on the push edge, need the stick to return first, and repeat slowly when held
  (`TargetSwitchRepeatDelay`/`Interval`, `TargetSwitchCooldown`).
- Destroyed or disabled targets are dropped at once; targets that leave view are dropped after
  `TargetSelectionSmoothing`. New waves are locked automatically. With no targets the aim drifts back to the front.
- With Aim Assist off there is no locking: the stick aims freely, starting from where the crosshair was.
- The turret is heavy, like a real one: it turns at `TurretTraverseSpeed` (30 deg/s; real tanks are about 25-40)
  and spins up/brakes with `TurretTraverseAcceleration`, without overshooting. The gun elevates at
  `GunElevationSpeed` (8 deg/s). A new lock therefore takes a moment to swing onto: the display shows
  **LAYING GUN...** and a yellow crosshair until the gun is within `OnTargetTolerance`, then red.
- The crosshair shows where the gun really points, and the shot goes there too, so firing while the turret is still
  swinging misses. The manual stick can't run more than a few degrees ahead of the turret.
- The turret angle is kept relative to the hull, so when the hull turns the turret has to catch up at its own speed.
- Any actor with a `UTargetableComponent` is a target. Use `TargetPointOffset` for where the crosshair sits and
  `DisplayName` for the label.

## Loading and firing

Modelled on a real tank loader, simplified: take a round from the ready rack, ram it into the breech, the sliding
wedge closes by itself and the loader calls **"UP"**; the gunner fires; the gun recoils, the breech opens and throws
out the case stub (modern rounds have a combustible case, only the stub is left).

- `UTankShellComponent` (`ShellA/B/C`, rack left of the seat): a grabbable round. Pivot in the middle, +X = nose
  (`NoseOffset`). Released anywhere, it slides back to its slot. After firing, the slot refills after `RefillDelay`.
- `UTankBreechComponent` (`Breech`): placed at the breech mouth, +X into the gun.
  - A held round is accepted only if its nose is within `CaptureRadius` of the mouth and it is within `CaptureAngle`
    of the bore. Then it can only slide along the bore.
  - Pushed past `RamThreshold` x `RamDepth`, the round slides home, the wedge (`BreechBlock`) closes, the
    `ReadyLamp` lights and the display shows **UP - READY TO FIRE**.
  - Let go half way and the round goes back to the rack; pull it back out past `ExitMargin` and it is free again.
  - After a shot: recoil (`RecoilDistance`), the breech opens and the `CasingStub` flies onto the floor.
  - `AutoLoad()` (R / gamepad X) does the same without hands, for monitor play.
- `UTankCannonComponent` (`Cannon`): `Fire()` works only when loaded (otherwise a click). It spawns
  `ATankShellProjectile` from the barrel tip along the aim direction, with sound, muzzle flash (`MuzzleFlash`,
  `MuzzleFlashLight`) and a haptic kick in both hands. No camera shake (VR comfort).
- `ATankShellProjectile`: fast tracer (`MuzzleSpeed` 150 m/s, light gravity). On a hit it calls
  `UGameplayStatics::ApplyPointDamage` (`Damage` 25) and shows a short flash. It ignores our own tank and its rider.
- `UTankHealthComponent` (on `BP_EnemyTank_Placeholder`, 50 HP = two hits): listens to the owner's standard damage,
  drops the lock-on at 0 HP and removes the actor. Any Blueprint enemy can instead use **Event AnyDamage** with its
  own HP (e.g. from `DT_EnemyFeatures`); the cannon needs nothing else. The enemy hull and turret block shells.

## Cigarette

Just for feel, usable any time. `UCockpitCigaretteComponent` (`Cigarette`, on the ashtray): bring the filter end
within `DragDistance` of your mouth (`MouthOffset` from the camera) and the ember (`CigaretteEmber`, `CigaretteGlow`)
heats up. Take it away after at least `MinDragTime` and a few smoke puffs (`MI_Cigarette_Smoke`) drift out in front
of your face. Released, it goes back to the ashtray. `CrackleSound` is empty; set one if you have a sound.

Both items use `UCockpitGrabbableComponent`: a `BP_GrabComponent` (Grab Type **Custom**) under the item's mesh. The
item follows the hand itself and returns home in cockpit space (no physics inside a moving tank).

## Cockpit display

A UMG sight (`UCockpitDisplayWidget`) on a world Widget Component, not a camera feed. It shows the crosshair (red
locked, white idle, cyan manual), a fixed boresight, target markers (red selected, yellow selectable, grey not
selectable, clamped to the edge when off screen) and three text lines (Aim Assist state, target name and distance,
gun state).
Widgets are created once and only moved each tick.

To restyle it, make a Widget Blueprint with parent `CockpitDisplayWidget`, name the widgets `Crosshair` (Border),
`StatusText`, `TargetText` and `GunText`, and assign it to the tank's `Display` component.

## Aim Assist setting

An `AimAssistToggle` button in the VR menu (`WBP_Menu`), styled like the other buttons. `UTankSettingsSubsystem`
saves it (slot `TankPlayerSettings`) and the targeting reacts immediately.

## Replacing placeholders with final art

- Every visual in `BP_PlayerTank` and `BP_EnemyTank_Placeholder` is a named Static Mesh component (`HullLower`,
  `TurretBody`, `Barrel`, `Dashboard`, ...). Swap or delete them freely; no code references them.
- Keep the functional components: `HullCollision` (resize it), `TurretPivot`/`BarrelPivot` (turret and barrel meshes
  go under these), `AimOrigin`, `SightFrame`, `CockpitRoot`, `Seat` (eye height 120 cm, `DesignEyeHeight`),
  `DriveStick`, `AimStick`, `Display`, `OverviewArm`/`OverviewCamera`, `Cannon`, `Breech`, `ShellA/B/C`,
  `Cigarette`.
- Named visuals the code drives: `BreechBlock`, `ReadyLamp`, `CasingStub` (under `Breech`), `MuzzleFlash`,
  `MuzzleFlashLight` (under `BarrelPivot`), `CigaretteEmber`, `CigaretteGlow` (under `Cigarette`). Keep the names
  when swapping meshes; the other rack, breech and ashtray meshes are free.
- Stick knobs, round cases and the cigarette paper keep a `BP_GrabComponent` with Grab Type **Custom** under them.
- Materials are instances of `M_TankPlaceholder` and can be replaced.
- Enemy tanks need a `UTargetableComponent` to be locked, collision to be hit, and either `UTankHealthComponent`
  or their own Event AnyDamage.

## Debugging

- `Tank.Debug.Movement 1` / `Tank.Debug.Targeting 1`: on-screen and in-world debug info (0 forces it off).
- `Tank.DestroyTarget` destroys the locked target, `Tank.NextWave` spawns the next wave.

## Tests

*Session Frontend → Automation*, filter `Tank`, or:
`UnrealEditor-Cmd.exe TankGameVR.uproject -ExecCmds="Automation RunTests Tank;Quit" -unattended -nullrhi`

They cover steering behaviour and frame-rate independence, target selection and cycling, the crosshair projection,
the Aim Assist setting, grabbing the cockpit sticks, boarding, input mappings, the overview camera, loading by hand
(alignment, ramming, pulling out, letting go), auto load, firing and refill, damage and death, the real enemy
Blueprint killed by two shots, and the cigarette.

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
9. Rack and breech within reach on the left? Grip a round, put it nose-first into the breech, push: clunk, green
   lamp, display says UP. Sideways it must not go in.
10. Trigger: shot, flash, kick in both hands, breech recoils and opens, stub drops on the floor; the enemy takes two
    hits. Trigger while empty: just a click. The menu laser still clicks with the trigger.
11. Cigarette to the mouth: ember glows; take it away: smoke in front of you; release: back to the ashtray.

Not tested yet on a real headset: comfort values, stick/rack/breech reach, eye-height calibration, mouth position
(`MouthOffset`).
