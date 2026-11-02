"""Assign the artist's temporary fish image to every IconID currently used by Fish.

Explicit authoring utility: reruns replace fish icon images, preserving other icon rows
and all fish definitions. For individual species art later, edit the Icon table instead.
"""
import csv
import io
import json
from pathlib import Path
import unreal

texture = unreal.load_asset('/Game/Art/Fish/Icon_DummyFish')
if not isinstance(texture, unreal.Texture2D):
    raise RuntimeError('Missing Texture2D /Game/Art/Fish/Icon_DummyFish')
controller = unreal.load_asset('/Game/Blueprints/BP_PlayerController')
defaults = unreal.get_default_object(controller.generated_class())
fish = defaults.get_editor_property('fish_data_table')
icons = defaults.get_editor_property('fish_icon_data_table')
if not fish or fish.get_editor_property('row_struct').get_name() != 'PSFishDefinition':
    raise RuntimeError('The player controller must reference a Fish definition table')
if not icons or icons.get_editor_property('row_struct').get_name() != 'PSIconDefinition':
    raise RuntimeError('The player controller must reference an Icon definition table')

fish_before = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(fish)
fish_rows = list(csv.DictReader(io.StringIO(fish_before)))
if not fish_rows or any(row['IconID'] in ('', 'None') for row in fish_rows):
    raise RuntimeError('Every fish must have an IconID before assigning its image')
used_ids = {row['IconID'] for row in fish_rows}
icons_before = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(icons)
reader = csv.DictReader(io.StringIO(icons_before))
fields = reader.fieldnames
if not fields or fields[0] != '---' or 'Image' not in fields:
    raise RuntimeError('Unexpected Icon table columns')
icon_rows = list(reader)
by_id = {row['---']: row for row in icon_rows}
for icon_id in sorted(used_ids):
    if icon_id not in by_id:
        row = {field: '' for field in fields}
        row.update({'---': icon_id, 'Name': '임시 공용 물고기'})
        icon_rows.append(row)
        by_id[icon_id] = row
    by_id[icon_id]['Image'] = texture.get_path_name()
output = io.StringIO()
writer = csv.DictWriter(output, fieldnames=fields, lineterminator='\n')
writer.writeheader()
writer.writerows(icon_rows)
if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(icons, output.getvalue()):
    unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(icons, icons_before)
    raise RuntimeError('Icon update failed; previous rows restored')

actual = list(csv.DictReader(io.StringIO(unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(icons))))
actual_by_id = {row['---']: row for row in actual}
if any(actual_by_id[icon_id]['Image'] != texture.get_path_name() for icon_id in used_ids):
    raise RuntimeError('Some fish did not retain the dummy image')
if unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(fish) != fish_before:
    raise RuntimeError('Fish definitions unexpectedly changed')

texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
for asset in (texture, icons):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + asset.get_path_name())
report = {'fish_count': len(fish_rows), 'icon_ids': sorted(used_ids),
          'texture': texture.get_path_name(), 'icon_table': icons.get_path_name()}
report_path = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / '.codex-tmp' / 'dummy-fish-applied.json'
report_path.parent.mkdir(parents=True, exist_ok=True)
with report_path.open('w', encoding='utf-8') as out:
    json.dump(report, out, ensure_ascii=False, indent=2)
unreal.log('DUMMY_FISH_APPLIED ' + json.dumps(report))
