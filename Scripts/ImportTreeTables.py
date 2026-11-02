"""Create the three CSV-backed tables and connect TestMap. Reruns preserve existing tables.

For updates use the DataTable editor's Reimport action; the CSV source path is retained.
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
tools = unreal.AssetToolsHelpers.get_asset_tools()
tables = {}
for name, row_struct in [('Item', 'PSItemTableRow'), ('TreeCutDrop', 'PSTreeCutDropRow'), ('TreeShakeDrop', 'PSTreeShakeDropRow')]:
    source = root / 'SourceData' / 'TreeTables' / f'{name}.csv'
    if not source.is_file():
        raise RuntimeError(f'Missing CSV: {source}')
    table = unreal.load_asset(f'/Game/DataBase/DT_{name}')
    if not table:
        factory = unreal.DataTableFactory()
        factory.set_editor_property('struct', unreal.load_object(None, f'/Script/PeaceSign.{row_struct}'))
        table = tools.create_asset(f'DT_{name}', '/Game/DataBase', unreal.DataTable, factory)
        if not table or not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, str(source)):
            raise RuntimeError(f'Could not import {name}')
        # Keep native Reimport pointing to the prepared CSV rather than the workbook.
        table.get_editor_property('asset_import_data').scripted_add_filename(str(source), 0, '')
    if table.get_editor_property('row_struct').get_name() != row_struct:
        raise RuntimeError(f'DT_{name} uses an incompatible row struct')
    tables[name] = table

# Unreal Python treats a bool return with output parameters as success/failure:
# successful calls return the Error output (empty string), failures return None.
validation_result = unreal.PSGameDataLibrary.validate_tree_tables(tables['Item'], tables['TreeCutDrop'], tables['TreeShakeDrop'])
if validation_result is None:
    raise RuntimeError('Tree data validation failed; use Validate Tree Tables in Blueprint for the Error output')
for table in tables.values():
    if not unreal.EditorAssetLibrary.save_loaded_asset(table):
        raise RuntimeError('Could not save a tree data table')

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/TestMap'):
    raise RuntimeError('Could not open TestMap')
grids = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
         if isinstance(actor, unreal.PSGridWorld)]
if len(grids) != 1:
    raise RuntimeError('Expected one GridWorld')
gathering = grids[0].get_editor_property('gathering')
gathering.set_editor_property('item_table', tables['Item'])
gathering.set_editor_property('tree_cut_drop_table', tables['TreeCutDrop'])
gathering.set_editor_property('tree_shake_drop_table', tables['TreeShakeDrop'])
if not level.save_current_level():
    raise RuntimeError('Could not save TestMap')
unreal.log('TREE_TABLES_READY Item + TreeCutDrop + TreeShakeDrop')
