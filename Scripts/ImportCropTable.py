"""Create the lettuce prototype DataTable from the project's normalized CSV."""
from pathlib import Path
import unreal

source = Path(unreal.Paths.project_dir()).resolve() / "SourceData" / "CropTables" / "Crop.csv"
table = unreal.load_asset("/Game/DataBase/DT_Crop")
if not table:
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.load_object(None, "/Script/PeaceSign.PSCropDefinition"))
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DT_Crop", "/Game/DataBase", unreal.DataTable, factory)
if not table or table.get_editor_property("row_struct").get_name() != "PSCropDefinition":
    raise RuntimeError("DT_Crop is missing or uses a different row type")
if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, str(source)):
    raise RuntimeError("Could not import Crop.csv")
table.get_editor_property("asset_import_data").scripted_add_filename(str(source), 0, "")
if not unreal.EditorAssetLibrary.save_loaded_asset(table):
    raise RuntimeError("Could not save DT_Crop")
unreal.log("PEACESIGN_CROP_TABLE_READY")
