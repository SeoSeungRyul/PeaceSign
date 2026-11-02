"""User-approved seven-slot / fourth-current layout. Adds only the missing last slot."""
import unreal

widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget or not unreal.PSFishingWidget.add_fish_panel_prefixes(widget):
    raise RuntimeError('Could not resolve fishing panel names')
if not unreal.PSFishingWidget.upgrade_october26_layout(widget):
    raise RuntimeError('Could not add the seventh slot to the existing layout')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
defaults = unreal.get_default_object(widget.generated_class())
if defaults.get_editor_property('visible_direction_slots') != 7 or defaults.get_editor_property('current_direction_slot') != 3:
    raise RuntimeError('Seven-slot defaults did not persist')
if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
    raise RuntimeError('Could not save the seven-slot layout')
unreal.log('FISHING_SEVEN_SLOTS_READY')
