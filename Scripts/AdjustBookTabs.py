"""Attach book tabs to the page edges, preserving text, artwork and content layout."""
import unreal

journal = unreal.load_asset('/Game/UI/Encyclopedia/WBP_Encyclopedia')
if not journal or not unreal.PSFishingJournalWidget.adjust_book_tabs(journal):
    raise RuntimeError('Could not adjust Encyclopedia bookmark layering')
unreal.BlueprintEditorLibrary.compile_blueprint(journal)
if not unreal.EditorAssetLibrary.save_loaded_asset(journal):
    raise RuntimeError('Could not save Encyclopedia bookmark layout')

inventory = unreal.load_asset('/Game/UI/Inventory/WBP_Inventory')
if not inventory or not unreal.PSInventoryWidget.apply_book_tab_artwork(inventory):
    raise RuntimeError('Could not attach inventory bookmarks')
unreal.BlueprintEditorLibrary.compile_blueprint(inventory)
if not unreal.EditorAssetLibrary.save_loaded_asset(inventory):
    raise RuntimeError('Could not save inventory bookmark layout')
unreal.log('BOOK_TAB_ATTACHMENT_READY')
