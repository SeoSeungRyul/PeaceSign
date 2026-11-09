import unreal
unreal.EditorLevelLibrary.load_level('/Game/Maps/TestMap')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, unreal.StaticMeshActor):
        component=actor.static_mesh_component
        mesh=component.get_editor_property('static_mesh')
        mat=component.get_material(0)
        unreal.log('WATER_BACKDROP: ' + actor.get_actor_label() + ' mesh=' + str(mesh.get_path_name() if mesh else '')
                   + ' material=' + str(mat.get_path_name() if mat else '') + ' position=' + str(actor.get_actor_location()))
