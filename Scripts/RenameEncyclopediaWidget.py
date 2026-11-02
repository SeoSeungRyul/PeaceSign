"""Rename the existing journal Blueprint through Unreal's asset API, preserving its tree."""
import unreal

old_path = '/Game/UI/Fishing/WBP_FishingJournal'
new_path = '/Game/UI/Encyclopedia/WBP_Encyclopedia'
library = unreal.EditorAssetLibrary
referencers = library.find_package_referencers_for_asset(old_path, True) if library.does_asset_exist(old_path) else []
widget = unreal.load_asset(new_path)
if not widget:
    if not library.does_asset_exist(old_path):
        raise RuntimeError('Existing journal Blueprint is missing')
    library.make_directory('/Game/UI/Encyclopedia')
    if not library.rename_asset(old_path, new_path):
        raise RuntimeError('Could not rename Encyclopedia Blueprint')
    widget = unreal.load_asset(new_path)
if not isinstance(widget, unreal.WidgetBlueprint):
    raise RuntimeError('Renamed asset is not a Widget Blueprint')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if not library.save_loaded_asset(widget):
    raise RuntimeError('Could not save renamed Encyclopedia Blueprint')

controller = unreal.load_asset('/Game/Blueprints/BP_PlayerController')
defaults = unreal.get_default_object(controller.generated_class())
defaults.set_editor_property('fishing_journal_widget_class', widget.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
if not library.save_loaded_asset(controller):
    raise RuntimeError('Could not save Encyclopedia controller binding')
for path in referencers:
    if path == old_path:
        continue
    asset = unreal.load_asset(path)
    if asset and not library.save_loaded_asset(asset):
        raise RuntimeError('Could not save updated referencer: ' + path)
if defaults.get_editor_property('fishing_journal_widget_class') != widget.generated_class():
    raise RuntimeError('Controller does not use the renamed Blueprint')
unreal.log('ENCYCLOPEDIA_WIDGET_RENAMED ' + widget.get_path_name())
