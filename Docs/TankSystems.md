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
  extinguisher, **Q** listens to the radio.
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
| Put out a fire | grip the **extinguisher** on the right wall, point it at the fire, hold that hand's **trigger** | D-pad down (auto) | G (auto: one press sprays 2.5 s, holding keeps spraying) |
| Smoke a cigarette | grip the **cigarette** on the ashtray (dashboard, right), bring it to your mouth, then away | | |
| Radio report | lift the **handset** off its hook on the right wall and hold it to your ear | D-pad right | Q |

A held cockpit stick wins over the thumbstick; otherwise the thumbstick moves the stick. While seated, `IMC_Tank`
(priority 10) takes over the thumbsticks the template uses for teleport and snap turn.

The overview camera sits about 15 m behind and above the tank and shows a status panel on the monitor (speed, turn
rate, Aim Assist, target). In the headset the switch is an instant cut; on a monitor it blends (`ViewBlendTime`).

Each VR trigger has its own action (`IA_Tank_TriggerLeft/Right`): the hand holding the extinguisher sprays, the other
one fires. They do not consume the trigger, so the template menu laser and hand poses still work.

Code: `Source/TankGameVR/{Tank,Cockpit,Targeting,Weapons,Combat,Enemies,Game,Settings,UI,Testing,Tests}`. Content: `Content/TankGame/`.
`Tools/TankSetup/*.py` regenerates the content (`06_radio_and_cables.py` adds the radio and the cockpit cables, `07_tank_proportions.py` the cockpit armour and the
long guns).
The project uses the engine's **Cable Component** plugin (enabled in the `.uproject`).

## Steering

- One stick: X steers, Y is throttle. Steering alone drives forward along a curve; no pivot turns unless
  `bAllowPivotTurn` is set.
- A small deflection gives a wide curve, full deflection a tight one. Turn rate depends on speed like a tracked
  vehicle, and the tank never slides sideways.
- Neutral stick slows to a stop. Walls and buildings stop the tank; it never passes through level geometry.
- **Hills:** the tank drives on whatever is below it (landscape or meshes). It measures the ground at the four track
  ends, leans with it (`TiltSpeed`) and goes slower uphill (`SteepClimbSpeedScale`). Ground steeper than
  `MaxClimbAngle` (32 degrees) is a wall when going up; going down or along it is always allowed, so it cannot get
  trapped on a slope. A step or drop of more than `MaxGroundStep` (60 cm) stops it as well. Only static level
  geometry counts as ground, never vehicles or wrecks.
- If leaning turns out uncomfortable in the headset, switch `bTiltWithGround` off: the hull stays level and still
  climbs.
- However high the tank is placed in a level, it sits down on the ground at start (`RideHeight` 90 cm).
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

**AP and HE.** Two rounds that look and act differently:

- **AP** (`UTankDamageType_AP`): a fast, thin, pale tracer and a small sharp flash. One hard point hit (25) that
  goes through armour. Nothing else happens where it lands.
- **HE** (`UTankDamageType_HE`): a slower, fat red tracer, a fireball, a blast (`HESplashDamage` 18 at the centre,
  falling off to 0 at `HESplashRadius` 4.5 m) and **burning ground** where it lands.
- **Burning ground** (`ATankGroundFire`, `HEGroundFireClass` on the cannon): a 3 m patch that burns for 10 s. Every
  tank in it loses `DamagePerSecond` (4) HP per second, **our own tank too**; the display then blinks **BURNING
  GROUND - DRIVE OUT!**. Tank armour halves it, an AT gun crew takes one and a half times as much. Use it to hurt
  what stands still or to block a path. A second HE into the same spot keeps the fire going.
- The display tells the player what the rounds do: with a target locked the gun line adds **GOOD VS TANK** (green)
  or **WEAK VS TANK** (red) for the loaded round, and after each shot it reports the result for a moment, e.g.
  `AP HIT TANK  -25`, `HE HIT TANK  -13  WEAK - WRONG ROUND`, `HE BURST - GROUND ON FIRE`, `AP MISSED`.

The breech remembers which round went in (`GetLoadedRoundType`). Pick the round for the target:

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
  Ground steeper than `MaxClimbAngle` (35 degrees, same as the nav mesh) is a wall for them too.
  Uses the nav mesh when the level has a `NavMeshBoundsVolume` (built at runtime for a 1.8 m wide vehicle, see
  `DefaultEngine.ini`). Without one it drives straight at the goal and only goes round a single building in the
  way, which is not enough between several buildings.
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

