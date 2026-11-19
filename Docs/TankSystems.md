# VR Tank: steering, cockpit, lock-on aiming, loading and firing, enemies and the arena

My part of the game: joystick tank steering, the placeholder cockpit with two sticks and a display, Dark Souls-style
target cycling with a moving crosshair, the Aim Assist setting, an overview camera, a hand-loaded main gun with AP and
HE rounds, a cigarette, a fire extinguisher, a smoke grenade switch, three enemy types and the wave/arena flow.
UE 5.8.3, built on the VR Template (`BP_XRPawn` and its grab system are used as they are).

## Try it

- **Headset:** open `Content/TankGame/Maps/LV_TankTest` and press **VR Preview**. You board the tank automatically
  and the first wave comes after a few seconds. Survive 5 waves.
- **Without a headset:** press **Play** and click into the game window. WASD drives, arrow keys aim, **C** switches
  to the overview camera, **R** loads AP, **H** loads HE, **Space** fires, **V** fires smoke, **G** uses the
  extinguisher.
- **Spectating during VR:** press **Tab** in the game window for the template's spectator camera (WASD/QE + mouse).

## Controls

| Action | VR (in cockpit) | Controller | Keyboard |
|---|---|---|---|
| Drive: right/left = curve, forward/back = throttle/reverse | grip the **left (blue) stick** | left thumbstick | W A S D |
| Aim: flick right/left = next target on that side (free aim with Aim Assist off) | grip the **right (orange) stick** | right thumbstick | arrow keys |
| Aim Assist On/Off | VR menu (B / Menu) → **Aim Assist** | left stick click | T |
| Cockpit ↔ overview camera | X (Quest/PICO left), B (Index left) | gamepad Y | C |
| Load the gun | grip a **round** in the rack (left of the seat), push it nose-first into the **breech**. Top two = AP (black tip), bottom two = HE (olive, yellow band) | X = AP, D-pad left = HE (auto) | R = AP, H = HE (auto) |
| Fire | **trigger** (either hand) | RT | Space / F |
| Smoke screen | flip up the red cover on the dashboard (left of the screen), push the button under it | D-pad up | V |
| Put out a fire | grip the **extinguisher** on the right wall, point it at the fire, hold that hand's **trigger** | D-pad down (auto) | G (auto) |
| Smoke a cigarette | grip the **cigarette** on the ashtray (dashboard, right), bring it to your mouth, then away | | |

A held cockpit stick wins over the thumbstick; otherwise the thumbstick moves the stick. While seated, `IMC_Tank`
(priority 10) takes over the thumbsticks the template uses for teleport and snap turn.

The overview camera sits about 15 m behind and above the tank and shows a status panel on the monitor (speed, turn
rate, Aim Assist, target). In the headset the switch is an instant cut; on a monitor it blends (`ViewBlendTime`).

Each VR trigger has its own action (`IA_Tank_TriggerLeft/Right`): the hand holding the extinguisher sprays, the other
one fires. They do not consume the trigger, so the template menu laser and hand poses still work.

Code: `Source/TankGameVR/{Tank,Cockpit,Targeting,Weapons,Combat,Enemies,Game,Settings,UI,Testing,Tests}`. Content: `Content/TankGame/`.
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
- `UTankHealthComponent`: listens to the owner's standard damage, drops the lock-on at 0 HP and (optionally)
  removes the actor. `DamageMultipliers` is the armour: damage of a type is scaled (e.g. HE x0.5 on a tank). Any
  Blueprint enemy can instead use **Event AnyDamage** with its own HP; the cannon needs nothing else.

**AP and HE.** AP (`UTankDamageType_AP`) is one hard point hit (25). HE (`UTankDamageType_HE`) is a slower round with
a smaller direct hit (25) plus a blast (`HESplashDamage` 18 at the centre, falling off to 0 at `HESplashRadius`
4.5 m). The breech remembers which round went in (`GetLoadedRoundType`) and the display says `UP - AP READY` /
`UP - HE READY`. Pick the round for the target:

| | Tank | Armored car | AT gun |
|---|---|---|---|
| AP | x1.0 (2 hits) | x0.6 (2 hits, goes through) | x0.5 (3 hits, through the shield) |
| HE | x0.5 (4 hits) | x1.5 (1 hit) | x1.5 (1 hit) |

## Enemies

