import unreal

root = '/Game/Art/Tiles/Water/LinkPrototype/'
waves = unreal.load_asset(root + 'WW_LinkWater')
waves.get_editor_property('water_waves').get_editor_property('gerstner_wave_generator').set_editor_property('max_amplitude', 3.0)
unreal.EditorAssetLibrary.save_loaded_asset(waves)
shore = unreal.load_asset(root + 'M_LinkWaterShore')
node = unreal.MaterialEditingLibrary.get_material_property_input_node(shore, unreal.MaterialProperty.MP_OPACITY_MASK)
node.set_editor_property('code', 'return (C.b > C.r + 0.05 && C.g > 0.25 && C.b > C.g + 0.01) ? 0.0 : 1.0;')
unreal.MaterialEditingLibrary.recompile_material(shore)
unreal.EditorAssetLibrary.save_loaded_asset(shore)
if not unreal.EditorLevelLibrary.load_level('/Game/Maps/WaterLinkComparison'):
    raise RuntimeError('Prototype map missing')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() in ['Legacy_Water', 'Link_Water']:
        actor.refresh_comparison()
    if isinstance(actor, unreal.SkyLight):
        component = actor.light_component
        component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        component.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
        component.set_editor_property('intensity', 1.0)
        component.recapture_sky()
    if isinstance(actor, unreal.CameraActor):
        actor.set_actor_rotation(unreal.Rotator(pitch=-50,yaw=0,roll=0), False)
    if isinstance(actor, unreal.DirectionalLight):
        actor.set_actor_rotation(unreal.Rotator(pitch=-55,yaw=-30,roll=0), False)
mat = unreal.load_asset(root + 'MI_LinkWater')
for name,value in [('Water Roughness',0.08),('Water Specular',0.8),('Default Near Normal Strength',0.8)]:
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mat,name,value)
unreal.MaterialEditingLibrary.update_material_instance(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.EditorLevelLibrary.save_current_level()
unreal.log('LINK_WATER_MAP_REFRESHED')
