"""Run in Unreal Editor with -ExecutePythonScript. Creates new assets only."""
import unreal

ROOT = '/Game/Art/Tiles/Water'
PATH = ROOT + '/M_WaterSurface'
if any(unreal.EditorAssetLibrary.does_asset_exist(p) for p in [PATH, ROOT + '/MI_WaterSurface']):
    raise RuntimeError('Preserving existing water assets; edit them explicitly instead: ' + PATH)

lib = unreal.MaterialEditingLibrary
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'M_WaterSurface', ROOT, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)
lib.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)

texture = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D)
texture.set_editor_property('parameter_name', 'SpriteTexture')
texture.set_editor_property('texture', unreal.load_asset(ROOT + '/Set3/Water_Set3_MM'))
world = lib.create_material_expression(material, unreal.MaterialExpressionWorldPosition)
time = lib.create_material_expression(material, unreal.MaterialExpressionTime)
custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('description', 'Water only; fixed artwork UVs and world-space ripples')
custom.set_editor_property('code', '''
// Colors are linear after texture sampling. Restrict effects to bright cyan
// water, excluding grass and the dark-blue painted shoreline shadow.
float mask = smoothstep(0.05, 0.18, Sprite.b - Sprite.r)
           * smoothstep(0.32, 0.46, Sprite.g)
           * smoothstep(0.01, 0.05, Sprite.b - Sprite.g);
float2 p = Position.xy * max(RippleScale, 0.0001);
float t = Seconds * max(RippleSpeed, 0.0);
// Fixed top-down camera: screen up is world +X. Advect every layer together
// toward +X; do not move the source artwork UVs or shoreline.
p.x -= t * (1.5 / 0.65);
float wave = sin(p.x + p.y * 0.63)
           * sin(p.y * 1.31 - p.x * 0.41);
float crest = pow(saturate(sin(p.x * 0.65 + p.y * 1.45
            + sin(p.x * 0.4) * 0.8)), 14.0);
float glint = crest * (0.45 + 0.55 * saturate(sin(p.x * 1.1 - p.y * 0.4)));
return saturate(Sprite * (1.0 + mask * wave * clamp(RippleStrength, 0.0, 0.3))
              + mask * glint * clamp(ShimmerStrength, 0.0, 0.3));
''')
inputs = []
for name in ['Sprite', 'Position', 'Seconds', 'RippleStrength', 'RippleSpeed', 'RippleScale', 'ShimmerStrength']:
    item = unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
custom.set_editor_property('inputs', inputs)

def connect(source, output, name):
    if not lib.connect_material_expressions(source, output, custom, name):
        raise RuntimeError('Connection failed: ' + name)

connect(texture, 'RGB', 'Sprite')
connect(world, '', 'Position')
connect(time, '', 'Seconds')
for name, default in [('RippleStrength', 0.28), ('RippleSpeed', 1.6),
                      ('RippleScale', 0.05), ('ShimmerStrength', 0.14)]:
    scalar = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
    scalar.set_editor_property('parameter_name', name)
    scalar.set_editor_property('default_value', default)
    connect(scalar, '', name)
if not lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError('Could not connect water color')
lib.layout_material_expressions(material)
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'MI_WaterSurface', ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
lib.set_material_instance_parent(instance, material)
unreal.EditorAssetLibrary.save_loaded_asset(instance)
unreal.log('WATER_SURFACE_CREATED: ' + instance.get_path_name())
