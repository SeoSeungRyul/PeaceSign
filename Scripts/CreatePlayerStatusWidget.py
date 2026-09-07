import unreal
path = '/Game/UI/WBP_PlayerStatus'
asset = unreal.load_asset(path)
if not asset:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/PeaceSign.PSPlayerStatusWidget'))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('WBP_PlayerStatus', '/Game/UI', unreal.WidgetBlueprint, factory)
    if not asset:
        raise RuntimeError('Could not create player status widget')
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log('PEACESIGN_STATUS_WIDGET_CREATED')

controller = unreal.load_asset('/Game/Blueprints/BP_PlayerController')
if not controller:
    raise RuntimeError('Player controller Blueprint missing')
defaults = unreal.get_default_object(controller.generated_class())
defaults.set_editor_property('player_status_widget_class', asset.generated_class())
unreal.EditorAssetLibrary.save_loaded_asset(controller)
unreal.log('PEACESIGN_STATUS_WIDGET_ASSIGNED')
