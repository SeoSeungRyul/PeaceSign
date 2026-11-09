import unreal
lib = unreal.MaterialEditingLibrary
mat = unreal.load_asset('/Game/Art/Tiles/Water/LinkPrototype/MI_LinkWater')
lib.set_material_instance_static_switch_parameter_value(mat, 'UseFixedZ', False)
lib.set_material_instance_static_switch_parameter_value(mat, 'Enable Waves', True)
lib.update_material_instance(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('LINK_FIXED_Z: ' + str(lib.get_material_instance_static_switch_parameter_value(mat, 'UseFixedZ')))
