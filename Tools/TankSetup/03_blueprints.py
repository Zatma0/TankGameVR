"""BP_PlayerTank (placeholder hull, turret and cockpit around the C++ ATankVehicle components),
BP_EnemyTank_Placeholder / BP_EnemyTank_TestMoving and BP_TankTestWaveSpawner.

All meshes are plain engine shapes with placeholder materials. Every visual is a separate, named component that
teammates can delete or re-point to final meshes; the gameplay code only uses the C++ components
(HullCollision, TurretPivot, BarrelPivot, AimOrigin, SightFrame, CockpitRoot, Seat, DriveStick, AimStick, Display,
Breech, ShellA/B/C, Cigarette) and a few named visuals (BreechBlock, ReadyLamp, CasingStub, MuzzleFlash,
MuzzleFlashLight, CigaretteEmber, CigaretteGlow)."""
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
CONE = unreal.load_asset("/Engine/BasicShapes/Cone")
GRAB_CLASS = unreal.load_asset("/Game/XRFramework/Blueprints/BP_GrabComponent").generated_class()


def mat(name):
    return unreal.load_asset(f"{MAT}/{name}")


def rgb(r, g, b):
    # unreal.Color takes (b, g, r, a) positionally, so always name them.
    return unreal.Color(r=r, g=g, b=b, a=255)


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


def label(bp, name, parent, text, location, size=4.0, color=None, yaw=180.0):
    comp = component(bp, name, unreal.TextRenderComponent, parent)
    comp.set_editor_property("text", text)
    comp.set_editor_property("world_size", size)
    comp.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    comp.set_editor_property("text_render_color", color or rgb(235, 235, 235))
    comp.set_editor_property("relative_location", unreal.Vector(*location))
    comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0))
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


def grab(bp, name, surface):
    comp = component(bp, name, GRAB_CLASS, surface)
    comp.set_editor_property("GrabType", type(comp.get_editor_property("GrabType")).CUSTOM)
    return comp


def point_light(bp, name, parent, location, intensity, radius, color):
    light = component(bp, name, unreal.PointLightComponent, parent)
    light.set_editor_property("relative_location", unreal.Vector(*location))
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_editor_property("intensity", intensity)
    light.set_editor_property("attenuation_radius", radius)
    light.set_editor_property("cast_shadows", False)
    light.set_editor_property("light_color", color)
    return light


