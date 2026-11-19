"""Builds LV_TankTest: a 120 x 120 m walled arena on a grid field with ruins and rocks for cover, the player tank
(the VR pawn boards it at start), enemy spawn points along the edges, the arena director, a nav mesh for the enemy
vehicles, a spectator camera for the monitor, and lighting. Rebuilds from scratch.
Ugurkan's real arena only needs the same pieces: NavMeshBoundsVolume, spawn points and BP_TankArenaDirector."""
import math
import random
import unreal

LEVEL = "/Game/TankGame/Maps/LV_TankTest"
ROOT = "TankTest"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
GRID = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray")
OBSTACLE = unreal.load_asset("/Game/TankGame/Materials/MI_Env_Obstacle")
WALL = unreal.load_asset("/Game/TankGame/Materials/MI_Env_Wall")
ROCK = unreal.load_asset("/Game/TankGame/Materials/MI_Env_Rock")
HALF = 6000  # arena half size, cm


def bp_class(path):
    return unreal.load_asset(path).generated_class()


def place(cls, label, folder, location, yaw=0.0, pitch=0.0):
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(f"{ROOT}/{folder}")
    return actor


def block(label, folder, center, size, material, yaw=0.0, roll=0.0):
    actor = place(unreal.StaticMeshActor, label, folder, center, yaw=yaw)
    if roll:
        actor.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=yaw, roll=roll), False)
    comp = actor.static_mesh_component
    comp.set_static_mesh(CUBE)
    comp.set_material(0, material)
    comp.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


def ruin(name, x, y, yaw):
    """Broken L of a house: two wall pieces of uneven height and some rubble."""
    rad = math.radians(yaw)
    fx, fy = math.cos(rad), math.sin(rad)
    rx, ry = -fy, fx
    block(f"{name}_WallA", "Arena/Ruins", (x, y, 160), (900, 60, 320), WALL, yaw)
    block(f"{name}_WallB", "Arena/Ruins", (x - fx * 420 + rx * 330, y - fy * 420 + ry * 330, 110), (60, 600, 220), WALL, yaw)
    block(f"{name}_Broken", "Arena/Ruins", (x + fx * 330 + rx * 30, y + fy * 330 + ry * 30, 260), (240, 60, 200), WALL, yaw)
    for index in range(3):
        offset = random.uniform(-350, 350)
        block(f"{name}_Rubble{index}", "Arena/Ruins", (x + fx * offset + rx * random.uniform(80, 220), y + fy * offset + ry * random.uniform(80, 220), 30),
              (random.uniform(80, 160), random.uniform(60, 140), 60), WALL, yaw + random.uniform(-30, 30))


def rocks(name, x, y):
    for index in range(random.randint(2, 4)):
        size = (random.uniform(180, 420), random.uniform(160, 380), random.uniform(120, 300))
        block(f"{name}_{index}", "Arena/Rocks", (x + random.uniform(-250, 250), y + random.uniform(-250, 250), size[2] * 0.4), size, ROCK,
              random.uniform(0, 90), random.uniform(-8, 8))


if eal.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)) or isinstance(actor, unreal.NavMeshBoundsVolume):
            eas.destroy_actor(actor)
else:
    les.new_level(LEVEL)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", bp_class("/Game/XRFramework/Blueprints/BP_XRGameMode"))

# Ground: 200 m grid field (the grid gives clear speed/turn cues in VR); the fight stays inside the walls.
block("Ground", "Environment", (0, 0, -50), (20000, 20000, 100), GRID)

# Arena walls (earth berm height, you can see over them from the turret but shells stop).
for name, center, size in (("North", (HALF, 0, 125), (300, HALF * 2 + 300, 250)), ("South", (-HALF, 0, 125), (300, HALF * 2 + 300, 250)),
                           ("East", (0, HALF, 125), (HALF * 2 + 300, 300, 250)), ("West", (0, -HALF, 125), (HALF * 2 + 300, 300, 250))):
    block(f"ArenaWall_{name}", "Arena/Walls", center, size, WALL)

