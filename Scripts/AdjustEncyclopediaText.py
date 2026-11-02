"""Center Encyclopedia text inside its artwork regions without rebuilding the book."""
import unreal

journal = unreal.load_asset('/Game/UI/Encyclopedia/WBP_Encyclopedia')
if not journal or not unreal.PSFishingJournalWidget.adjust_book_text_alignment(journal):
    raise RuntimeError('Could not adjust Encyclopedia text')
unreal.BlueprintEditorLibrary.compile_blueprint(journal)
if not unreal.EditorAssetLibrary.save_loaded_asset(journal):
    raise RuntimeError('Could not save Encyclopedia text adjustment')
unreal.log('ENCYCLOPEDIA_TEXT_ALIGNMENT_READY horizontal=center vertical=center fit=down_only')