def build_gun(bp):
    """Ready rack with four rounds (AP on top, HE below), the breech and the muzzle flash.
    Round axis = X, nose at +25 cm."""
    for name, he in (("ShellA", False), ("ShellB", False), ("ShellC", True), ("ShellD", True)):
        tip = mat("MI_Ammo_TipHE") if he else mat("MI_Ammo_TipAP")
        mesh(bp, f"{name}Case", name, CYLINDER, mat("MI_Ammo_Case"), (-7.5, 0, 0), (12, 12, 35), (90.0, 0.0, 0.0))
        mesh(bp, f"{name}Body", name, CYLINDER, tip, (13.5, 0, 0), (11, 11, 7), (90.0, 0.0, 0.0))
        mesh(bp, f"{name}Nose", name, CONE, tip, (21, 0, 0), (11 if he else 6, 11 if he else 6, 8), (-90.0, 0.0, 0.0))
        if he:
            mesh(bp, f"{name}Band", name, CYLINDER, mat("MI_Ammo_BandHE"), (12.0, 0, 0), (11.4, 11.4, 1.6), (90.0, 0.0, 0.0))
        grab(bp, f"{name}Grab", f"{name}Case")

    # Rack (rounds at x -55..-5, y -56, z 87/73/59/45).
    mesh(bp, "AmmoRackBase", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-30, -56, 36), (56, 20, 4))
    mesh(bp, "AmmoRackRear", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-59, -56, 66), (4, 20, 64))
    mesh(bp, "AmmoRackCradleRear", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-45, -64, 66), (4, 4, 60))
    mesh(bp, "AmmoRackCradleFront", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-15, -64, 66), (4, 4, 60))
    mesh(bp, "AmmoRackDivider", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-30, -64, 66), (40, 3, 2))
    label(bp, "AmmoLabel", "CockpitRoot", "AMMO", (-30, -56, 102), 3.5)
    label(bp, "AmmoLabelAP", "CockpitRoot", "AP", (-56.5, -42, 79), 3.0, yaw=0.0)
    label(bp, "AmmoLabelHE", "CockpitRoot", "HE", (-56.5, -42, 51), 3.0, rgb(240, 200, 40), yaw=0.0)

    # Breech: 14 x 14 cm mouth at the component, housing runs forward.
    mesh(bp, "BreechTop", "Breech", CUBE, mat("MI_Gun_Breech"), (24, 0, 10), (48, 26, 6))
    mesh(bp, "BreechBottom", "Breech", CUBE, mat("MI_Gun_Breech"), (24, 0, -10), (48, 26, 6))
    mesh(bp, "BreechLeft", "Breech", CUBE, mat("MI_Gun_Breech"), (24, -10, 0), (48, 6, 14))
    mesh(bp, "BreechRight", "Breech", CUBE, mat("MI_Gun_Breech"), (24, 10, 0), (48, 6, 14))
    mesh(bp, "BreechBlock", "Breech", CUBE, mat("MI_Tank_Barrel"), (-3, 0, 0), (5, 26, 26))
    mesh(bp, "ReadyLamp", "Breech", SPHERE, mat("MI_Gun_ReadyLamp"), (-2, 0, 17), (4, 4, 4))
    mesh(bp, "CasingStub", "Breech", CYLINDER, mat("MI_Ammo_Case"), (0, 0, 0), (12, 12, 6), (90.0, 0.0, 0.0))
    label(bp, "BreechLabel", "Breech", "LOAD", (-2, 0, 22), 3.5)

    # Shown for a moment on each shot (the barrel is 500 cm long).
    mesh(bp, "MuzzleFlash", "BarrelPivot", SPHERE, mat("MI_Tank_MuzzleFlash"), (530, 0, 0), (70, 45, 45))
    point_light(bp, "MuzzleFlashLight", "BarrelPivot", (545, 0, 0), 300.0, 2500.0, rgb(255, 190, 110))


def build_cigarette(bp):
    """Cigarette on an ashtray, right side of the dashboard. Axis = X, ember at +5 cm, filter at -5 cm."""
    mesh(bp, "Ashtray", "CockpitRoot", CYLINDER, mat("MI_Ashtray"), (62, 40, 84), (11, 11, 2))
    mesh(bp, "CigarettePaper", "Cigarette", CYLINDER, mat("MI_Cigarette_Paper"), (1.25, 0, 0), (1.2, 1.2, 7.5), (90.0, 0.0, 0.0))
    mesh(bp, "CigaretteFilter", "Cigarette", CYLINDER, mat("MI_Cigarette_Filter"), (-3.75, 0, 0), (1.25, 1.25, 2.5), (90.0, 0.0, 0.0))
    mesh(bp, "CigaretteEmber", "Cigarette", CYLINDER, mat("MI_Cigarette_Ember"), (5.3, 0, 0), (1.15, 1.15, 0.6), (90.0, 0.0, 0.0))
    point_light(bp, "CigaretteGlow", "Cigarette", (6.5, 0, 0), 0.0, 35.0, rgb(255, 120, 40))
    grab(bp, "CigaretteGrab", "CigarettePaper")


