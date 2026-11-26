import unreal, os
# Run in the editor (Tools > Execute Python Script) after build.py.
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "Sound", "out")
DEST = "/Game/TankGame/Audio"
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
    raise RuntimeError("PIE is running")
tasks = []
for f in sorted(os.listdir(SRC)):
    if f.endswith(".wav"):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(SRC, f))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
n = 0
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".wav"):
        continue
    name = f[:-4]
    asset = unreal.load_asset(DEST + "/" + name)
    if not asset:
        print("MISSING", name)
        continue
    asset.set_editor_property("looping", name.endswith("_Loop"))
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    n += 1
print("imported", n, "sound waves into", DEST)
