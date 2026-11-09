import unreal

path = '/Game/Maps/WaterLinkComparison'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    if not unreal.EditorLevelLibrary.load_level(path):
        raise RuntimeError('Could not resume blank prototype map')
else:
    if not unreal.EditorLevelLibrary.new_level(path):
        raise RuntimeError('Could not create isolated comparison map')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if any(a.get_actor_label() in ['Legacy_Water', 'Link_Water'] for a in actors.get_all_level_actors()):
    raise RuntimeError('Preserving already populated comparison map')
cls = unreal.load_class(None, '/Script/PeaceSign.PSLinkWaterComparison')
if not cls:
    raise RuntimeError('Comparison actor class not built')
for name, y, native in [('Legacy_Water', 0, False), ('Link_Water', 650, True)]:
    pond = actors.spawn_actor_from_class(cls, unreal.Vector(0,y,0))
    pond.set_actor_label(name)
    pond.set_editor_property('use_link_water', native)
    pond.refresh_comparison()
    if native and pond.get_editor_property('preview_status') != 'Native Water Body Custom active':
        raise RuntimeError('Native preview not ready: ' + pond.get_editor_property('preview_status'))

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,800))
sun.set_actor_rotation(unreal.Rotator(pitch=-55,yaw=-30,roll=0), False)
sun.light_component.set_editor_property('intensity', 5.0)
sun.light_component.set_editor_property('atmosphere_sun_light', True)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0,0,0))
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0,0,500))
sky.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity', 1.0)
sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
sky.light_component.recapture_sky()
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-400,575,800))
camera.set_actor_rotation(unreal.Rotator(pitch=-50,yaw=0,roll=0), False)
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
camera.camera_component.set_editor_property('projection_mode', unreal.CameraProjectionMode.ORTHOGRAPHIC)
camera.camera_component.set_editor_property('ortho_width', 1500.0)
world = unreal.EditorLevelLibrary.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-400,575,800), unreal.Rotator(-50,0,0))
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Could not save comparison map')
unreal.log('LINK_WATER_MAP_CREATED: ' + path)