- Waves from `Waves` (default: 2 tanks / 1 tank + 2 cars / tank + car + AT gun / 2+2+1 / 2+2+2). A wave arrives in
  ones and twos: `SpawnGroupSize` (1 to 2) enemies come in `SpawnInterval` apart, then nothing for
  `TimeBetweenGroups` (6 s), then the next one or two. They appear at `ATankEnemySpawnPoint` actors at least
  `MinDistanceFromPlayer` away (each spawn point can be limited to some types).
- **Out of sight** (`bSpawnOutOfSight`): enemies only come in where the player is not looking, i.e. outside the
  player's vision cone or behind a building, a hill or smoke. Spawn points in view are skipped while there are
  others; only if every possible spot is in view is one of them used anyway. Without spawn points they come from a ring around the player, on level ground
  only (no hillsides, roofs or pits).
- A tank or car that cannot get to the player (stuck out of sight for `LostEnemyTimeout`, 20 s, or out of sight
  for three times that) is brought back in at another spot, so a wave always ends.
- Score per kill (`KillScore`: tank 100, car 60, AT gun 150). After a cleared wave the crew repairs
  `RepairBetweenWaves` (35) HP.
- Player tank at 0 HP: engine and gun dead, **TANK DESTROYED**, restart after `RestartDelay` (the rider stays in the
  seat). All waves cleared: **AREA CLEAR**, then a new match.
- Ugurkan's `BP_EnemySpawner` can call `SpawnEnemy(Type, Transform)` on the director instead, if it should decide
  the spawning.

`LV_TankTest` is a 120 x 120 m test arena (walls, ruins, rocks, 10 spawn points, nav mesh).

### Using it in another level (e.g. `UgurkanLevel`)

Place `BP_PlayerTank` (any height, it lands on the ground) and press Play. That is all a level needs:
`UTankLevelSetupSubsystem` fills in the rest while the game runs, without changing the level, and leaves alone
whatever the level already has.

| Missing in the level | What happens at run time | To take control yourself |
|---|---|---|
| Arena director | `BP_TankArenaDirector` is added and runs its waves | place one and set it up |
| `NavMeshBoundsVolume` | navigation is built in a box around the player (`AutoNavigationSize`, 400 x 400 m), so enemies path around buildings | place a volume over the playable area |
| Simple collision on a big mesh | a blocking box around its bounds (logged with the mesh name) | add collision in the Static Mesh editor |
| Spawn points | enemies come from a ring around the player, on level ground | place `TankEnemySpawnPoint` actors |

The switches are on the director (`bCreateNavigationIfMissing`, `bBlockMeshesWithoutCollision`), and
`Tank.LevelSetup 0` turns the whole thing off. Convex hulls count as simple collision; only meshes with none at all
get a box.

**A spawner of the level's own** (Ugurkan's `BP_EnemySpawner` with `BP_EnemyBase`). His Blueprints are not changed.
Every `BP_EnemyBase` his spawner creates is swapped for the real enemy of that type (`SpawnedEnemyType`: tank,
armored car, AT gun): his actor stays as a hidden stand-in attached to ours, and when ours is destroyed his
`OnEnemyDied` dispatcher fires and the stand-in is removed. If his spawner removes its enemy, ours goes too.

- Until his spawner has really produced an enemy the director runs its own waves. With his first enemy it hands the
  waves over (`SetExternalWaves`): from then on his spawner decides when and what, the director only spawns the
  vehicle, keeps score and handles game over and restart. The display shows his `WaveCount`.
- The enemy appears where his spawner put it if that is free, level ground away from the player and out of the
  player's sight; otherwise at one of the usual spawn spots (also out of sight).
- Any other Blueprint spawner can use the same hook: `SetPlaceholderEnemyClass(Class, TypeVariable, DiedEvent)`, or
  simply call `SpawnEnemy(Type, Transform)` on the director.

## Vision cone

`UTankVisionConeComponent` (`VisionCone` on the player tank) is what the player can see right now: a cone from the
player's eyes (the headset in VR, the cockpit or overview camera on a monitor) along the view direction, `HalfAngle`
65 degrees to each side, `Range` 200 m. Only the horizontal direction counts, so looking down at the controls does
not blind the player. With `bCheckLineOfSight` a point behind a building, a hill or smoke is not seen either.

From Blueprint (e.g. a spawner):

- **Get Player Vision Cone** gives the component from anywhere.
- **Can See (Point)** / **Can See Actor**: inside the cone and nothing in the way. **Is In Cone** checks angle and
  range only.
