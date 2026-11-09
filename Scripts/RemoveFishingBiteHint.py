"""Remove only the bite input hint; preserve the existing fishing art and layout."""
import unreal

widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget or not unreal.PSFishingWidget.remove_bite_input_hint(widget):
    raise RuntimeError('Could not remove the fishing bite input hint')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
    raise RuntimeError('Could not save the fishing bite input hint removal')
unreal.log('FISHING_BITE_HINT_REMOVED')