def build_extinguisher(bp):
    """Red bottle on a bracket on the right wall. Item axis X = nozzle direction, nozzle tip at (12, 0, 14)."""
    mesh(bp, "ExtinguisherBracket", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-8, 57, 56), (12, 3, 34))
    mesh(bp, "ExtinguisherStrap", "CockpitRoot", CUBE, mat("MI_Ammo_Rack"), (-8, 52, 50), (16, 12, 2))
    mesh(bp, "ExtinguisherBottle", "Extinguisher", CYLINDER, mat("MI_Extinguisher"), (0, 0, -12), (13, 13, 40))
    mesh(bp, "ExtinguisherCap", "Extinguisher", SPHERE, mat("MI_Extinguisher"), (0, 0, 8), (13, 13, 6))
    mesh(bp, "ExtinguisherValve", "Extinguisher", CUBE, mat("MI_Extinguisher_Metal"), (0, 0, 12), (5, 4, 6))
    mesh(bp, "ExtinguisherLever", "Extinguisher", CUBE, mat("MI_Extinguisher_Metal"), (-5, 0, 16), (11, 3, 1.5), (-10.0, 0.0, 0.0))
    mesh(bp, "ExtinguisherNozzle", "Extinguisher", CYLINDER, mat("MI_Extinguisher_Metal"), (6, 0, 14), (2.5, 2.5, 12), (90.0, 0.0, 0.0))
    mesh(bp, "ExtinguisherLabelBand", "Extinguisher", CYLINDER, mat("MI_Smoke_Button"), (0, 0, -8), (13.3, 13.3, 7))
    grab(bp, "ExtinguisherGrab", "ExtinguisherBottle")
    label(bp, "ExtinguisherSign", "CockpitRoot", "FIRE", (-8, 57.5, 80), 3.0, rgb(255, 60, 40), yaw=-90.0)


def build_smoke_switch(bp):
    """Guarded switch on the dashboard: base plate, button, ready lamp and a red cover hinged at the front edge."""
    mesh(bp, "SmokePanel", "SmokeLauncher", CUBE, mat("MI_Cockpit_Panel"), (0, 0, 0.8), (13, 11, 1.6))
    mesh(bp, "SmokeButtonCollar", "SmokeLauncher", CYLINDER, mat("MI_Extinguisher_Metal"), (-0.5, 0, 1.9), (4.6, 4.6, 0.8))
    mesh(bp, "SmokeButton", "SmokeLauncher", CYLINDER, mat("MI_Smoke_Button"), (-0.5, 0, 2.8), (3.4, 3.4, 1.6))
    mesh(bp, "SmokeLamp", "SmokeLauncher", SPHERE, mat("MI_Smoke_Lamp"), (-4.5, 4.0, 2.0), (1.4, 1.4, 1.4))
    hinge = component(bp, "SmokeCoverHinge", unreal.SceneComponent, "SmokeLauncher")
    # +X of the hinge runs over the button towards the seat; opening pitches it up and over to the front.
    hinge.set_editor_property("relative_location", unreal.Vector(5.5, 0, 4.0))
    hinge.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=180.0, roll=0.0))
    mesh(bp, "SmokeCover", "SmokeCoverHinge", CUBE, mat("MI_Smoke_Cover"), (5.5, 0, 0.3), (11, 9, 1.0))
    mesh(bp, "SmokeCoverLipLeft", "SmokeCoverHinge", CUBE, mat("MI_Smoke_Cover"), (5.5, -4.3, -1.3), (11, 0.6, 2.6))
    mesh(bp, "SmokeCoverLipRight", "SmokeCoverHinge", CUBE, mat("MI_Smoke_Cover"), (5.5, 4.3, -1.3), (11, 0.6, 2.6))
    mesh(bp, "SmokeCoverHingePin", "SmokeLauncher", CYLINDER, mat("MI_Extinguisher_Metal"), (5.5, 0, 4.0), (1.0, 1.0, 10), (0.0, 0.0, 90.0))
    grab(bp, "SmokeCoverGrab", "SmokeCover")
    label(bp, "SmokeLabel", "SmokeLauncher", "SMOKE", (0, 0, 9), 2.4, rgb(255, 90, 60))