- On the director: **Get Hidden Spawn Transform (Type)** or **... By Index (0 tank, 1 car, 2 AT gun)** returns a
  ready spawn transform the player is not looking at, on level, free ground (a spawn point if the level has any).
  **Player Can See (Point)** is the short form of the check.

`Tank.Debug.VisionCone 1` draws the cone on the ground. It starts at the player's own eyes, so the player never sees
its edges; watch it from the spectator camera (Tab) or the editor.

## Getting hit, fires and the extinguisher

- The hull repairs itself slowly: after `RepairDelay` (6 s) without any damage, `RepairPerSecond` (2) HP come back
  per second up to full. Any hit, a cockpit fire or burning ground stops it. The display shows **REPAIRING** next to
  the hull value. Both values are on `Health` in `BP_PlayerTank`.
- The player tank has 100 HP (`Health`). `UTankDamageReactionComponent` makes hits felt: a kick in both hands, the
  cockpit light flickers, the display flashes red and says where it came from (**HIT - LEFT** ...). No camera
  shake.
- Heavy hits sometimes start a fire (`HeavyHitFireChance` 15%, autocannon rounds 2%) at one of `FireSpotA/B/C`
  (under the dashboard, in front of the ammo rack, the cable loom on the right wall). At most `MaxFires` (2) places
  burn at once, each fire in a different place, and no new one starts for `MinTimeBetweenFires` (20 s) after the
  last. A fire grows, burns HP and, left alone, spreads to the next spot (never past the limit). The display shows
  **FIRE! USE THE EXTINGUISHER**.
- `UCockpitExtinguisherComponent` (`Extinguisher`, red bottle on the right wall): grip it, point the nozzle, hold that
  hand's trigger. The powder puts out fires inside its cone. The bottle never runs out and has no cooldown (set
  `UsePerSecond` above 0 for a limited charge).
  G sprays at the biggest fire without hands.

## Smoke grenades

`UCockpitSmokeLauncherComponent` (`SmokeLauncher`, dashboard left of the screen): a guarded switch. Grip the red cover
(`SmokeCoverHinge`) and flip it up (it latches past 55 degrees), then push the button (`SmokeButton`) with a
fingertip. V does the same.

Like on a real tank the grenades come from two banks of launcher tubes on the turret sides (`TubesPerSide` 4 each,
shown as simple tubes until the turret model has its own: `bShowTubes`). Every tube fires one grenade; the left bank
covers the left half and the right bank the right half of a fan (`FanAngle` 44 degrees to each side) in front of the
turret. They burst about 22 m out (`ScreenDistance`) into an arc of smoke (`ATankSmokeCloud`) that stands on the
ground, hills included. The fan follows **the turret**, so turn it towards the threat first. For about 10 s enemy
gunners can't see through it. Two salvos, 25 s to reload one; the lamp is green when ready and blinks while
reloading.

## Radio and cockpit cables

A radio set hangs on the right wall, ahead of the extinguisher; its handset (`UCockpitRadioComponent`, component
`Radio`) sits on a hook and is tied to the set by a cord (`RadioCable`, a Cable Component).

- **Lift it to your ear** (within `EarDistance` of either ear) and command reports the enemies you cannot see right
  now (outside the vision cone or behind cover): the busiest quarter around the **hull** (front / left / right / rear)
  and what is there (enemy tanks / armored cars / AT gun / mixed armor), e.g. "Hammer one, contact left, enemy tanks.
  Over." Nobody hidden: "no contacts around you". A burst of static first, the voice `VoiceDelay` (0.25 s) later.
  The display shows the same line as a subtitle (`RADIO: CONTACT LEFT - ENEMY TANKS`).
- **It rings** (and the display says `RADIO CALL - PICK UP THE HANDSET`) when enemies appear out of sight that have
  not been reported yet. An unanswered call stops after `CallTimeout` (15 s) and tries again after
  `TimeBetweenCalls` (10 s). Enemies already reported do not make it ring again.
- Away from the ear the voice is only faint (`AwayVolume`); hanging up cuts it.
- **The cord only stretches so far:** past `MaxCableReach` (100 cm from the set) the handset slips out of the hand
  and goes back on its hook.
- Q / D-pad right plays the report without the handset (monitor play).

The voice lines are Windows' built-in speech (Microsoft Zira) through a handset radio chain
(`Tools/Sound/radio.py`). They read as radio traffic, but for a release they should be replaced with recorded lines
(same file names `SW_Radio_<Front|Left|Right|Rear>_<Tanks|Cars|ATGun|Mixed>` and `SW_Radio_Clear`).

