"""Adds the "Aim Assist: On/Off" option to the VR template's existing in-game menu (WBP_Menu), before "Real life".
Idempotent: does nothing if the option is already there."""
import unreal

menu = unreal.load_asset("/Game/XRFramework/Blueprints/WBP_Menu")
added = unreal.TankEditorUtils.add_widget_to_widget_blueprint(menu, "VerticalBox_48", unreal.AimAssistToggleWidget, "AimAssistToggle", True)
unreal.EditorAssetLibrary.save_loaded_asset(menu)
print("added", added)
print([w for w in unreal.TankEditorUtils.list_widget_blueprint_widgets(menu) if "Button" in w or "Toggle" in w])
print("status", menu.get_editor_property("status"))
