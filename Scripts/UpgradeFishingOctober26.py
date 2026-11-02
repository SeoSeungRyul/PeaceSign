"""Targeted seven-slot migration (user correction overrides the design document), editable journal, and placeholder Icon table. Never rebuilds the fishing tree."""
import unreal

assets = unreal.AssetToolsHelpers.get_asset_tools()
fishing = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not fishing or not unreal.PSFishingWidget.add_fish_panel_prefixes(fishing):
    raise RuntimeError('Could not apply fishing panel names')
if not fishing or not unreal.PSFishingWidget.upgrade_october26_layout(fishing):
    raise RuntimeError('Fishing layout migration failed')
unreal.BlueprintEditorLibrary.compile_blueprint(fishing)
if not unreal.EditorAssetLibrary.save_loaded_asset(fishing):
    raise RuntimeError('Could not save fishing layout')

journal = unreal.load_asset('/Game/UI/Encyclopedia/WBP_Encyclopedia')
if not journal:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/PeaceSign.PSFishingJournalWidget'))
    journal = assets.create_asset('WBP_Encyclopedia', '/Game/UI/Encyclopedia', unreal.WidgetBlueprint, factory)
if not journal or not unreal.PSFishingJournalWidget.initialize_designer_layout(journal):
    raise RuntimeError('Journal layout initialization failed')
unreal.BlueprintEditorLibrary.compile_blueprint(journal)
unreal.EditorAssetLibrary.save_loaded_asset(journal)

# Use the artist's dummy fish as a clearly named shared temporary icon.
# Existing assigned Icon tables and their per-species artwork are preserved.
controller = unreal.load_asset('/Game/Blueprints/BP_PlayerController')
defaults = unreal.get_default_object(controller.generated_class())
if not defaults.get_editor_property('fish_data_table'):
    fish_table = unreal.load_asset('/Game/DataBase/DT_Fish')
    if not fish_table:
        raise RuntimeError('Fish data table missing')
    defaults.set_editor_property('fish_data_table', fish_table)
if not defaults.get_editor_property('fish_icon_data_table'):
    icons = unreal.load_asset('/Game/DataBase/DT_FishIcon')
    if not icons:
        factory = unreal.DataTableFactory()
        factory.set_editor_property('struct', unreal.load_object(None, '/Script/PeaceSign.PSIconDefinition'))
        icons = assets.create_asset('DT_FishIcon', '/Game/DataBase', unreal.DataTable, factory)
        fish = defaults.get_editor_property('fish_data_table')
        ids = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(fish, 'IconID') if fish else ['070201']
        rows = ['---,Name,Image']
        for icon_id in sorted(set(ids)):
            if icon_id and icon_id != 'None':
                rows.append(icon_id + ',임시 공용 물고기,"Texture2D\'/Game/Art/Fish/Icon_DummyFish.Icon_DummyFish\'"')
        if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(icons, '\n'.join(rows) + '\n'):
            raise RuntimeError('Icon table initialization failed')
        unreal.EditorAssetLibrary.save_loaded_asset(icons)
    defaults.set_editor_property('fish_icon_data_table', icons)
defaults.set_editor_property('fishing_journal_widget_class', journal.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
unreal.EditorAssetLibrary.save_loaded_asset(controller)
unreal.log('FISHING_OCT26_UPGRADE_READY')
