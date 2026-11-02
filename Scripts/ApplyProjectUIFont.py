"""Persist the shared game font in all UI Widget Blueprints without changing layout."""
import unreal

font = unreal.load_asset('/Game/UI/Font/NanumFontSetup_TTF_SQUARE_ROUND/NanumSquareRoundR_Font')
if not isinstance(font, unreal.Font):
    raise RuntimeError('The requested NanumSquareRoundR_Font asset is missing')

assets = unreal.EditorAssetLibrary.list_assets('/Game/UI', recursive=True, include_folder=False)
widget_count = 0
text_count = 0
for path in assets:
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    if str(data.asset_class_path.asset_name) != 'WidgetBlueprint':
        continue
    widget = unreal.load_asset(path)
    changed = unreal.PSUIWidget.apply_designer_font(widget)
    if changed < 0:
        raise RuntimeError('Could not apply font: ' + path)
    # Leave already-correct Designer assets untouched, including inventory artwork.
    if changed > 0:
        unreal.BlueprintEditorLibrary.compile_blueprint(widget)
        if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
            raise RuntimeError('Could not save font: ' + path)
    widget_count += 1
    text_count += changed
    unreal.log('PROJECT_UI_FONT asset={} changed={}'.format(path, changed))
if widget_count == 0:
    raise RuntimeError('No Widget Blueprints found in /Game/UI')
unreal.log('PROJECT_UI_FONT_READY widgets={} texts={}'.format(widget_count, text_count))
