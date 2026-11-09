"""Update the legacy water shader only; preserve all MI tuning and native water."""
from pathlib import Path
import re
import unreal

material = unreal.load_asset('/Game/Art/Tiles/Water/M_WaterSurface')
if not material:
    raise RuntimeError('Existing legacy water material missing')
lib = unreal.MaterialEditingLibrary
node = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
if not isinstance(node, unreal.MaterialExpressionCustom):
    raise RuntimeError('Unexpected water graph; preserving it')
source = Path(__file__).with_name('CreateWaterSurfaceMaterial.py').read_text(encoding='utf-8')
code = re.search(r"custom.set_editor_property\('code', '''(.*?)'''\)", source, re.S).group(1)
node.set_editor_property('code', code)
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log('WATER_FLOW_UPDATED: world +X / fixed-camera screen up. MI values unchanged.')
