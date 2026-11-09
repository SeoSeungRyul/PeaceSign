"""Apply the requested slower pan to the gameplay character Blueprint only."""
import unreal

blueprint = unreal.load_asset('/Game/Blueprints/BP_PSPlayerCharacter')
if not blueprint:
    raise RuntimeError('Gameplay character Blueprint is missing')
defaults = unreal.get_default_object(blueprint.generated_class())
before = defaults.get_editor_property('fishing_camera_blend_seconds')
defaults.set_editor_property('fishing_camera_blend_seconds', 1.2)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
defaults = unreal.get_default_object(blueprint.generated_class())
after = defaults.get_editor_property('fishing_camera_blend_seconds')
if abs(after - 1.2) > 0.001 or not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
    raise RuntimeError('Could not save the slower fishing camera setting')
unreal.log('FISHING_CAMERA_SLOWED before={} after={}'.format(before, after))
