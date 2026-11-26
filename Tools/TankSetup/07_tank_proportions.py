"""Proportion fixes on the placeholder tanks: hull-coloured armour around the open cockpit (from outside the white
cockpit walls no longer show above the hull) and real-length guns on the player and enemy tank.
Run after 03 (and 06). Only touches these parts."""
import unreal

eal = unreal.EditorAssetLibrary
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
MAT = "/Game/TankGame/Materials"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")


def mat(name):
    return unreal.load_asset(f"{MAT}/{name}")


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
        raise RuntimeError(f"could not add {name}: {fail}")
    sub.rename_subobject(handle, unreal.Text(name))
    return handles(bp)[name][1]


def mesh(bp, name, parent, shape, material, location, size_cm, rotation=(0.0, 0.0, 0.0)):
    comp = component(bp, name, unreal.StaticMeshComponent, parent)
    comp.set_editor_property("static_mesh", shape)
    comp.set_editor_property("relative_location", unreal.Vector(*location))
    comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(size_cm[0] / 100.0, size_cm[1] / 100.0, size_cm[2] / 100.0))
    comp.set_material(0, material)
    comp.set_collision_profile_name("NoCollision")
    return comp


def move(bp, name, location):
    h = handles(bp).get(name)
    if h:
        h[1].set_editor_property("relative_location", unreal.Vector(*location))


def save(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)


# Player tank. Cockpit tub walls (CockpitRoot space): left y -90, right y 60, back x -90, front x 80, top z 90.
# The armour sits just outside them, from the hull deck (z 40) to just over the wall tops; the front plate stays
# below the dashboard so the view out is the same.
tank = eal.load_asset("/Game/TankGame/Blueprints/Tank/BP_PlayerTank")
hull = mat("MI_Tank_Hull")
# The left wall is also the hull side there, so its armour goes all the way down to the lower hull.
mesh(tank, "CockpitArmorLeft", "CockpitRoot", CUBE, hull, (-5, -95, 46), (182, 6, 92))
mesh(tank, "CockpitArmorRight", "CockpitRoot", CUBE, hull, (-5, 65, 66), (182, 6, 52))
mesh(tank, "CockpitArmorBack", "CockpitRoot", CUBE, hull, (-95, -15, 66), (6, 166, 52))
mesh(tank, "CockpitArmorFront", "CockpitRoot", CUBE, hull, (85, -15, 61), (6, 166, 42))
# Gun: 500 cm from the trunnion, about 2.5 m past the hull front.
mesh(tank, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (255, 0, 0), (16, 16, 500), (90.0, 0.0, 0.0))
move(tank, "MuzzleFlash", (530, 0, 0))
move(tank, "MuzzleFlashLight", (545, 0, 0))
save(tank)

enemy = eal.load_asset("/Game/TankGame/Blueprints/Enemies/BP_Enemy_Tank")
mesh(enemy, "Barrel", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (245, 0, 0), (16, 16, 470), (90.0, 0.0, 0.0))
mesh(enemy, "BarrelEvacuator", "BarrelPivot", CYLINDER, mat("MI_Tank_Barrel"), (270, 0, 0), (24, 24, 40), (90.0, 0.0, 0.0))
move(enemy, "MuzzleFlash", (505, 0, 0))
move(enemy, "Muzzle", (480, 0, 0))
save(enemy)
print("proportions done")