def build_player_tank():
    bp = get_or_create_bp(f"{BP_DIR}/Tank", "BP_PlayerTank", unreal.TankVehicle)

    # Exterior. Root = hull centre, 90 cm above ground; cockpit opening at x 60..230, y -160..-10.
    mesh(bp, "HullLower", "HullCollision", CUBE, mat("MI_Tank_Hull"), (0, 0, -20), (600, 320, 60))
    mesh(bp, "HullUpperRear", "HullCollision", CUBE, mat("MI_Tank_Hull"), (-120, 0, 30), (360, 320, 40))
    mesh(bp, "HullUpperRight", "HullCollision", CUBE, mat("MI_Tank_Hull"), (180, 75, 30), (240, 170, 40))
    mesh(bp, "HullUpperFront", "HullCollision", CUBE, mat("MI_Tank_HullDark"), (265, -85, 30), (70, 150, 40))
    mesh(bp, "TrackLeft", "HullCollision", CUBE, mat("MI_Tank_Tracks"), (0, -175, -35), (620, 40, 90))
    mesh(bp, "TrackRight", "HullCollision", CUBE, mat("MI_Tank_Tracks"), (0, 175, -35), (620, 40, 90))
    mesh(bp, "TurretRing", "TurretPivot", CYLINDER, mat("MI_Tank_HullDark"), (0, 0, -65), (190, 190, 72))
    mesh(bp, "TurretBody", "TurretPivot", CYLINDER, mat("MI_Tank_Hull"), (0, 0, 0), (220, 220, 60))
    mesh(bp, "TurretHatch", "TurretPivot", CYLINDER, mat("MI_Tank_HullDark"), (-40, 30, 35), (60, 60, 10))
    mesh(bp, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (255, 0, 0), (16, 16, 500), (90.0, 0.0, 0.0))
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
    build_gun(bp)
    build_cigarette(bp)
    build_extinguisher(bp)
    build_smoke_switch(bp)

    # Small fill light so the cockpit is readable when the hull shadows it (no shadows: cheap for VR).
    light = component(bp, "CockpitLight", unreal.PointLightComponent, "CockpitRoot")
    light.set_editor_property("relative_location", unreal.Vector(10, -15, 150))
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_editor_property("intensity", 25.0)
    light.set_editor_property("attenuation_radius", 220.0)
    light.set_editor_property("cast_shadows", False)
    light.set_editor_property("light_color", rgb(255, 236, 214))

    compile_save(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("tank_mapping_context", unreal.load_asset("/Game/TankGame/Input/IMC_Tank"))
    cdo.set_editor_property("drive_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Drive"))
    cdo.set_editor_property("aim_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Aim"))
    cdo.set_editor_property("toggle_view_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_ToggleView"))
    cdo.set_editor_property("fire_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Fire"))
    cdo.set_editor_property("auto_load_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_AutoLoad"))
    cdo.set_editor_property("toggle_aim_assist_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_ToggleAimAssist"))
    cdo.set_editor_property("load_he_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_LoadHE"))
    cdo.set_editor_property("smoke_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Smoke"))
    cdo.set_editor_property("extinguish_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_Extinguish"))
    cdo.set_editor_property("trigger_left_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_TriggerLeft"))
    cdo.set_editor_property("trigger_right_action", unreal.load_asset("/Game/TankGame/Input/IA_Tank_TriggerRight"))
    compile_save(bp)
    return bp


def build_enemies():
    base = get_or_create_bp(f"{BP_DIR}/Enemies", "BP_EnemyTank_Placeholder", unreal.Actor)
    hull = mesh(base, "Hull", "__root__", CUBE, mat("MI_Enemy_Hull"), (0, 0, 60), (520, 300, 100))
    hull.set_collision_profile_name("BlockAllDynamic")
    mesh(base, "TrackLeft", "__root__", CUBE, mat("MI_Tank_Tracks"), (0, -160, 40), (540, 40, 80))
    mesh(base, "TrackRight", "__root__", CUBE, mat("MI_Tank_Tracks"), (0, 160, 40), (540, 40, 80))
    # Shells can come in high (long range, uphill), so the turret stops them too.
    turret = mesh(base, "Turret", "__root__", CYLINDER, mat("MI_Enemy_Turret"), (-30, 0, 140), (200, 200, 60))
    turret.set_collision_profile_name("BlockAllDynamic")
    mesh(base, "Barrel", "__root__", CYLINDER, mat("MI_Tank_Barrel"), (170, 0, 145), (14, 14, 300), (90.0, 0.0, 0.0))
    targetable = component(base, "Targetable", unreal.TargetableComponent, "__root__")
    # Centre of the silhouette (hull 10..110, turret up to 170), so shots go into the middle, not over the hull.
    targetable.set_editor_property("target_point_offset", unreal.Vector(0, 0, 85))
    health = component(base, "Health", unreal.TankHealthComponent, "__root__")
    health.set_editor_property("max_health", 50.0)
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