Three more cables just hang in the cockpit (`CableLoomRight` along the right wall, `CableDash` under the dashboard,
`CableLeftWall` over the rack). They sag a few cm, sway when the tank pulls away, brakes and turns (the cable sim runs
in world space), and `UCockpitCableJoltComponent` gives every cable a kick when the gun fires or the hull is hit.
Their `EndLocation` is in `CockpitRoot` space; move or delete them freely, or add more Cable Components under
`CockpitRoot`. They have no collision, so keep them along the walls.

A hose on the extinguisher was left out on purpose: on a hand extinguisher the hose is short and rides in the same
hand as the bottle, so a cable sim would barely show.

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

## Sound

Everything is played through `UTankAudioSubsystem` (`Audio/TankAudio.h`): `PlayAt` for sounds out in the world (they
fade with distance), `PlayInside` for the cockpit and headset (same level wherever you are), `MakeLoop`/`SetLoop`
for engines, tracks, the turret motor and fires. The sounds are in `Content/TankGame/Audio` (`SW_*`, 48 kHz).

Each sound starts on the frame of its event, not on a timer:

| Moment | Sound |
|---|---|
| Round caught by the breech / auto load starts | `SW_Shell_Ram` |
| Round slides home (`AutoLoadTime`) | `SW_Shell_Seat` |
| Wedge shut (`CloseTime` later) | `SW_Breech_Close` + `SW_Gun_Ready` |
| Trigger | `SW_Cannon_Fire` at the muzzle + `SW_Cannon_Interior` inside (HE a little lower) |
| Empty trigger | `SW_Dry_Fire` |
| Shell lands | AP on a vehicle `SW_Impact_AP`, AP on ground/walls `SW_Impact_Dirt`, HE `SW_Explosion_HE` |
| Recoil over, breech drops open | `SW_Breech_Open` |
| Case stub touches the floor (and each bounce, quieter) | `SW_Casing_Drop` |
| Hit on our hull | heavy `SW_Hull_Hit`, autocannon `SW_Hull_Hit_Light` (fire damage makes no hit sound) |
| Smoke cover latches / button | `SW_Switch_Click` |
| Smoke launch / grenades burst (`FlightTime`) | `SW_Smoke_Salvo` / `SW_Smoke_Burst` |
| Target locked / gun on target | `SW_Lock_Beep` / `SW_On_Target` |
| Wave starts / cleared / game over | `SW_Wave_Start` / `SW_Wave_Clear` / `SW_Game_Over` |
| Enemy guns | tank `SW_Enemy_Cannon`, armored car `SW_Autocannon_Shot`, AT gun `SW_ATGun_Fire` |
| Enemy destroyed | `SW_Enemy_Destroyed`, then the wreck burns (`SW_Fire_Loop`) |

Loops follow the game: the engine (`SW_Engine_Loop`) revs with speed and stick, tracks (`SW_Tracks_Loop`) come in
when moving, the turret motor (`SW_Turret_Loop`) with traverse speed, cockpit fire and burning ground crackle with
their size, the extinguisher hisses while spraying, and `SW_Warning_Loop` sounds while an AT gun lays on you or the
fire has burned for `FireAlarmTime`. Enemies have their own engine loop (pitch per type: tank low, car high).

Big bangs share a headroom (`LoudHeadroom`, `LoudRecoveryTime`, `LoudFloor` on the subsystem): two of them at once
play as made, a third on top only as loud as the room left, so a pile-up never clips. Far bangs take less of it.
Measured in the editor: four bangs on one frame peak at -0.7 dBFS (they clipped before), a normal fight at about
-18.5 LUFS with peaks around -1.7 dBFS.

Every start is written to a journal (`GetJournal`, `GetLastPlayed`, `GetLastVolume`, `GetJournalText`), which is
how `Tank.Audio.*` checks that sounds land on their events. `StartRecording`/`StopRecording` record the game's mix
to a wav, e.g. for a gameplay video.

To change a sound: re-import over the `SW_*` asset, or set the sound slots on the components (`FireSound`,
`InteriorFireSound`, `APImpactSound`, `HEImpactSound`, `GroundImpactSound` on the cannon; `CaptureSound`,
`SeatSound`, `CloseSound`, `ReadySound`, `OpenSound`, `CasingSound` on the breech; `FireSound` on enemy guns; the
loops on `Sounds` of the tank and the enemies). To rebuild them all: `Tools/Sound/fetch.py`, `build.py`, then
`import_sounds.py` in the editor. Sources and authors: `Docs/SoundCredits.md`.

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
- Sounds: see **Sound** above; every slot can be swapped for another sound or a Sound Cue.

### What a final tank model has to match

