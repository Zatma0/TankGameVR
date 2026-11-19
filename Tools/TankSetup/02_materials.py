"""Placeholder materials: one master (colour, roughness, metallic, emissive) and readable instances per role.
Re-running keeps the existing master and only updates/adds instances.
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
        return eal.load_asset(path)
    m =tools.create_asset("M_TankPlaceholder", DIR, unreal.Material, unreal.MaterialFactoryNew())

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
    # Gun and ammo
    "MI_Ammo_Case":          ((0.80, 0.60, 0.25), 0.35, 0.9, 0.0),
    "MI_Ammo_Tip":           ((0.22, 0.24, 0.20), 0.5, 0.4, 0.0),
    "MI_Ammo_Rack":          ((0.12, 0.13, 0.12), 0.7, 0.3, 0.0),
    "MI_Gun_Breech":         ((0.26, 0.28, 0.25), 0.45, 0.7, 0.0),
    "MI_Gun_ReadyLamp":      ((0.06, 0.30, 0.09), 0.3, 0.0, 0.0),
    "MI_Tank_Tracer":        ((1.00, 0.65, 0.30), 0.3, 0.0, 30.0),
    "MI_Tank_MuzzleFlash":   ((1.00, 0.75, 0.40), 0.3, 0.0, 60.0),
    # Cigarette
    "MI_Cigarette_Paper":    ((0.95, 0.94, 0.90), 0.8, 0.0, 0.0),
    "MI_Cigarette_Filter":   ((0.85, 0.55, 0.25), 0.8, 0.0, 0.0),
    "MI_Cigarette_Ember":    ((1.00, 0.35, 0.05), 0.6, 0.0, 1.0),
    "MI_Cigarette_Smoke":    ((0.80, 0.80, 0.80), 1.0, 0.0, 0.25),
    "MI_Ashtray":            ((0.35, 0.36, 0.38), 0.3, 0.8, 0.0),
    # AP: black dart tip, HE: olive with a yellow band
    "MI_Ammo_TipAP":         ((0.07, 0.07, 0.08), 0.4, 0.6, 0.0),
    "MI_Ammo_TipHE":         ((0.30, 0.33, 0.18), 0.6, 0.2, 0.0),
    "MI_Ammo_BandHE":        ((0.95, 0.78, 0.10), 0.5, 0.0, 0.0),
    # Extinguisher and smoke switch
    "MI_Extinguisher":       ((0.75, 0.06, 0.04), 0.35, 0.1, 0.0),
    "MI_Extinguisher_Metal": ((0.10, 0.10, 0.10), 0.4, 0.8, 0.0),
    "MI_Smoke_Cover":        ((0.85, 0.10, 0.06), 0.3, 0.0, 0.3),
    "MI_Smoke_Button":       ((0.90, 0.85, 0.80), 0.4, 0.0, 0.0),
    "MI_Smoke_Lamp":         ((0.20, 1.00, 0.30), 0.3, 0.0, 0.0),
    # Enemies
    "MI_Enemy_Car":          ((0.42, 0.33, 0.22), 0.7, 0.1, 0.0),
    "MI_Enemy_ATGun":        ((0.30, 0.31, 0.24), 0.6, 0.3, 0.0),
    "MI_Enemy_Crew":         ((0.28, 0.30, 0.20), 0.9, 0.0, 0.0),
    "MI_Enemy_Skin":         ((0.80, 0.62, 0.48), 0.8, 0.0, 0.0),
    "MI_Enemy_Wreck":        ((0.06, 0.05, 0.05), 0.95, 0.2, 0.0),
    "MI_Enemy_Tracer":       ((1.00, 0.30, 0.15), 0.3, 0.0, 40.0),
    "MI_Env_Wall":           ((0.50, 0.46, 0.40), 0.95, 0.0, 0.0),
    "MI_Env_Rock":           ((0.38, 0.37, 0.35), 0.9, 0.0, 0.0),
}


def build_translucent(name, additive):
    """Soft unlit see-through material for the sphere puffs: smoke (translucent) or a glow (additive, flames and
    laser). Thin at the silhouette and fades where it meets the ground, so the spheres don't read as balls.
    Rebuilt every run."""
    path = f"{DIR}/{name}"
    m = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", False)

    def param(cls, pname, value, x, y):
        e = mel.create_material_expression(m, cls, x, y)
        e.set_editor_property("parameter_name", pname)
        e.set_editor_property("default_value", value)
        return e

    color = param(unreal.MaterialExpressionVectorParameter, "Color", linear(0.5, 0.5, 0.5), -900, -200)
    strength = param(unreal.MaterialExpressionScalarParameter, "EmissiveStrength", 1.0, -900, -50)
    opacity = param(unreal.MaterialExpressionScalarParameter, "Opacity", 0.8, -900, 100)
    fresnel = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -900, 250)
    fresnel.set_editor_property("exponent", 1.5)
    edge = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, -700, 250)
    mel.connect_material_expressions(fresnel, "", edge, "")
    # How far from other surfaces it fades out: wide for the smoke screen, almost none for small flames.
    soft_edge = param(unreal.MaterialExpressionScalarParameter, "SoftEdge", 100.0, -900, 400)
    soft = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -700, 400)
    mel.connect_material_expressions(soft_edge, "", soft, "FadeDistance")
    edge_fade = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 150)
    mel.connect_material_expressions(opacity, "", edge_fade, "A")
    mel.connect_material_expressions(edge, "", edge_fade, "B")
    fade = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 250)
    mel.connect_material_expressions(edge_fade, "", fade, "A")
    mel.connect_material_expressions(soft, "", fade, "B")

    glow = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, -150)
    mel.connect_material_expressions(color, "", glow, "A")
    mel.connect_material_expressions(strength, "", glow, "B")
    if additive:
        # Additive: brightness carries the fade.
        final = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -100, 0)
        mel.connect_material_expressions(glow, "", final, "A")
        mel.connect_material_expressions(fade, "", final, "B")
        mel.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


# name: (master, sRGB colour, opacity, emissive, soft edge cm)
SOFT = {
    "MI_Smoke_Screen":       ("M_TankSmoke", (0.74, 0.74, 0.72), 0.85, 1.0, 150.0),
    "MI_Fire_Smoke":         ("M_TankSmoke", (0.10, 0.09, 0.08), 0.75, 1.0, 25.0),
    "MI_Extinguisher_Spray": ("M_TankSmoke", (0.93, 0.94, 0.96), 0.5, 1.0, 6.0),
    # Flames are translucent, not additive, so they still read against the bright cockpit walls.
    "MI_Fire_Flame":         ("M_TankSmoke", (1.00, 0.40, 0.06), 0.92, 3.0, 2.0),
    "MI_Enemy_Laser":        ("M_TankGlow", (1.00, 0.08, 0.05), 1.0, 25.0, 1.0),
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

soft_masters = {"M_TankSmoke": build_translucent("M_TankSmoke", False), "M_TankGlow": build_translucent("M_TankGlow", True)}
for name, (parent, color, opacity, emissive, soft_edge) in SOFT.items():
    path = f"{DIR}/{name}"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, soft_masters[parent])
    mel.set_material_instance_vector_parameter_value(mi, "Color", linear(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Opacity", opacity)
    mel.set_material_instance_scalar_parameter_value(mi, "SoftEdge", soft_edge)
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", emissive)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
print("materials", len(INSTANCES) + len(SOFT) + 3)
