"""Placeholder materials: one master (colour, roughness, metallic, emissive) and readable instances per role.
Teammates replace these with final materials by reassigning slots on the Blueprint meshes; no code references them."""
import unreal

DIR = "/Game/TankGame/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def linear(r, g, b):
    return unreal.LinearColor(r ** 2.2, g ** 2.2, b ** 2.2, 1.0)


def build_master():
    path = f"{DIR}/M_TankPlaceholder"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_TankPlaceholder", DIR, unreal.Material, unreal.MaterialFactoryNew())

    def vec(name, value, x, y):
        e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    def scalar(name, value, x, y):
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    color = vec("Color", linear(0.5, 0.5, 0.5), -500, -200)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar("Roughness", 0.6, -500, 0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Metallic", 0.0, -500, 100), "", unreal.MaterialProperty.MP_METALLIC)
    emissive = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 250)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(scalar("EmissiveStrength", 0.0, -500, 300), "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


# name: (sRGB colour, roughness, metallic, emissive strength)
INSTANCES = {
    "MI_Tank_Hull":          ((0.33, 0.38, 0.22), 0.7, 0.1, 0.0),
    "MI_Tank_HullDark":      ((0.20, 0.23, 0.14), 0.75, 0.1, 0.0),
    "MI_Tank_Tracks":        ((0.10, 0.10, 0.10), 0.9, 0.2, 0.0),
    "MI_Tank_Barrel":        ((0.18, 0.20, 0.16), 0.5, 0.6, 0.0),
    "MI_Cockpit_Interior":   ((0.42, 0.45, 0.47), 0.8, 0.0, 0.0),
    "MI_Cockpit_Panel":      ((0.16, 0.17, 0.19), 0.6, 0.2, 0.0),
    "MI_Cockpit_Seat":       ((0.20, 0.13, 0.09), 0.85, 0.0, 0.0),
    "MI_Control_Stick":      ((0.06, 0.06, 0.06), 0.5, 0.3, 0.0),
    "MI_Control_DriveKnob":  ((0.15, 0.55, 1.00), 0.4, 0.0, 0.6),
    "MI_Control_AimKnob":    ((1.00, 0.45, 0.10), 0.4, 0.0, 0.6),
    "MI_Display_Frame":      ((0.03, 0.03, 0.03), 0.4, 0.4, 0.0),
    "MI_Enemy_Hull":         ((0.45, 0.16, 0.12), 0.7, 0.1, 0.0),
    "MI_Enemy_Turret":       ((0.30, 0.10, 0.08), 0.65, 0.2, 0.0),
    "MI_Env_Ground":         ((0.42, 0.40, 0.33), 0.95, 0.0, 0.0),
    "MI_Env_Obstacle":       ((0.55, 0.55, 0.52), 0.9, 0.0, 0.0),
}

master = build_master()
for name, (color, rough, metal, emissive) in INSTANCES.items():
    path = f"{DIR}/{name}"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_vector_parameter_value(mi, "Color", linear(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", emissive)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
print("materials", len(INSTANCES) + 1)
