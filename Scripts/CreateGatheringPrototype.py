"""Create editable gathering data once. Preserve existing authored table rows on reruns."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()

# BasicShapeMaterial has no editable Color parameter. Use a saved prototype material.
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Art/Gathering/M_GatheringColor'):
    material = tools.create_asset('M_GatheringColor', '/Game/Art/Gathering', unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError('Could not create gathering material')
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(0.2, 0.5, 0.1, 1))
    if not unreal.MaterialEditingLibrary.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Could not connect gathering color')
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save gathering material')

def table(name, struct_name, csv):
    result = unreal.load_asset('/Game/DataBase/' + name)
    if not result:
        factory = unreal.DataTableFactory()
        factory.set_editor_property('struct', unreal.load_object(None, '/Script/PeaceSign.' + struct_name))
        result = tools.create_asset(name, '/Game/DataBase', unreal.DataTable, factory)
        if not result or not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(result, csv):
            raise RuntimeError('Could not create ' + name)
        if not unreal.EditorAssetLibrary.save_loaded_asset(result):
            raise RuntimeError('Could not save ' + name)
    if result.get_editor_property('row_struct').get_name() != struct_name:
        raise RuntimeError(name + ' has an incompatible row type')
    return result

powers = [10, 12, 15, 15, 20, 20, 30, 30, 60]
axe_csv = '---,Tier,Damage,StaminaCost,ExtraLogChance\n'
for index, damage in enumerate(powers):
    tier = index + 1
    axe_csv += f'Axe.Tier{tier},{tier},{damage},{5 if tier >= 6 else 10},{0.2 if tier >= 4 else 0}\n'
axes = table('DT_Axe', 'PSAxeDefinition', axe_csv)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/TestMap'):
    raise RuntimeError('Could not open TestMap')
grids = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
         if isinstance(actor, unreal.PSGridWorld)]
if len(grids) != 1:
    raise RuntimeError('Expected one GridWorld')
gathering = grids[0].get_editor_property('gathering')
gathering.set_editor_property('axe_table', axes)
if not level.save_current_level():
    raise RuntimeError('Could not save gathering data references on TestMap')
unreal.log('GATHERING_PROTOTYPE_READY DT_Axe=9 test area X=-20..-5 Y=6..21; tree HP is configured on Gathering')
