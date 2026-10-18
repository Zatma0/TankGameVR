"""BP_PlayerTank (placeholder hull, turret and cockpit around the C++ ATankVehicle components),
BP_EnemyTank_Placeholder / BP_EnemyTank_TestMoving and BP_TankTestWaveSpawner.

All meshes are plain engine shapes with placeholder materials. Every visual is a separate, named component that
teammates can delete or re-point to final meshes; the gameplay code only uses the C++ components
(HullCollision, TurretPivot, BarrelPivot, AimOrigin, SightFrame, CockpitRoot, Seat, DriveStick, AimStick, Display)."""
import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary

BP_DIR = "/Game/TankGame/Blueprints"
MAT = "/Game/TankGame/Materials"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")
GRAB_CLASS = unreal.load_asset("/Game/XRFramework/Blueprints/BP_GrabComponent").generated_class()


def mat(name):
    return unreal.load_asset(f"{MAT}/{name}")


def get_or_create_bp(path, name, parent):
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        return eal.load_asset(full)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return tools.create_asset(name, path, unreal.Blueprint, factory)


def handles(bp):
    result = {}
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = sub.k2_find_subobject_data_from_handle(handle)
        obj = SDL.get_associated_object(data)
        if obj:
            result[str(SDL.get_variable_name(data))] = (handle, obj)
            if SDL.is_root_component(data):
                result["__root__"] = (handle, obj)
    return result


def component(bp, name, cls, parent):
    existing = handles(bp)
    if name in existing:
        return existing[name][1]
    params = unreal.AddNewSubobjectParams(parent_handle=existing[parent][0], new_class=cls, blueprint_context=bp)
    handle, fail = sub.add_new_subobject(params)
    if not SDL.is_handle_valid(handle):
        raise RuntimeError(f"{bp.get_name()}: could not add {name}: {fail}")
    sub.rename_subobject(handle, unreal.Text(name))
    return handles(bp)[name][1]


def mesh(bp, name, parent, shape, material, location, size_cm, rotation=(0.0, 0.0, 0.0)):
    """Static mesh sized in centimetres (engine basic shapes are 100 cm)."""
    comp = component(bp, name, unreal.StaticMeshComponent, parent)
    comp.set_editor_property("static_mesh", shape)
    comp.set_editor_property("relative_location", unreal.Vector(*location))
    comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(size_cm[0] / 100.0, size_cm[1] / 100.0, size_cm[2] / 100.0))
    comp.set_material(0, material)
    comp.set_collision_profile_name("NoCollision")
    return comp


def label(bp, name, parent, text, location, size=4.0, color=unreal.Color(235, 235, 235, 255)):
    comp = component(bp, name, unreal.TextRenderComponent, parent)
    comp.set_editor_property("text", text)
    comp.set_editor_property("world_size", size)
    comp.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    comp.set_editor_property("text_render_color", color)
    comp.set_editor_property("relative_location", unreal.Vector(*location))
    comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=180.0, roll=0.0))
    return comp


def compile_save(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)


def build_stick(bp, prefix, stick_component, knob_material):
    mesh(bp, f"{prefix}Boot", stick_component, CYLINDER, mat("MI_Control_Stick"), (0, 0, 1), (10, 10, 3))
    mesh(bp, f"{prefix}Shaft", stick_component, CYLINDER, mat("MI_Control_Stick"), (0, 0, 13), (3, 3, 26))
    knob = mesh(bp, f"{prefix}Knob", stick_component, SPHERE, knob_material, (0, 0, 27), (7, 7, 7))
    grab = component(bp, f"{prefix}Grab", GRAB_CLASS, f"{prefix}Knob")
    grab.set_editor_property("GrabType", type(grab.get_editor_property("GrabType")).CUSTOM)
    return knob, grab


