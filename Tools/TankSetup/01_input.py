"""Tank input: IA_Tank_Drive / IA_Tank_Aim (Axis2D) and IMC_Tank.

IMC_Tank is added above the VR template's IMC_Default while seated. It maps the *same* thumbstick axis keys the
template uses for teleport (right stick Y) and snap turn (left stick X), so those keys drive the tank instead.
Left stick = drive (steering joystick), right stick = aim. Keyboard/gamepad mappings are for desktop testing.
"""
import unreal

DIR = "/Game/TankGame/Input"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def get_or_create(name, cls, factory):
    path = f"{DIR}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    return tools.create_asset(name, DIR, cls, factory)


def make_action(name):
    action = get_or_create(name, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    eal.save_loaded_asset(action)
    return action


drive = make_action("IA_Tank_Drive")
aim = make_action("IA_Tank_Aim")

imc = get_or_create("IMC_Tank", unreal.InputMappingContext, unreal.InputMappingContext_Factory())
imc.unmap_all()


MODIFIERS = {}


def add(action, key, swizzle=False, negate=False):
    input_key = unreal.Key()
    input_key.set_editor_property("key_name", key)
    imc.map_key(action, input_key)
    MODIFIERS[(action.get_name(), key)] = (swizzle, negate)


def apply_modifiers():
    """Modifiers must be real subobjects of the asset to be saved; a C++ editor helper creates them."""
    for (action_name, key), (swizzle, negate) in MODIFIERS.items():
        if swizzle or negate:
            unreal.TankEditorUtils.set_key_mapping_modifiers(imc, key, swizzle, negate)


# VR controllers: X axis -> X, Y axis -> Y (swizzled into the second component).
for vendor, stick in [("OculusTouch", "Thumbstick"), ("ValveIndex", "Thumbstick"), ("PICO", "Thumbstick"), ("Vive", "Trackpad")]:
    add(drive, f"{vendor}_Left_{stick}_X")
    add(drive, f"{vendor}_Left_{stick}_Y", swizzle=True)
    add(aim, f"{vendor}_Right_{stick}_X")
    add(aim, f"{vendor}_Right_{stick}_Y", swizzle=True)

# Gamepad.
add(drive, "Gamepad_LeftX")
add(drive, "Gamepad_LeftY", swizzle=True)
add(aim, "Gamepad_RightX")
add(aim, "Gamepad_RightY", swizzle=True)

# Keyboard (desktop testing): WASD drive, arrow keys aim.
add(drive, "D")
add(drive, "A", negate=True)
add(drive, "W", swizzle=True)
add(drive, "S", swizzle=True, negate=True)
add(aim, "Right")
add(aim, "Left", negate=True)
add(aim, "Up", swizzle=True)
add(aim, "Down", swizzle=True, negate=True)

apply_modifiers()
eal.save_loaded_asset(imc)

summary = {}
for mapping in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"):
    action = mapping.get_editor_property("action").get_name()
    key = str(mapping.get_editor_property("key").get_editor_property("key_name"))
    mods = [m.get_class().get_name().replace("InputModifier", "") for m in mapping.get_editor_property("modifiers")]
    summary.setdefault(action, []).append(key + (f"[{','.join(mods)}]" if mods else ""))
for action, keys in summary.items():
    print(action, len(keys), keys)
