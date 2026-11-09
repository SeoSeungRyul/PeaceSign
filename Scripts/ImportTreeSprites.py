"""Import 256px tree textures, create standing/stump sprites and author a grove below spawn.

Run after building PeaceSignEditor. Existing terrain and saved felled trees are
preserved by runtime placement. Reruns preserve authored sprite assets.
"""
import json
import math
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'SourceArt/Gathering/TreeSprites/256'
species = [('Oak', unreal.PSTreeSpecies.OAK), ('Maple', unreal.PSTreeSpecies.MAPLE),
           ('Cherry', unreal.PSTreeSpecies.CHERRY), ('Palm', unreal.PSTreeSpecies.PALM),
           ('Cactus', unreal.PSTreeSpecies.CACTUS), ('Birch', unreal.PSTreeSpecies.BIRCH)]
tools = unreal.AssetToolsHelpers.get_asset_tools()
standing, stumps = {}, {}
for name, kind in species:
    filename = source / f'T_Tree_{name}.png'
    if not filename.is_file():
        raise RuntimeError('Missing source ' + str(filename))
    texture_path = f'/Game/Art/Gathering/Trees/T_Tree_{name}'
    texture = unreal.load_asset(texture_path)
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(filename))
        task.set_editor_property('destination_path', '/Game/Art/Gathering/Trees')
        task.set_editor_property('destination_name', f'T_Tree_{name}')
        task.set_editor_property('automated', True)
        task.set_editor_property('save', True)
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(texture_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Tree texture import failed')
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError('Could not save texture')
    for is_stump in (False, True):
        sprite = unreal.PSGatheringTreeActor.create_tree_sprite_asset(
            texture, f'/Game/Art/Gathering/Trees/S_{"Stump" if is_stump else "Tree"}_{name}', is_stump)
        if not sprite or not unreal.EditorAssetLibrary.save_loaded_asset(sprite):
            raise RuntimeError(f'Could not create/save sprite {name}, stump={is_stump}')
        (stumps if is_stump else standing)[kind] = sprite

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/TestMap'):
    raise RuntimeError('Could not load TestMap')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
grids = [a for a in actors if isinstance(a, unreal.PSGridWorld)]
if len(grids) != 1:
    raise RuntimeError('Expected one GridWorld')
grid = grids[0]
starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
pawns = [a for a in actors if isinstance(a, unreal.PSPlayerCharacter)]
anchor = pawns[0] if pawns else (starts[0] if starts else None)
if not anchor:
    raise RuntimeError('No player spawn anchor found')
spawn = anchor.get_actor_location()
origin = grid.get_actor_location()
cell_size = grid.get_editor_property('cell_size')
spawn_cell = (math.floor((spawn.x-origin.x)/cell_size), math.floor((spawn.y-origin.y)/cell_size))
gathering = grid.get_editor_property('gathering')
gathering.set_editor_property('tree_sprites', standing)
gathering.set_editor_property('stump_sprites', stumps)
gathering.set_editor_property('sprite_height_in_cells', 3.2)
placed = list(gathering.get_editor_property('placed_trees'))
existing_ids = {str(p.get_editor_property('placement_id')) for p in placed}
for i, (name, kind) in enumerate(species):
    placement_id = 'Showcase.' + name
    if placement_id in existing_ids:
        continue
    record = unreal.PSTreeRecord()
    record.set_editor_property('cell', unreal.IntPoint(spawn_cell[0]-6-(i//3)*5, spawn_cell[1]+(-6,-2,3)[i%3]))
    record.set_editor_property('species', kind)
    record.set_editor_property('placement_id', placement_id)
    placed.append(record)
gathering.set_editor_property('placed_trees', placed)
if not level.save_current_level():
    raise RuntimeError('Could not save placed tree definitions')
report = {'spawn_cell': spawn_cell, 'standing': [s.get_path_name() for s in standing.values()],
          'placements': [{'id': str(p.get_editor_property('placement_id')),
                          'x': p.get_editor_property('cell').x, 'y': p.get_editor_property('cell').y} for p in placed]}
(root / '.codex-tmp/tree-sprites-import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('TREE_SPRITES_READY ' + json.dumps(report))
