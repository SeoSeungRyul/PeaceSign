"""Import the temporary bush and enable one grass-only 20x20 prototype patch in TestMap."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'SourceArt/Gathering/Bushes/T_Bush_Temporary_20261105.png'
path = '/Game/Art/Gathering/Bushes/T_Bush_Temporary'
texture = unreal.load_asset(path)
if not texture:
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = '/Game/Art/Gathering/Bushes'
    task.destination_name = 'T_Bush_Temporary'
    task.automated = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(path)
if not isinstance(texture, unreal.Texture2D):
    raise RuntimeError('Bush texture import failed')
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
    raise RuntimeError('Bush texture save failed')
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/TestMap'):
    raise RuntimeError('TestMap load failed')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
grids = [actor for actor in actors if isinstance(actor, unreal.PSGridWorld)]
if len(grids) != 1:
    raise RuntimeError('Expected one grid world')
bushes = grids[0].get_editor_property('bushes')
bushes.set_editor_property('texture', texture)
bushes.set_editor_property('patch_origin', unreal.IntPoint(-10, -10))
bushes.set_editor_property('enabled', True)
if not level.save_current_level():
    raise RuntimeError('TestMap save failed')
unreal.log('BUSH_IMPORT_PASS')
