import unreal


def create_material():
    path = '/Game/Art/Tiles/Farm/M_FarmTileUV'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        unreal.MaterialEditingLibrary.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        unreal.log('Preserving existing farm material: ' + path)
        return

    # A saved asset is used in editor and packaged games; UV parameters crop source quadrants.
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_FarmTileUV', '/Game/Art/Tiles/Farm', unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError('Could not create farm material')
    lib = unreal.MaterialEditingLibrary
    def connect(source, output, target, input_name):
        if not lib.connect_material_expressions(source, output, target, input_name):
            raise RuntimeError('Material connection failed: ' + input_name)

    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    lib.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate)
    scale = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    scale.set_editor_property('parameter_name', 'UVScale')
    scale.set_editor_property('default_value', unreal.LinearColor(1, 1, 0, 0))
    offset = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    offset.set_editor_property('parameter_name', 'UVOffset')
    offset.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
    multiply = lib.create_material_expression(material, unreal.MaterialExpressionMultiply)
    add = lib.create_material_expression(material, unreal.MaterialExpressionAdd)
    texture = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D)
    texture.set_editor_property('parameter_name', 'SpriteTexture')
    texture.set_editor_property('texture', unreal.load_asset('/Game/Art/Tiles/Farm/Set1/Farm_Set1_MM'))
    for expression, target in ((scale, multiply), (offset, add)):
        mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask)
        mask.set_editor_property('r', True)
        mask.set_editor_property('g', True)
        mask.set_editor_property('b', False)
        mask.set_editor_property('a', False)
        connect(expression, '', mask, '')
        connect(mask, '', target, 'B')
    connect(uv, '', multiply, 'A')
    connect(multiply, '', add, 'A')
    connect(add, '', texture, 'UVs')
    assert lib.connect_material_property(texture, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(texture, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log('Farm masked material saved: ' + path)

create_material()
