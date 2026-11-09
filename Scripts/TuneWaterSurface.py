"""Strengthen the existing water effect without touching textures or maps."""
from pathlib import Path
import re
import unreal

material = unreal.load_asset('/Game/Art/Tiles/Water/M_WaterSurface')
instance = unreal.load_asset('/Game/Art/Tiles/Water/MI_WaterSurface')
if not material or not instance:
    raise RuntimeError('Expected existing water material and instance')
lib = unreal.MaterialEditingLibrary
# Use the same shader definition as the initial-creation script, without running it.
source = Path(__file__).with_name('CreateWaterSurfaceMaterial.py').read_text(encoding='utf-8')
code = re.search(r"custom.set_editor_property\('code', '''(.*?)'''\)", source, re.S).group(1)
custom = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
if not isinstance(custom, unreal.MaterialExpressionCustom):
    raise RuntimeError('Expected custom water color expression; preserving asset')
custom.set_editor_property('code', code)
values = {'RippleStrength': 0.28, 'RippleSpeed': 1.6,
          'RippleScale': 0.05, 'ShimmerStrength': 0.14}
for name, value in values.items():
    # UE5.8's setter returns false even after assigning; verify the readback.
    lib.set_material_instance_scalar_parameter_value(instance, name, value)
    actual = lib.get_material_instance_scalar_parameter_value(instance, name)
    if abs(actual - value) > 0.00001:
        raise RuntimeError('Parameter readback failed: ' + name)
lib.recompile_material(material)
lib.update_material_instance(instance)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.EditorAssetLibrary.save_loaded_asset(instance)
unreal.log('WATER_TUNED: ' + str(values))
