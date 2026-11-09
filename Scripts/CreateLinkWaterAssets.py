import unreal

root = '/Game/Art/Tiles/Water/LinkPrototype'
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
def create(name, cls, factory):
    path = root + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Preserving existing prototype asset: ' + path)
    return assets.create_asset(name, root, cls, factory)

wave_class = unreal.load_class(None, '/Script/Water.WaterWavesAsset')
factory_class = unreal.load_class(None, '/Script/WaterEditor.WaterWavesAssetFactory')
if not wave_class or not factory_class:
    raise RuntimeError('Native Water asset classes unavailable')
waves = create('WW_LinkWater', wave_class, unreal.new_object(factory_class))
source = waves.get_editor_property('water_waves')
if source is None:
    source = unreal.new_object(unreal.load_class(None, '/Script/Water.GerstnerWaterWaves'), outer=waves)
    waves.set_editor_property('water_waves', source)
generator = source.get_editor_property('gerstner_wave_generator')
for key, value in {'num_waves': 8, 'min_wavelength': 60.0, 'max_wavelength': 200.0,
                   'min_amplitude': 0.3, 'max_amplitude': 3.0,
                   'small_wave_steepness': 0.1, 'large_wave_steepness': 0.1}.items():
    generator.set_editor_property(key, value)
unreal.EditorAssetLibrary.save_loaded_asset(waves)

water = create('MI_LinkWater', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
parent = unreal.load_asset('/Water/Materials/WaterSurface/Water_Material_CustomMesh')
if not parent:
    raise RuntimeError('Native CustomMesh water material missing')
lib.set_material_instance_parent(water, parent)
lib.set_material_instance_static_switch_parameter_value(water, 'UseFixedZ', False)
for name,value in [('Water Roughness',0.08),('Water Specular',0.8),('Default Near Normal Strength',0.8)]:
    lib.set_material_instance_scalar_parameter_value(water,name,value)
lib.update_material_instance(water)
unreal.EditorAssetLibrary.save_loaded_asset(water)
unreal.log('LINK_WATER_PARAMETERS: ' + str(lib.get_scalar_parameter_names(parent)))

shore = create('M_LinkWaterShore', unreal.Material, unreal.MaterialFactoryNew())
shore.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
shore.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
shore.set_editor_property('two_sided', True)
lib.set_base_material_usage(shore, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
tex = lib.create_material_expression(shore, unreal.MaterialExpressionTextureSampleParameter2D)
tex.set_editor_property('parameter_name', 'SpriteTexture')
tex.set_editor_property('texture', unreal.load_asset('/Game/Art/Tiles/Water/Set3/Water_Set3_MM'))
cutout = lib.create_material_expression(shore, unreal.MaterialExpressionCustom)
cutout.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
cutout.set_editor_property('code', 'return (C.b > C.r + 0.05 && C.g > 0.25 && C.b > C.g + 0.01) ? 0.0 : 1.0;')
item = unreal.CustomInput(); item.set_editor_property('input_name', 'C')
cutout.set_editor_property('inputs', [item])
assert lib.connect_material_expressions(tex, 'RGB', cutout, 'C')
assert lib.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(cutout, '', unreal.MaterialProperty.MP_OPACITY_MASK)
lib.recompile_material(shore)
unreal.EditorAssetLibrary.save_loaded_asset(shore)
unreal.log('LINK_WATER_ASSETS_CREATED')
