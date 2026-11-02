"""Explicit one-time Fish prefix migration. Keeps the existing tree, artwork and slot layout."""
import unreal

widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget or not unreal.PSFishingWidget.add_fish_panel_prefixes(widget):
    raise RuntimeError('Fishing panel rename failed; check for conflicting names')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
    raise RuntimeError('Could not save renamed fishing panels')
unreal.log('FISHING_PANEL_NAMES_READY')