def blocker(comp):
    """Placeholder part that stops shells (the hull box alone misses turret hits)."""
    comp.set_collision_profile_name("BlockAllDynamic")
    return comp


def remove(bp, name):
    """Drops a component an older version of this script made."""
    existing = handles(bp)
    if name in existing:
        context = sub.k2_gather_subobject_data_for_blueprint(bp)[0]
        sub.delete_subobject(context, existing[name][0], bp)


def wreck_fire(bp, location, size):
    """Flames, glow and a smoke column that only show on the wreck (names start with WreckFire)."""
    remove(bp, "WreckFireSmoke")
    mesh(bp, "WreckFire", "__root__", CONE, mat("MI_Fire_Flame"), location, size)
    # A short column of dark smoke leaning downwind.
    for index in range(3):
        grow = 1.0 + index * 0.6
        mesh(bp, f"WreckFireSmoke{index}", "__root__", SPHERE, mat("MI_Fire_Smoke"),
             (location[0] - 40 - index * 70, location[1] + index * 25, location[2] + size[2] * (0.9 + index * 0.9)), (size[0] * 1.6 * grow, size[0] * 1.6 * grow, size[0] * 1.4 * grow))
    point_light(bp, "WreckFireLight", "__root__", (location[0], location[1], location[2] + 60), 40.0, 1500.0, rgb(255, 140, 60))


def muzzle_flash(bp, location, size):
    mesh(bp, "MuzzleFlash", "BarrelPivot", SPHERE, mat("MI_Tank_MuzzleFlash"), location, size)


def build_enemy_tank():
    """Main battle tank. Root = hull box centre, 70 cm above the ground (ground at z -70)."""
    bp = get_or_create_bp(f"{BP_DIR}/Enemies", "BP_Enemy_Tank", unreal.TankEnemy_Tank)
    blocker(mesh(bp, "Hull", "__root__", CUBE, mat("MI_Enemy_Hull"), (0, 0, 0), (520, 290, 100)))
    mesh(bp, "HullGlacis", "__root__", CUBE, mat("MI_Enemy_Hull"), (262, 0, 12), (60, 280, 70), (35.0, 0.0, 0.0))
    mesh(bp, "HullRearDeck", "__root__", CUBE, mat("MI_Enemy_Turret"), (-180, 0, 55), (150, 240, 14))
    for side, y in (("Left", -165), ("Right", 165)):
        mesh(bp, f"Track{side}", "__root__", CUBE, mat("MI_Tank_Tracks"), (0, y, -22), (560, 46, 90))
        mesh(bp, f"Fender{side}", "__root__", CUBE, mat("MI_Enemy_Hull"), (0, y, 26), (580, 56, 6))
        for index, x in enumerate((-200, -100, 0, 100, 200)):
            mesh(bp, f"Wheel{side}{index}", "__root__", CYLINDER, mat("MI_Tank_Barrel"), (x, y * 1.16, -30), (62, 62, 8), (0.0, 0.0, 90.0))
    blocker(mesh(bp, "Turret", "TurretPivot", CYLINDER, mat("MI_Enemy_Turret"), (0, 0, 5), (210, 210, 55)))
    mesh(bp, "TurretBustle", "TurretPivot", CUBE, mat("MI_Enemy_Turret"), (-115, 0, 8), (80, 150, 45))
    mesh(bp, "TurretCupola", "TurretPivot", CYLINDER, mat("MI_Enemy_Hull"), (-35, 52, 40), (48, 48, 22))
    mesh(bp, "TurretAntenna", "TurretPivot", CYLINDER, mat("MI_Tank_Tracks"), (-130, -55, 90), (2, 2, 150))
    mesh(bp, "Mantlet", "BarrelPivot", CUBE, mat("MI_Enemy_Turret"), (5, 0, 0), (40, 70, 50))
    mesh(bp, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (245, 0, 0), (16, 16, 470), (90.0, 0.0, 0.0))
    mesh(bp, "BarrelEvacuator", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (270, 0, 0), (24, 24, 40), (90.0, 0.0, 0.0))
    muzzle_flash(bp, (505, 0, 0), (80, 50, 50))
    wreck_fire(bp, (-60, 0, 90), (90, 90, 150))
    compile_save(bp)
    return bp


