"""Import the twelve inventory PNGs and link Item.IconID -> DT_GatheringIcon.

Existing textures and icon rows are preserved. Only the twelve Item.IconID values
are updated; names, prices, drop tables and the fish icon table are untouched.
Run in Unreal Editor Python after compiling PeaceSignEditor.
"""
import csv
import io
import json
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'SourceArt' / 'Gathering' / 'InventoryIcons-256'
files = sorted(source.glob('03*.png'))
expected = {f'030{kind}{species:02d}' for kind in (1, 2) for species in range(1, 7)}
if len(files) != 12 or {p.stem.split('_', 1)[0] for p in files} != expected:
    raise RuntimeError('Expected exactly six logs and six tree seeds')

items = unreal.load_asset('/Game/DataBase/DT_Item')
if not items or items.get_editor_property('row_struct').get_name() != 'PSItemTableRow':
    raise RuntimeError('Missing DT_Item / PSItemTableRow')
before = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(items)
reader = csv.DictReader(io.StringIO(before))
fields = reader.fieldnames
rows = list(reader)
by_id = {row['---']: row for row in rows}
if not expected.issubset(by_id):
    raise RuntimeError('The twelve item rows must exist before importing icons')

tools = unreal.AssetToolsHelpers.get_asset_tools()
textures = {}
for path in files:
    asset_name = 'T_' + path.stem
    asset_path = '/Game/Art/Gathering/Inventory/' + asset_name
    texture = unreal.load_asset(asset_path)
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(path))
        task.set_editor_property('destination_path', '/Game/Art/Gathering/Inventory')
        task.set_editor_property('destination_name', asset_name)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', False)
        task.set_editor_property('save', True)
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Could not import Texture2D ' + asset_path)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError('Could not save ' + asset_path)
    textures[path.stem.split('_', 1)[0]] = texture

icons = unreal.load_asset('/Game/DataBase/DT_GatheringIcon')
if not icons:
    factory = unreal.DataTableFactory()
    factory.set_editor_property('struct', unreal.load_object(None, '/Script/PeaceSign.PSIconDefinition'))
    icons = tools.create_asset('DT_GatheringIcon', '/Game/DataBase', unreal.DataTable, factory)
if not icons or icons.get_editor_property('row_struct').get_name() != 'PSIconDefinition':
    raise RuntimeError('DT_GatheringIcon must use PSIconDefinition')
icon_before = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(icons)
icon_reader = csv.DictReader(io.StringIO(icon_before))
icon_fields = icon_reader.fieldnames
icon_rows = list(icon_reader)
icon_by_id = {row['---']: row for row in icon_rows}
for item_id, texture in textures.items():
    if item_id not in icon_by_id:
        row = {field: '' for field in icon_fields}
        row.update({'---': item_id, 'Name': by_id[item_id]['Name'], 'Image': texture.get_path_name()})
        icon_rows.append(row)
        icon_by_id[item_id] = row
    by_id[item_id]['IconID'] = item_id


def replace_rows(table, field_names, data, original):
    out = io.StringIO()
    writer = csv.DictWriter(out, fieldnames=field_names, lineterminator='\n')
    writer.writeheader()
    writer.writerows(data)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, out.getvalue()):
        unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, original)
        raise RuntimeError('Import failed: ' + table.get_path_name())


replace_rows(icons, icon_fields, icon_rows, icon_before)
replace_rows(items, fields, rows, before)
for asset in (items, icons):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + asset.get_path_name())

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/TestMap'):
    raise RuntimeError('Could not open TestMap')
grids = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
         if isinstance(actor, unreal.PSGridWorld)]
if len(grids) != 1:
    raise RuntimeError('Expected one GridWorld')
grids[0].get_editor_property('gathering').set_editor_property('item_icon_table', icons)
if not level.save_current_level():
    raise RuntimeError('Could not save TestMap')

report = {'items': sorted(textures), 'icon_table': icons.get_path_name(),
          'textures': {key: value.get_path_name() for key, value in textures.items()}}
report_path = root / '.codex-tmp' / 'gathering-inventory-icons.json'
report_path.parent.mkdir(parents=True, exist_ok=True)
report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('GATHERING_INVENTORY_ICONS_READY ' + json.dumps(report))
