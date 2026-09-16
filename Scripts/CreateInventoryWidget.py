import unreal

path = '/Game/UI/WBP_Inventory'
asset = unreal.load_asset(path)
if not asset:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/PeaceSign.PSInventoryWidget'))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'WBP_Inventory', '/Game/UI', unreal.WidgetBlueprint, factory)
    if not asset:
        raise RuntimeError('Could not create inventory widget')
unreal.BlueprintEditorLibrary.compile_blueprint(asset)
unreal.EditorAssetLibrary.save_loaded_asset(asset)
controller = unreal.load_asset('/Game/Blueprints/BP_PlayerController')
if not controller:
    raise RuntimeError('Player controller Blueprint missing')
unreal.get_default_object(controller.generated_class()).set_editor_property(
    'inventory_widget_class', asset.generated_class())
unreal.EditorAssetLibrary.save_loaded_asset(controller)
unreal.log('PEACESIGN_INVENTORY_WIDGET_ASSIGNED')