def build_enemy_car():
    """Six-wheeled armored car with a small autocannon turret. Ground at z -80."""
    bp = get_or_create_bp(f"{BP_DIR}/Enemies", "BP_Enemy_ArmoredCar", unreal.TankEnemy_ArmoredCar)
    blocker(mesh(bp, "Body", "__root__", CUBE, mat("MI_Enemy_Car"), (-10, 0, 0), (430, 230, 90)))
    mesh(bp, "BodyNose", "__root__", CUBE, mat("MI_Enemy_Car"), (215, 0, -8), (70, 220, 60), (-30.0, 0.0, 0.0))
    mesh(bp, "BodyRoof", "__root__", CUBE, mat("MI_Enemy_Car"), (-30, 0, 52), (330, 200, 14))
    mesh(bp, "VisionSlit", "__root__", CUBE, mat("MI_Display_Frame"), (176, 0, 30), (4, 120, 8), (-30.0, 0.0, 0.0))
    mesh(bp, "Headlights", "__root__", CUBE, mat("MI_Ammo_Case"), (238, 0, 6), (4, 190, 10))
    for side, y in (("Left", -125), ("Right", 125)):
        for index, x in enumerate((-150, 0, 150)):
            mesh(bp, f"Wheel{side}{index}", "__root__", CYLINDER, mat("MI_Tank_Tracks"), (x, y, -38), (84, 84, 30), (0.0, 0.0, 90.0))
            mesh(bp, f"Hub{side}{index}", "__root__", CYLINDER, mat("MI_Tank_Barrel"), (x, y * 1.13, -38), (32, 32, 4), (0.0, 0.0, 90.0))
    blocker(mesh(bp, "Turret", "TurretPivot", CYLINDER, mat("MI_Enemy_Turret"), (0, 0, 15), (115, 115, 42)))
    mesh(bp, "TurretHatch", "TurretPivot", CYLINDER, mat("MI_Enemy_Car"), (-20, 0, 38), (45, 45, 6))
    mesh(bp, "GunMount", "BarrelPivot", CUBE, mat("MI_Enemy_Turret"), (0, 0, 0), (28, 26, 24))
    mesh(bp, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (75, 0, 0), (7, 7, 140), (90.0, 0.0, 0.0))
    mesh(bp, "BarrelFlash", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (140, 0, 0), (10, 10, 16), (90.0, 0.0, 0.0))
    muzzle_flash(bp, (160, 0, 0), (40, 28, 28))
    wreck_fire(bp, (-40, 0, 70), (70, 70, 120))
    compile_save(bp)
    return bp


