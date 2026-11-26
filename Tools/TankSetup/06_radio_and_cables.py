"""Radio set + handset and the hanging cockpit cables in BP_PlayerTank, their two materials and the radio key.
Run after 01-05 (Tools > Execute Python Script). Only adds/updates these parts; the rest of the tank is untouched."""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary

MAT = "/Game/TankGame/Materials"
INPUT = "/Game/TankGame/Input"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
GRAB_CLASS = unreal.load_asset("/Game/XRFramework/Blueprints/BP_GrabComponent").generated_class()

# Same master and parameters as 02_materials.py.
for name, color, rough, metal in (("MI_Radio", (0.16, 0.18, 0.11), 0.75, 0.1), ("MI_Cockpit_Cable", (0.02, 0.02, 0.02), 0.6, 0.0)):
    path = f"{MAT}/{name}"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(f"{MAT}/M_TankPlaceholder"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*(c ** 2.2 for c in color), 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)


def mat(name):
    return unreal.load_asset(f"{MAT}/{name}")


def handles(bp):
    result = {}
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = sub.k2_find_subobject_data_from_handle(handle)
        obj = SDL.get_associated_object(data)
        if obj:
            result[str(SDL.get_variable_name(data))] = (handle, obj)
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


def cable(bp, name, start, end, length, width):
    """A hanging cable between two cockpit points (CockpitRoot space). Both ends are fixed; the sim makes it sag and
    swing. The tank's CableJolt ties the end to CockpitRoot at play time."""
    comp = component(bp, name, unreal.CableComponent, "CockpitRoot")
    comp.set_editor_property("relative_location", unreal.Vector(*start))
    comp.set_editor_property("end_location", unreal.Vector(*end))
    comp.set_editor_property("cable_length", length)
    comp.set_editor_property("num_segments", 10)
    comp.set_editor_property("solver_iterations", 6)
    comp.set_editor_property("enable_stiffness", True)
    comp.set_editor_property("cable_width", width)
    comp.set_editor_property("num_sides", 6)
    comp.set_material(0, mat("MI_Cockpit_Cable"))
    comp.set_collision_profile_name("NoCollision")
    return comp


bp = eal.load_asset("/Game/TankGame/Blueprints/Tank/BP_PlayerTank")

# Radio set on the right wall, handset (component "Radio", pivot = its middle, earpiece up) hanging on the hook.
mesh(bp, "RadioSet", "CockpitRoot", CUBE, mat("MI_Radio"), (20, 56, 76), (18, 8, 16))
mesh(bp, "RadioDial", "CockpitRoot", CYLINDER, mat("MI_Extinguisher_Metal"), (14, 51.5, 80), (3, 3, 1.5), (90.0, 0.0, 90.0))
mesh(bp, "RadioLamp", "CockpitRoot", CUBE, mat("MI_Smoke_Lamp"), (26, 51.6, 81), (1.2, 0.6, 1.2))
mesh(bp, "RadioHook", "CockpitRoot", CUBE, mat("MI_Extinguisher_Metal"), (20, 51, 90), (3, 3, 2))
mesh(bp, "RadioHandsetGrip", "Radio", CUBE, mat("MI_Radio"), (0, 0, 0), (4, 4.5, 15))
mesh(bp, "RadioHandsetEar", "Radio", CYLINDER, mat("MI_Radio"), (0, -1.5, 8), (6, 6, 3), (0.0, 0.0, 90.0))
mesh(bp, "RadioHandsetMouth", "Radio", CYLINDER, mat("MI_Radio"), (0, -1.5, -8), (5, 5, 3), (0.0, 0.0, 90.0))
mesh(bp, "RadioHandsetKey", "Radio", CUBE, mat("MI_Extinguisher_Metal"), (2.4, 0, 1), (1, 2, 4))
grab = component(bp, "RadioGrab", GRAB_CLASS, "RadioHandsetGrip")
grab.set_editor_property("GrabType", type(grab.get_editor_property("GrabType")).CUSTOM)
radio_cable = handles(bp).get("RadioCable")
if radio_cable:
    radio_cable[1].set_material(0, mat("MI_Cockpit_Cable"))

# Cables that just hang and swing: a loom along the right wall, a lead under the dashboard, one over the rack.
# Slightly longer than the span: they sag about a hand's width and sway, but stay along their wall.
cable(bp, "CableLoomRight", (-30, 57, 72), (60, 57, 64), 92, 2.4)
cable(bp, "CableDash", (72, -80, 74), (72, -30, 76), 51.5, 1.6)
cable(bp, "CableLeftWall", (-60, -86, 86), (10, -86, 82), 71.5, 1.8)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp)

# Radio key: Q / D-pad right (VR uses the handset).
path = f"{INPUT}/IA_Tank_Radio"
action = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset("IA_Tank_Radio", INPUT, unreal.InputAction, unreal.InputAction_Factory())
action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
eal.save_loaded_asset(action)
imc = eal.load_asset(f"{INPUT}/IMC_Tank")
imc.modify()
mapped = {(m.get_editor_property("action").get_name(), str(m.get_editor_property("key").get_editor_property("key_name")))
          for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")}
for key in ("Q", "Gamepad_DPad_Right"):
    if ("IA_Tank_Radio", key) not in mapped:
        k = unreal.Key()
        k.set_editor_property("key_name", key)
        imc.map_key(action, k)
eal.save_loaded_asset(imc, False)

cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("radio_action", action)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp)
print("radio and cables:", sorted(k for k in handles(bp) if k.startswith(("Radio", "Cable"))))