Three types, matching Ugurkan's `E_EnemyTypes` and `DT_EnemyFeatures`. Shared C++ base `ATankEnemy`
(`Source/TankGameVR/Enemies`), behaviour in `ATankEnemy_Tank`, `ATankEnemy_ArmoredCar`, `ATankEnemy_AntiTank`, meshes
in `BP_Enemy_Tank`, `BP_Enemy_ArmoredCar`, `BP_Enemy_AntiTank`.

- **Stats from the table.** At BeginPlay each enemy reads its row (`Tank_Row`, `ArmoredCar_Row`, `AntiTank_Row`, or
  `StatsRow`): `E_HP`, `E_Damage` (per shot; the car splits it over a 4-round burst), `E_Speed` in **km/h**, `E_Range`
  in **metres**. The range is where it likes to fight; it opens fire from `FireRangeScale` times further. If
  `E_Model` is a real mesh (not an engine basic shape) it replaces the placeholder meshes. The table is only read,
  never changed.
- **Driving** (`UEnemyDriveComponent`): turns at a limited rate and never strafes, slows for sharp turns, tracked
  vehicles pivot, wheeled ones need speed to turn. Follows the ground (climbs up to `MaxStepHeight` 50 cm, leans at
  most 15 degrees), keeps apart from other enemies, steers around what its feelers touch and backs out when stuck.
  Uses the nav mesh when the level has a `NavMeshBoundsVolume` (built at runtime for a 1.8 m wide vehicle, see
  `DefaultEngine.ini`), otherwise drives straight.
- **Gun** (`UEnemyWeaponComponent`): turret and gun turn at a limited speed, it needs a clear line of sight (smoke
  counts) and to stay on target for `AimTime`, then fires. Accuracy gets better while it tracks, worse while it
  drives. Rounds lead a moving target a bit and fly through other enemies.
- **Tank:** comes in from a flank, firing on the move (inaccurate), then stops at its range for aimed shots and moves
  to a new spot after two shots or when hit hard. Slow, heavy rounds.
- **Armored car:** fast and never still for long: short hops around the player at 15-20 m with autocannon bursts,
  and every other hop a 2.5 s stop to fire. That stop is the moment to hit it.
- **AT gun:** moves to a spot where it can see you, digs in (2 s), then lays the gun for 2.6 s with a blinking red
  laser before one 40-damage shot. The display warns **AT GUN AIMING - MOVE OR SMOKE!**. Breaking the line of sight
  (cover or smoke) makes it start the lay again. It moves after two shots or when hit.
- **Wrecks** burn for `WreckTime` (10 s) with flames and smoke (`WreckFire*` components), keep blocking shots and
  can't be locked.

## Arena and waves

`ATankArenaDirector` (`BP_TankArenaDirector`, put one in the level) runs the match:

- Waves from `Waves` (default: 2 tanks / 1 tank + 2 cars / tank + car + AT gun / 2+2+1 / 2+2+2). Enemies arrive one
  by one (`SpawnInterval`) at `ATankEnemySpawnPoint` actors at least `MinDistanceFromPlayer` away (each spawn point
  can be limited to some types). Without spawn points they come from a ring around the player.
- Score per kill (`KillScore`: tank 100, car 60, AT gun 150). After a cleared wave the crew repairs
  `RepairBetweenWaves` (35) HP.
- Player tank at 0 HP: engine and gun dead, **TANK DESTROYED**, restart after `RestartDelay` (the rider stays in the
  seat). All waves cleared: **AREA CLEAR**, then a new match.
- Ugurkan's `BP_EnemySpawner` can call `SpawnEnemy(Type, Transform)` on the director instead, if it should decide
  the spawning.

`LV_TankTest` is a 120 x 120 m test arena (walls, ruins, rocks, 10 spawn points, nav mesh). The real arena only needs
the same three things: a `NavMeshBoundsVolume` over the playable area, spawn points and the director.

## Getting hit, fires and the extinguisher

- The player tank has 100 HP (`Health`). `UTankDamageReactionComponent` makes hits felt: a kick in both hands, the
  cockpit light flickers, the display flashes red and says where it came from (**HIT - LEFT** ...). No camera
  shake.
- Heavy hits start a fire (45%, autocannon rounds 6%) at one of `FireSpotA/B/C` (under the dashboard, in front of
  the ammo rack, the cable loom on the right wall). A fire grows, burns HP and spreads to the next spot if left
  alone. The display shows **FIRE! USE THE EXTINGUISHER**.
