"""Create editable fishing assets once; reruns preserve existing designer and table edits."""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/PeaceSign.PSFishingWidget'))
    widget = asset_tools.create_asset('WBP_Fishing', '/Game/UI/Fishing', unreal.WidgetBlueprint, factory)
if not widget or not unreal.PSFishingWidget.add_fish_panel_prefixes(widget):
    raise RuntimeError('Could not apply fishing panel names')
if not widget or not unreal.PSFishingWidget.initialize_designer_layout(widget):
    raise RuntimeError('Could not initialize fishing Widget Blueprint')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
unreal.EditorAssetLibrary.save_loaded_asset(widget)

table = unreal.load_asset('/Game/DataBase/DT_FishingCast')
if not table:
    factory = unreal.DataTableFactory()
    factory.set_editor_property('struct', unreal.load_object(None, '/Script/PeaceSign.PSFishingCastDefinition'))
    table = asset_tools.create_asset('DT_FishingCast', '/Game/DataBase', unreal.DataTable, factory)
    if not table or not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(
            table, '---,DistanceInCells,DifficultyWeights\nGreen,2,"(55,30,13,2)"\nYellow,4,"(50,30,15,5)"\nRed,6,"(45,30,17,8)"\n'):
        raise RuntimeError('Could not initialize casting table')
    unreal.EditorAssetLibrary.save_loaded_asset(table)
controller = unreal.load_asset('/Game/Blueprints/BP_PlayerController')
if not controller:
    raise RuntimeError('Player controller Blueprint missing')
defaults = unreal.get_default_object(controller.generated_class())
defaults.set_editor_property('fishing_widget_class', widget.generated_class())
defaults.set_editor_property('fishing_cast_data_table', table)
# Migrate the previously serialized 4-second prototype limit to the approved 10-second limit.
if defaults.get_editor_property('max_bite_delay') == 4.0:
    defaults.set_editor_property('max_bite_delay', 10.0)
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
unreal.EditorAssetLibrary.save_loaded_asset(controller)
unreal.log('PEACESIGN_FISHING_ASSETS_READY')
