"""Apply the approved Encyclopedia book design once; normal game/editor startup preserves it.
Rerunning replaces only the journal layout and the inventory's three tab images.
"""
from pathlib import Path
import json
import unreal

source = Path(r"C:/Users/main/Downloads/도감, 인벤토리")
assets = unreal.AssetToolsHelpers.get_asset_tools()
for folder in ("Book", "Encyclopedia"):
    for image in sorted((source / folder).rglob("*.png")):
        relative = image.relative_to(source).with_suffix("")
        destination = "/Game/UI/" + str(relative).replace("\\", "/")
        texture = unreal.load_asset(destination)
        if not texture:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(image))
            task.set_editor_property("destination_path", destination.rsplit("/", 1)[0])
            task.set_editor_property("destination_name", image.stem)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            assets.import_asset_tasks([task])
            texture = unreal.load_asset(destination)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("Missing UI texture: " + destination)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("srgb", True)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError("Could not save texture: " + destination)

journal = unreal.load_asset("/Game/UI/Encyclopedia/WBP_Encyclopedia")
if not journal:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/PeaceSign.PSFishingJournalWidget"))
    journal = assets.create_asset("WBP_Encyclopedia", "/Game/UI/Encyclopedia", unreal.WidgetBlueprint, factory)
if not unreal.PSFishingJournalWidget.apply_book_designer_layout(journal):
    raise RuntimeError("Could not apply Encyclopedia layout")
unreal.BlueprintEditorLibrary.compile_blueprint(journal)
if not unreal.EditorAssetLibrary.save_loaded_asset(journal):
    raise RuntimeError("Could not save journal")

inventory = unreal.load_asset("/Game/UI/Inventory/WBP_Inventory")
if not unreal.PSInventoryWidget.apply_book_tab_artwork(inventory):
    raise RuntimeError("Could not update inventory tab artwork")
unreal.BlueprintEditorLibrary.compile_blueprint(inventory)
if not unreal.EditorAssetLibrary.save_loaded_asset(inventory):
    raise RuntimeError("Could not save inventory")

controller = unreal.load_asset("/Game/Blueprints/BP_PlayerController")
defaults = unreal.get_default_object(controller.generated_class())
fish = defaults.get_editor_property("fish_data_table")
icons = defaults.get_editor_property("fish_icon_data_table")
if not fish or not icons:
    raise RuntimeError("Existing fish database or icon table is not assigned")
defaults.set_editor_property("fishing_journal_widget_class", journal.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
if not unreal.EditorAssetLibrary.save_loaded_asset(controller):
    raise RuntimeError("Could not save controller")
report = {
    "fish_table": fish.get_path_name(),
    "fish_count": len(unreal.DataTableFunctionLibrary.get_data_table_row_names(fish)),
    "fish_json": unreal.DataTableFunctionLibrary.export_data_table_to_json_string(fish),
    "icon_table": icons.get_path_name(),
}
output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "BookJournal"
output.mkdir(parents=True, exist_ok=True)
(output / "Database.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("BOOK_JOURNAL_LAYOUT_READY " + str(report["fish_count"]))