- `UCockpitExtinguisherComponent` (`Extinguisher`, red bottle on the right wall): grip it, point the nozzle, hold that
  hand's trigger. The powder puts out fires inside its cone; the bottle has limited charge that slowly comes back.
  G sprays at the biggest fire without hands.

## Smoke grenades

`UCockpitSmokeLauncherComponent` (`SmokeLauncher`, dashboard left of the screen): a guarded switch. Grip the red cover
(`SmokeCoverHinge`) and flip it up (it latches past 55 degrees), then push the button (`SmokeButton`) with a
fingertip. The turret launchers throw a smoke wall (`ATankSmokeCloud`) about 22 m out **in the direction the gun
points**, so turn the turret towards the threat first. For about 10 s enemy gunners can't see through it. Two salvos,
25 s to reload one; the lamp is green when ready and blinks while reloading. V does the same.

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
selectable, clamped to the edge when off screen) and text: Aim Assist, hull %, gun state, rounds left and smoke,
target type and distance, wave/enemies/score, and one warning line in the middle (AT gun, fire, hits, wave
messages). A red frame flashes on hits.
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
- New cockpit parts keep their functional names: `Extinguisher` (bottle mesh with `ExtinguisherGrab` under it, nozzle
  at `NozzleOffset`), `SmokeLauncher` with `SmokeCoverHinge` (cover mesh + `SmokeCoverGrab`), `SmokeButton`,
  `SmokeLamp`, `FireSpotA/B/C`, `ShellD`. The flames, smoke and spray are made at runtime from basic shapes and the
  `MI_Fire_*` / `MI_Smoke_Screen` / `MI_Extinguisher_Spray` materials; swap the materials or replace them with
  particles later.
- Enemy Blueprints: put the art under `HullCollision` (hull), `TurretPivot` (turret, turns) and `BarrelPivot` (gun,
  elevates). Keep `Muzzle` at the barrel tip, `MuzzleFlash`, `AimLaser` (AT gun) and the `WreckFire*` parts. Or set a
  real mesh in `E_Model` of the table and it is used instead of the placeholders.
- Sound slots are there but empty (`FireSound` on the enemy guns, `LaunchSound`, `SpraySound`...); they get filled
  last.

## Debugging

- `Tank.Debug.Movement 1` / `Tank.Debug.Targeting 1`: on-screen and in-world debug info (0 forces it off).
- `Tank.DestroyTarget` destroys the locked target, `Tank.NextWave` spawns the next test-spawner wave.
- `Tank.SkipWave` clears the arena wave and starts the next one, `Tank.Restart` restarts the match.

## Tests

*Session Frontend → Automation*, filter `Tank`, or:
`UnrealEditor-Cmd.exe TankGameVR.uproject -ExecCmds="Automation RunTests Tank;Quit" -unattended -nullrhi`

They cover steering behaviour and frame-rate independence, target selection and cycling, the crosshair projection,
the Aim Assist setting, grabbing the cockpit sticks, boarding, input mappings, the overview camera, loading by hand
(alignment, ramming, pulling out, letting go), auto load, firing and refill, damage and death, the real enemy
Blueprint killed by two shots, and the cigarette. `Tank.Combat.*` covers reading Ugurkan's table, armour and AP/HE,
the HE blast and no friendly fire, each enemy type's behaviour (tank closes in and fires, car circles and bursts, AT
gun lays then fires and smoke breaks it), fires and the extinguisher (the spraying hand does not fire the gun), the
smoke switch, and waves / game over / restart.

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
12. HE round (bottom of the rack) into the breech: display says **UP - HE READY**; one HE hit wrecks an armored car.
13. Smoke switch: grip the red cover, flip it up, it stays; push the button: two grenades fly out where the gun
    points and a smoke wall grows. An AT gun aiming at you loses its lay (the warning goes away).
14. Get hit until something burns: grip the extinguisher, point it, hold the trigger of that hand: powder, the
    fire shrinks and goes out, the gun does not fire. Release: the bottle goes back to the wall.
15. Barrel over your head: turn the turret all the way round; the gun passes over the open cockpit, not through it.

Not tested yet on a real headset: comfort values, stick/rack/breech reach, eye-height calibration, mouth position
(`MouthOffset`).