# Cover: ruins and rock groups in a loose ring, the middle stays open for the start.
random.seed(11)
for index, (x, y, yaw) in enumerate(((2600, 1800, 20), (-2400, 2600, -40), (-2900, -1700, 70), (2200, -2800, -15), (4300, 3900, 45), (-4200, -4100, 10))):
    ruin(f"Ruin_{index}", x, y, yaw)
for index, (x, y) in enumerate(((800, 3600), (3800, -500), (-1200, -3800), (-3900, 600), (4700, -4300), (-4600, 4500), (1500, -1500), (-1600, 1300))):
    rocks(f"Rocks_{index}", x, y)
for index in range(10):
    x = random.uniform(-HALF + 600, HALF - 600)
    y = random.uniform(-HALF + 600, HALF - 600)
    if abs(x) < 1500 and abs(y) < 1500:
        continue
    size = (random.uniform(150, 400), random.uniform(150, 400), random.uniform(100, 300))
    block(f"Obstacle_{index:02d}", "Arena/Obstacles", (x, y, size[2] / 2.0), size, OBSTACLE, random.uniform(0, 90))

sun = place(unreal.DirectionalLight, "Sun", "Environment", (0, 0, 1000), yaw=40.0, pitch=-45.0)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(7.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = place(unreal.SkyLight, "SkyLight", "Environment", (0, 0, 800))
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("real_time_capture", False)
sky.light_component.set_editor_property("intensity", 1.5)
sky_sphere = place(unreal.load_asset("/Engine/EngineSky/BP_Sky_Sphere").generated_class(), "SkySphere", "Environment", (0, 0, 0))
sky_sphere.set_editor_property("Directional light actor", sun)
sky_sphere.set_editor_property("Refresh material", True)
place(unreal.ExponentialHeightFog, "HeightFog", "Environment", (0, 0, -100))

# Nav mesh for the enemy vehicles (built at runtime, see DefaultEngine.ini).
nav = place(unreal.NavMeshBoundsVolume, "NavMeshBounds", "Arena", (0, 0, 200))
nav.set_actor_scale3d(unreal.Vector(HALF * 2 / 200.0 + 2, HALF * 2 / 200.0 + 2, 6.0))

# Player: the VR pawn spawns at the start and boards the tank's seat automatically.
place(unreal.PlayerStart, "VR_PlayerStart", "Player", (0, 0, 120))
tank = place(bp_class("/Game/TankGame/Blueprints/Tank/BP_PlayerTank"), "PlayerTank", "Player", (0, 0, 90))
spectator = place(bp_class("/Game/VRSpectator/VRSpectator"), "VR_Spectator", "Player", (-1500, -900, 650), yaw=28.0, pitch=-15.0)

# Enemies come in from the edges; every third point is for the light vehicles and AT guns only.
for index in range(10):
    angle = index * 36.0 + 18.0
    radius = HALF - 900
    x, y = math.cos(math.radians(angle)) * radius, math.sin(math.radians(angle)) * radius
    point = place(unreal.TankEnemySpawnPoint, f"EnemySpawn_{index:02d}", "Enemies", (x, y, 50), yaw=angle + 180.0)
    if index % 3 == 1:
        point.set_editor_property("allow_tanks", False)
place(bp_class("/Game/TankGame/Blueprints/Game/BP_TankArenaDirector"), "ArenaDirector", "Enemies", (0, 0, 0))

sign = place(unreal.TextRenderActor, "Sign_Controls", "Debug", (600, 0, 450), yaw=180.0)
sign.text_render.set_text("TANK TEST ARENA - survive 5 waves\n"
                          "Drive: left stick / WASD   Aim: right stick / arrows   Fire: trigger / Space\n"
                          "Load: rack by hand / R = AP, H = HE   Smoke: dashboard switch / V   Fire out: extinguisher / G\n"
                          "Console: Tank.SkipWave, Tank.Restart, Tank.Debug.Targeting 1")
sign.text_render.set_world_size(22.0)
sign.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)

sky.light_component.recapture_sky()
les.save_current_level()
counts = {}
for actor in eas.get_all_level_actors():
    folder = str(actor.get_folder_path())
    counts[folder] = counts.get(folder, 0) + 1
print("level", LEVEL, counts)
