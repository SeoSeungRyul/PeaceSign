"""Apply requested feedback placement without rebuilding the user's fishing UI."""
import unreal

widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget or not unreal.PSFishingWidget.update_feedback_layout(widget):
    raise RuntimeError('Could not migrate fishing feedback layout')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
    raise RuntimeError('Could not save fishing feedback layout')
unreal.log('FISHING_FEEDBACK_LAYOUT_READY')
