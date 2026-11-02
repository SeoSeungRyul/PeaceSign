"""One-time imported-art layout for WBP_Fishing. Rerunning replaces Designer layout.

Normal initialization and CreateFishingWidget.py preserve subsequent Designer edits.
"""
import unreal

assets = unreal.EditorAssetLibrary.list_assets('/Game/UI/Fishing', recursive=True, include_folder=False)
for path in assets:
    if '/Fish_' not in path:
        continue
    texture = unreal.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        continue
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget or not unreal.PSFishingWidget.apply_artwork_designer_layout(widget):
    raise RuntimeError('Missing fishing artwork or invalid Widget Blueprint; layout was not applied')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
    raise RuntimeError('Could not save the fishing artwork layout')
defaults = unreal.get_default_object(widget.generated_class())
brush = defaults.get_editor_property('current_arrow_brushes').get_editor_property('up')
texture = brush.get_editor_property('resource_object')
if not texture or '/Arrow_Now/' not in texture.get_path_name():
    raise RuntimeError('The Widget Blueprint did not retain its artwork defaults')
unreal.log('FISHING_ARTWORK_LAYOUT_READY ' + texture.get_path_name())
