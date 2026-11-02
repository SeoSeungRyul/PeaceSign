"""Explicitly rebuild the temporary fishing UI and place the test pond on TestMap.

This resets WBP_Fishing's prototype layout. Do not rerun after hand-authoring its UI.
"""
import math
import unreal

widget = unreal.load_asset('/Game/UI/Fishing/WBP_Fishing')
if not widget or not unreal.PSFishingWidget.rebuild_prototype_designer_layout(widget):
    raise RuntimeError('Could not rebuild the fishing prototype layout')
unreal.BlueprintEditorLibrary.compile_blueprint(widget)
if not unreal.EditorAssetLibrary.save_loaded_asset(widget):
    raise RuntimeError('Could not save the fishing prototype widget')

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/TestMap'):
    raise RuntimeError('Could not open TestMap')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
grids = [actor for actor in actors if isinstance(actor, unreal.PSGridWorld)]
starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
if len(grids) != 1 or len(starts) != 1:
    raise RuntimeError('Expected one GridWorld and one PlayerStart')
grid = grids[0]
position = starts[0].get_actor_location()
origin = grid.get_actor_location()
cell_size = grid.get_editor_property('cell_size')
player_x = math.floor((position.x - origin.x) / cell_size)
player_y = math.floor((position.y - origin.y) / cell_size)
# Screen up is world +X. Leave one dry cell before the near water edge.
pond_origin = unreal.IntPoint(player_x + 2, player_y - 4)
grid.set_editor_property('enable_fishing_test_pond', True)
grid.set_editor_property('fishing_test_pond_origin', pond_origin)
grid.call_method('GenerateEditorPreview')
if not level.save_current_level():
    raise RuntimeError('Could not save TestMap')
unreal.log('FISHING_PROTOTYPE_READY horizontal UI, pond origin=' + str(pond_origin))