def build_enemy_at_gun():
    """Towed anti-tank gun with a gun shield, split trails and two crew. Ground at z -55."""
    bp = get_or_create_bp(f"{BP_DIR}/Enemies", "BP_Enemy_AntiTank", unreal.TankEnemy_AntiTank)
    mesh(bp, "Axle", "__root__", CYLINDER, mat("MI_Tank_Barrel"), (0, 0, -15), (14, 14, 170), (0.0, 0.0, 90.0))
    for side, y in (("Left", -88), ("Right", 88)):
        mesh(bp, f"Wheel{side}", "__root__", CYLINDER, mat("MI_Tank_Tracks"), (0, y, -15), (80, 80, 16), (0.0, 0.0, 90.0))
        mesh(bp, f"Trail{side}", "__root__", CUBE, mat("MI_Enemy_ATGun"), (-120, y * 0.45, -38), (210, 12, 12), (-5.0, -12.0 if y < 0 else 12.0, 0.0))
        mesh(bp, f"Spade{side}", "__root__", CUBE, mat("MI_Enemy_ATGun"), (-222, y * 0.95, -48), (6, 26, 20))
    blocker(mesh(bp, "Carriage", "__root__", CUBE, mat("MI_Enemy_ATGun"), (0, 0, 5), (90, 70, 40)))
    blocker(mesh(bp, "Shield", "TurretPivot", CUBE, mat("MI_Enemy_ATGun"), (35, 0, 30), (8, 180, 110), (-12.0, 0.0, 0.0)))
    mesh(bp, "ShieldWingLeft", "TurretPivot", CUBE, mat("MI_Enemy_ATGun"), (22, -98, 28), (8, 36, 100), (-12.0, 35.0, 0.0))
    mesh(bp, "ShieldWingRight", "TurretPivot", CUBE, mat("MI_Enemy_ATGun"), (22, 98, 28), (8, 36, 100), (-12.0, -35.0, 0.0))
    mesh(bp, "Cradle", "BarrelPivot", CUBE, mat("MI_Enemy_ATGun"), (-30, 0, 0), (110, 26, 30))
    mesh(bp, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (140, 0, 0), (11, 11, 300), (90.0, 0.0, 0.0))
    mesh(bp, "MuzzleBrake", "BarrelPivot", CUBE, mat("MI_Tank_Barrel"), (288, 0, 0), (22, 18, 16))
    mesh(bp, "RecoilCylinder", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (40, 0, -11), (12, 12, 120), (90.0, 0.0, 0.0))
    muzzle_flash(bp, (315, 0, 0), (90, 55, 55))
    # Laser rangefinder: the code stretches AimLaser along X towards the target.
    laser = component(bp, "AimLaser", unreal.SceneComponent, "BarrelPivot")
    laser.set_editor_property("relative_location", unreal.Vector(20, 0, 10))
    mesh(bp, "AimLaserBeam", "AimLaser", CUBE, mat("MI_Enemy_Laser"), (50, 0, 0), (100, 8, 8))
    # Crew: gunner behind the shield, loader at the breech.
    for name, x, y in (("Gunner", -55, -45), ("Loader", -80, 50)):
        mesh(bp, f"{name}Body", "TurretPivot", CYLINDER, mat("MI_Enemy_Crew"), (x, y, -12), (36, 30, 70))
        mesh(bp, f"{name}Head", "TurretPivot", SPHERE, mat("MI_Enemy_Skin"), (x, y, 33), (22, 22, 24))
        mesh(bp, f"{name}Helmet", "TurretPivot", SPHERE, mat("MI_Enemy_Crew"), (x, y, 40), (27, 27, 14))
    wreck_fire(bp, (0, 0, 40), (60, 60, 100))
    compile_save(bp)
    return bp


def build_arena(enemy_tank, enemy_car, enemy_at):
    director = get_or_create_bp(f"{BP_DIR}/Game", "BP_TankArenaDirector", unreal.TankArenaDirector)
    compile_save(director)
    cdo = unreal.get_default_object(director.generated_class())
    cdo.set_editor_property("tank_class", enemy_tank.generated_class())
    cdo.set_editor_property("armored_car_class", enemy_car.generated_class())
    cdo.set_editor_property("anti_tank_class", enemy_at.generated_class())
    compile_save(director)
    return director


tank = build_player_tank()
enemy, moving, spawner = build_enemies()
enemy_tank = build_enemy_tank()
enemy_car = build_enemy_car()
enemy_at = build_enemy_at_gun()
director = build_arena(enemy_tank, enemy_car, enemy_at)
for bp in (tank, enemy, moving, spawner, enemy_tank, enemy_car, enemy_at, director):
    comps = [k for k in handles(bp) if not k.startswith("__")]
    print(bp.get_name(), bp.get_editor_property("status"), len(comps), "components")
