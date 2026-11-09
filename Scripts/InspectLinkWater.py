import unreal
lib = unreal.MaterialEditingLibrary
mat = unreal.load_asset('/Game/Art/Tiles/Water/LinkPrototype/MI_LinkWater')
unreal.log('LINK_SWITCHES: ' + str(lib.get_static_switch_parameter_names(mat)))
unreal.log('LINK_VECTORS: ' + str(lib.get_vector_parameter_names(mat)))
for name in lib.get_static_switch_parameter_names(mat):
    unreal.log('LINK_SWITCH: ' + str(name) + '=' + str(lib.get_material_instance_static_switch_parameter_value(mat,name)))