The gameplay is tuned to the placeholder's size and pivots (player tank, cm, X forward, Z up, from the actor root):

| Part | Now | If the model differs |
|---|---|---|
| Hull | 600 x 320 cm, top at about 100 cm; `HullCollision` box 300/160/50 half size | resize `HullCollision`; set `TankMovement.TrackContact` to the track ends (now 250, 130) and `RideHeight` (90) so the tracks sit on the ground |
| Turret ring | `TurretPivot` at (-60, 0, 150) | turret mesh origin on the ring centre, under `TurretPivot`; keep it high enough that the gun clears the open cockpit when it swings round |
| Gun | `BarrelPivot` at (110, 0, 25) from the turret, barrel 500 cm along +X (2.5 m past the hull front) | barrel mesh origin on the trunnion, under `BarrelPivot`; set the cannon's `MuzzleDistance` (510) to the new tip |
| Crew | `CockpitRoot` (150, -70, 10), eyes about 120 cm above the seat | interior room for the sticks (26 cm either side), breech (22, -56, 64) and the rack behind it |
| Smoke dischargers | 4 per side on the turret, `BankOffset` (62, 100, 14) mirrored | set `BankOffset` to where the model's launchers are |

Hull, turret and gun must be separate meshes (they move separately), in centimetres, facing +X. The
`CockpitArmor*` plates only hide the white cockpit walls from outside; a real hull makes them unnecessary.
Placeholder proportions (checked in `UgurkanLevel`): player tank 2.9 m high, enemy tank 2.0 m, both guns about
2.5 m past the hull. The two tanks still differ in style and height; the final model replaces both.
CaribbeanCity's car meshes are at about half scale (sedan 2.4 m long): scale them x2 if they are placed. The enemy
Blueprints follow the same rule (`HullCollision`, `TurretPivot`, `BarrelPivot`, and their `Muzzle` point at the tip).

## Debugging

- `Tank.Debug.Movement 1` / `Tank.Debug.Targeting 1`: on-screen and in-world debug info (0 forces it off).
- `Tank.DestroyTarget` destroys the locked target, `Tank.NextWave` spawns the next test-spawner wave.
- `Tank.SkipWave` clears the arena wave and starts the next one, `Tank.Restart` restarts the match.

## Tests

*Session Frontend → Automation*, filter `Tank`, or:
`UnrealEditor-Cmd.exe TankGameVR.uproject -ExecCmds="Automation RunTests Tank;Quit" -unattended -nullrhi`

They cover steering behaviour and frame-rate independence, hills (climbing, leaning, too steep, never through a
building), aiming from a slope, target selection and cycling, the crosshair projection,
the Aim Assist setting, grabbing the cockpit sticks, boarding, input mappings, the overview camera, loading by hand
(alignment, ramming, pulling out, letting go), auto load, firing and refill, damage and death, the real enemy
Blueprint killed by two shots, and the cigarette. `Tank.Combat.*` covers reading Ugurkan's table, armour and AP/HE,
the HE blast and no friendly fire, burning ground from HE (hurts enemies and us, burns out), each enemy type's behaviour (tank closes in and fires, car circles and bursts, AT
gun lays then fires and smoke breaks it), fires and the extinguisher (the spraying hand does not fire the gun), the
smoke switch, waves / game over / restart, the hull repairing itself, the level setup (director, blockers,
placeholder enemies of another spawner becoming real ones), the vision cone with spawning out of sight, and waves
arriving in ones and twos. `Tank.Audio.*` checks that each sound starts on its event (loading, shot, impact,
breech, case stub, hits, smoke, beeps, engine and tracks) and that piled-up bangs share the headroom.
`Tank.Cockpit.Radio*` / `CablesSwingAndJolt` / `Tank.Content.CockpitCablesHangInPlace` cover the radio (rings for
unseen enemies only, reports the busiest side and type, static then voice, all clear, unanswered call gives up and
retries, the cord follows the handset and slips past its reach) and the cables (ends in place, little sag, sway when
driving, jolt on a shot or hit).

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
16. Sound with headphones: engine always there but not tiring, the shot heavy but not painful, loading clicks heard
    over the engine, hits from the left come from the left, beeps clear but not annoying.
17. Radio: when it rings, lift the handset to your ear: static, then the voice says where the unseen enemies are;
    the display shows the subtitle. Pull the cord far away: the handset slips back onto its hook. Cables in the
    cockpit sway when you drive off or turn and jump when the gun fires; they never cover the display or the sticks.

Not tested yet on a real headset: comfort values, stick/rack/breech reach, eye-height calibration, mouth position
(`MouthOffset`).
