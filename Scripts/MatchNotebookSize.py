"""Match the Encyclopedia notebook to the existing inventory's authored frame."""
import unreal

journal = unreal.load_asset('/Game/UI/Encyclopedia/WBP_Encyclopedia')
inventory = unreal.load_asset('/Game/UI/Inventory/WBP_Inventory')
if not journal or not inventory or not unreal.PSFishingJournalWidget.match_inventory_notebook(journal, inventory):
    raise RuntimeError('Could not match Encyclopedia notebook to inventory')
unreal.BlueprintEditorLibrary.compile_blueprint(journal)
if not unreal.EditorAssetLibrary.save_loaded_asset(journal):
    raise RuntimeError('Could not save matched Encyclopedia notebook')
unreal.log('NOTEBOOK_SIZE_MATCH_READY')
