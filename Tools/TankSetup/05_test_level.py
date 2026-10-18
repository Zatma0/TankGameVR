"""Builds LV_TankTest: open grid field with obstacles (motion cues), the player tank (the VR pawn boards it at
start), a test wave spawner in front, a spectator camera for the monitor, and lighting. Rebuilds from scratch."""
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


def bp_class(path):
    return unreal.load_asset(path).generated_class()


def place(cls, label, folder, location, yaw=0.0, pitch=0.0):
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(f"{ROOT}/{folder}")
    return actor


def block(label, folder, center, size, material):
    actor = place(unreal.StaticMeshActor, label, folder, center)
    comp = actor.static_mesh_component
    comp.set_static_mesh(CUBE)
    comp.set_material(0, material)
    comp.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


if eal.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
            eas.destroy_actor(actor)
else:
    les.new_level(LEVEL)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", bp_class("/Game/XRFramework/Blueprints/BP_XRGameMode"))

# Environment: 200 m grid field (the grid gives clear speed/turn cues in VR), scattered obstacles.
block("Ground", "Environment", (0, 0, -50), (20000, 20000, 100), GRID)
random.seed(7)
for index in range(18):
    x = random.uniform(-6000, 8000)
    y = random.uniform(-7000, 7000)
    if abs(x) < 1200 and abs(y) < 1200:
        continue
    size = (random.uniform(150, 500), random.uniform(150, 500), random.uniform(100, 400))
    block(f"Obstacle_{index:02d}", "Environment", (x, y, size[2] / 2.0), size, OBSTACLE)

sun = place(unreal.DirectionalLight, "Sun", "Environment", (0, 0, 1000), yaw=40.0, pitch=-45.0)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(7.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = place(unreal.SkyLight, "SkyLight", "Environment", (0, 0, 800))
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("real_time_capture", True)
place(unreal.SkyAtmosphere, "SkyAtmosphere", "Environment", (0, 0, 0))
place(unreal.ExponentialHeightFog, "HeightFog", "Environment", (0, 0, -100))

# Player: the VR pawn spawns at the start and boards the tank's seat automatically.
place(unreal.PlayerStart, "VR_PlayerStart", "Player", (0, 0, 120))
tank = place(bp_class("/Game/TankGame/Blueprints/Tank/BP_PlayerTank"), "PlayerTank", "Player", (0, 0, 90))
spectator = place(bp_class("/Game/VRSpectator/VRSpectator"), "VR_Spectator", "Player", (-900, -500, 450), yaw=25.0, pitch=-15.0)

# Enemies: test waves 25-45 m ahead.
place(bp_class("/Game/TankGame/Blueprints/Testing/BP_TankTestWaveSpawner"), "TestWaveSpawner", "Enemies", (0, 0, 0))

sign = place(unreal.TextRenderActor, "Sign_Controls", "Debug", (600, 0, 450), yaw=180.0)
sign.text_render.set_text("TANK TEST\nLeft stick / left cockpit stick: drive (push right = curve right)\n"
                          "Right stick / right cockpit stick: aim, flick left/right to switch target\n"
                          "Menu: Aim Assist On/Off   Console: Tank.Debug.Movement 1, Tank.Debug.Targeting 1")
sign.text_render.set_world_size(22.0)
sign.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)

les.save_current_level()
counts = {}
for actor in eas.get_all_level_actors():
    folder = str(actor.get_folder_path())
    counts[folder] = counts.get(folder, 0) + 1
print("level", LEVEL, counts)