def build_player_tank():
    bp = get_or_create_bp(f"{BP_DIR}/Tank", "BP_PlayerTank", unreal.TankVehicle)

    # Exterior. Root = hull centre, 90 cm above ground; cockpit opening at x 60..230, y -160..-10.
    mesh(bp, "HullLower", "HullCollision", CUBE, mat("MI_Tank_Hull"), (0, 0, -20), (600, 320, 60))
    mesh(bp, "HullUpperRear", "HullCollision", CUBE, mat("MI_Tank_Hull"), (-120, 0, 30), (360, 320, 40))
    mesh(bp, "HullUpperRight", "HullCollision", CUBE, mat("MI_Tank_Hull"), (180, 75, 30), (240, 170, 40))
    mesh(bp, "HullUpperFront", "HullCollision", CUBE, mat("MI_Tank_HullDark"), (265, -85, 30), (70, 150, 40))
    mesh(bp, "TrackLeft", "HullCollision", CUBE, mat("MI_Tank_Tracks"), (0, -175, -35), (620, 40, 90))
    mesh(bp, "TrackRight", "HullCollision", CUBE, mat("MI_Tank_Tracks"), (0, 175, -35), (620, 40, 90))
    mesh(bp, "TurretBody", "TurretPivot", CYLINDER, mat("MI_Tank_Hull"), (0, 0, 0), (220, 220, 60))
    mesh(bp, "TurretHatch", "TurretPivot", CYLINDER, mat("MI_Tank_HullDark"), (-40, 30, 35), (60, 60, 10))
    mesh(bp, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (175, 0, 0), (16, 16, 350), (90.0, 0.0, 0.0))
    mesh(bp, "BarrelMantlet", "BarrelPivot", CUBE, mat("MI_Tank_Barrel"), (5, 0, 0), (30, 50, 40))

    # Cockpit tub (seat-relative: the seat floor is the rider's tracking origin, eyes at ~120 cm).
    mesh(bp, "CockpitFloor", "CockpitRoot", CUBE, mat("MI_Cockpit_Interior"), (-5, -15, 1), (170, 150, 2))
    mesh(bp, "CockpitWallLeft", "CockpitRoot", CUBE, mat("MI_Cockpit_Interior"), (-5, -90, 45), (170, 4, 90))
    mesh(bp, "CockpitWallRight", "CockpitRoot", CUBE, mat("MI_Cockpit_Interior"), (-5, 60, 45), (170, 4, 90))
    mesh(bp, "CockpitWallBack", "CockpitRoot", CUBE, mat("MI_Cockpit_Interior"), (-90, -15, 45), (4, 150, 90))
    mesh(bp, "CockpitWallFront", "CockpitRoot", CUBE, mat("MI_Cockpit_Panel"), (80, -15, 40), (4, 150, 80))
    mesh(bp, "Dashboard", "CockpitRoot", CUBE, mat("MI_Cockpit_Panel"), (70, -15, 80), (24, 150, 6))
    mesh(bp, "SeatBase", "CockpitRoot", CUBE, mat("MI_Cockpit_Seat"), (-20, 0, 18), (40, 40, 36))
    mesh(bp, "SeatCushion", "CockpitRoot", CUBE, mat("MI_Cockpit_Seat"), (-20, 0, 40), (46, 46, 8))
    mesh(bp, "SeatBack", "CockpitRoot", CUBE, mat("MI_Cockpit_Seat"), (-45, 0, 75), (8, 46, 60), (-8.0, 0.0, 0.0))
    mesh(bp, "ConsoleLeft", "CockpitRoot", CUBE, mat("MI_Cockpit_Panel"), (38, -30, 32), (30, 26, 64))
    mesh(bp, "ConsoleRight", "CockpitRoot", CUBE, mat("MI_Cockpit_Panel"), (38, 30, 32), (30, 26, 64))

    # Display frame just behind the widget (screen faces the seat, tilted 20 deg back).
    normal_x, normal_z = -0.9397, 0.3420
    mesh(bp, "DisplayFrame", "CockpitRoot", CUBE, mat("MI_Display_Frame"), (66 - normal_x * 1.5, 0, 100 - normal_z * 1.5), (2, 40, 31), (20.0, 180.0, 0.0))
    mesh(bp, "DisplayMount", "CockpitRoot", CUBE, mat("MI_Display_Frame"), (70, 0, 87), (4, 6, 14))

    build_stick(bp, "Drive", "DriveStick", mat("MI_Control_DriveKnob"))
    build_stick(bp, "Aim", "AimStick", mat("MI_Control_AimKnob"))
    label(bp, "DriveLabel", "CockpitRoot", "DRIVE", (40, -26, 104))
    label(bp, "AimLabel", "CockpitRoot", "AIM", (40, 26, 104))

    # Small fill light so the cockpit is readable when the hull shadows it (no shadows: cheap for VR).
    light = component(bp, "CockpitLight", unreal.PointLightComponent, "CockpitRoot")
    light.set_editor_property("relative_location", unreal.Vector(10, -15, 150))
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_editor_property("intensity", 25.0)
    light.set_editor_property("attenuation_radius", 220.0)
    light.set_editor_property("cast_shadows", False)
    light.set_editor_property("light_color", unreal.Color(255, 236, 214, 255))

    compile_save(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("tank_mapping_context", unreal.load_asset("/Game/TankGame/Input/IMC_Tank"))
    cdo.set_editor_property("drive_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Drive"))
    cdo.set_editor_property("aim_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Aim"))
    cdo.set_editor_property("toggle_view_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_ToggleView"))
    compile_save(bp)
    return bp


def build_enemies():
    base = get_or_create_bp(f"{BP_DIR}/Enemies", "BP_EnemyTank_Placeholder", unreal.Actor)
    hull = mesh(base, "Hull", "__root__", CUBE, mat("MI_Enemy_Hull"), (0, 0, 60), (520, 300, 100))
    hull.set_collision_profile_name("BlockAllDynamic")
    mesh(base, "TrackLeft", "__root__", CUBE, mat("MI_Tank_Tracks"), (0, -160, 40), (540, 40, 80))
    mesh(base, "TrackRight", "__root__", CUBE, mat("MI_Tank_Tracks"), (0, 160, 40), (540, 40, 80))
    mesh(base, "Turret", "__root__", CYLINDER, mat("MI_Enemy_Turret"), (-30, 0, 140), (200, 200, 60))
    mesh(base, "Barrel", "__root__", CYLINDER, mat("MI_Tank_Barrel"), (170, 0, 145), (14, 14, 300), (90.0, 0.0, 0.0))
    targetable = component(base, "Targetable", unreal.TargetableComponent, "__root__")
    targetable.set_editor_property("target_point_offset", unreal.Vector(0, 0, 130))
    compile_save(base)

    moving = get_or_create_bp(f"{BP_DIR}/Enemies", "BP_EnemyTank_TestMoving", base.generated_class())
    component(moving, "TestMover", unreal.TestTargetMoverComponent, "__root__")
    compile_save(moving)

    spawner = get_or_create_bp(f"{BP_DIR}/Testing", "BP_TankTestWaveSpawner", unreal.TankTestWaveSpawner)
    compile_save(spawner)
    cdo = unreal.get_default_object(spawner.generated_class())
    cdo.set_editor_property("enemy_class", moving.generated_class())
    compile_save(spawner)
    return base, moving, spawner


tank = build_player_tank()
enemy, moving, spawner = build_enemies()
for bp in (tank, enemy, moving, spawner):
    comps = [k for k in handles(bp) if not k.startswith("__")]
    print(bp.get_name(), bp.get_editor_property("status"), len(comps), "components")
