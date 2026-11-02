# 도감 책 UI

2026-10-28 사용자 확정 시안을 적용했습니다.

2026-10-29: 도감 WBP 이름을 `WBP_Encyclopedia`로 변경하고 Content/UI/Encyclopedia로 옮겼습니다. 디자이너 트리와 배치는 유지하며 컨트롤러·스크립트·테스트는 새 경로를 사용합니다. C++ 부모 클래스와 내부 바인딩 위젯 이름은 유지합니다.

글자 정렬은 고정된 위쪽 이동 대신 각 그림 영역을 기준으로 가로·세로 중앙 정렬합니다. 이름표, 제목, 특별 설명과 상세 표는 ScaleBox의 가운데 정렬을 사용하며 여러 줄 텍스트도 전체 높이를 기준으로 가운데에 놓습니다. 긴 내용은 영역을 넘을 때만 축소합니다. `Scripts/AdjustEncyclopediaText.py`는 기존 텍스트를 정렬 프레임으로 감싸며 책 그림과 탭·목록 배치를 다시 생성하지 않습니다.

## 접근과 탭

- I로 인벤토리 책을 연 뒤 위쪽 도감 탭으로 들어갑니다. F6 입력과 F6 닫기 처리는 제거했습니다.
- 위쪽 활성 탭은 위로, 오른쪽 활성 분류 탭은 오른쪽으로 더 나옵니다. 탭 그림과 클릭 영역이 함께 이동합니다.
- 2026-10-29 답안 이미지 반영: 위쪽 탭은 활성 상태에서도 아래 끝이 책에 겹치고 책 뒤에서 나오도록 배치합니다. 오른쪽 탭은 표지 앞·첫 장 뒤에 놓아 종이 가장자리부터 보입니다. 첫 장은 숨겨진 탭 부분의 클릭을 가립니다. `Scripts/AdjustBookTabs.py`로 기존 배치를 유지하며 이 변경만 적용합니다.
- 도감에서도 I 또는 Esc는 책 전체를 닫습니다. 인벤토리로 돌아가려면 위쪽 인벤토리 탭을 누릅니다.
- 인벤토리와 도감 전환 중에는 같은 책의 일시정지를 유지하고, 책을 닫으면 이 책이 건 일시정지만 해제합니다. 기존 단축바와 가방 데이터는 그대로 유지합니다.
- 스킬은 현재 연결할 화면이 없어 탭 그림과 준비 중 툴팁만 표시합니다.

## 목록과 데이터

왼쪽은 4×4 목록과 페이지 버튼, 오른쪽은 번호·이름, 사진, 별, 특별 설명과 설명/사이즈/최대 기록/계절/위치입니다. `/Game/DataBase/DT_Fish`의 유효한 행을 ID 순으로 읽습니다. 현재 16종이므로 1/1 페이지이며 항목 수가 늘면 16개씩 자동으로 페이지를 나눕니다. 아이콘은 기존 DT_FishIcon을 그대로 사용합니다.

미발견 항목은 이름과 정보가 ???로 표시되고 사진을 숨깁니다. 잡으면 이름·이미지와 기본 정보, 회색 별을 표시합니다. 최대 크기를 달성하면 노란 별과 DescriptionPlus를 표시하며 기존 도감 저장 기록을 사용합니다. 다른 세 분류(채집·음식·유물)는 탭과 빈 목록 화면만 제공하고 가짜 데이터를 만들지 않습니다.

## Designer 편집

`/Game/UI/Encyclopedia/WBP_Encyclopedia`에서 배치와 장식을 편집합니다. 인벤토리와 같은 1920×1080 기준, 같은 여백과 화면 배율을 사용합니다. `Scripts/MatchNotebookSize.py`가 WBP_Inventory 배경의 실제 앵커·크기·위치를 읽어 도감 노트의 외곽을 맞추고, Slate 배치 계산으로 상단 세 탭도 같은 위치와 크기에 맞춥니다. 인벤토리는 변경하지 않습니다. 원본 PNG/텍스처 해상도는 유지하고 도감의 화면 배치만 맞춥니다. 이 스크립트는 에디터의 Python 콘솔 또는 -ExecutePythonScript 모드에서 실행합니다.

FishList(UniformGridPanel), FishName/FishDetails/FishSpecial/FishSize/FishRecord/FishSeason/FishLocation(TextBlock), FishImage/FishStar/FishLock(Image) 이름을 유지합니다. FishList 내부 항목은 실행 시 생성하며 각 항목은 비율을 유지해 셀 안에 맞춥니다.

### 에디터에서 찾기

- 도감: Content/UI/Encyclopedia/WBP_Encyclopedia. Content Browser에서 WBP_Encyclopedia를 검색해 열고 Designer 탭에서 편집합니다.
- 인벤토리 전체: Content/UI/Inventory/WBP_Inventory.
- 인벤토리 가방: Content/UI/Inventory/WBP_BagPanel.
- 장비 영역: Content/UI/Inventory/WBP_EquipmentPanel.
- 인벤토리 정보 영역: Content/UI/Inventory/WBP_Information.

도감 Hierarchy의 JournalBook이 전체 내용, BookBackground/BookFirstPage/BookBinder가 노트 그림, TypesBackground/InformationBackground가 두 페이지, FishList가 목록 영역입니다. 글자는 이름 뒤에 Frame이 붙은 ScaleBox 안에 있으며 Frame의 Canvas Slot을 움직이거나 크기를 조절하면 중앙 정렬을 유지합니다. 실제 물고기 목록과 내용은 실행 중 채우므로 Designer의 목록이 비어 있어도 정상입니다.

평소에는 WBP를 직접 편집하고 Compile/Save합니다. ApplyEncyclopediaBook.py는 최초 전체 배치 생성용이므로 직접 편집한 후 다시 실행하지 않습니다. MatchNotebookSize.py는 크기 재정렬을 의도할 때만 사용합니다.

InventoryTab, PreviousPage, NextPage와 CategoryFish/CategoryGathering/CategoryFood/CategoryArtifact(Button)은 클릭 처리를 연결하는 이름입니다. CategoryTitleArt와 CategoryTitle은 분류 전환 시 갱신합니다. 통일한 상단 탭은 인벤토리처럼 활성 탭을 위로 24, 활성 분류 탭은 오른쪽으로 64 기준 픽셀만큼 이동합니다. 위쪽 탭은 아이콘 크기·위치를 유지하고 아래쪽 빈 배경만 연장해 활성 상태에서도 책과 틈 없이 연결합니다.

기존 WBP_Inventory는 가방·장비·스크롤 배치를 보존합니다. 기존 Button_Information/Button_Encyclopedia/Button_Setting 이미지에 실행 중 클릭 영역과 아래쪽 배경 연장을 올립니다. 위쪽 탭 줄은 책 뒤에 두고, 활성 인벤토리 탭은 기존 1920×1080 기준에서 위로 24픽셀 이동합니다. 연장한 부분은 표지에 가려져 아이콘이 잘리거나 책과 틈이 생기지 않습니다.

`Scripts/ApplyEncyclopediaBook.py`는 최초 에셋 적용용이며 다시 실행하면 도감 Designer 트리를 교체합니다. 평소 실행이나 InitializeDesignerLayout은 기존 배치를 보존합니다. 적용 전 원래 WBP 3개는 `.codex-tmp/book-journal-assets-before`에 복사했습니다.

## 검증

`PeaceSign.UI.BookJournal`은 실제 물고기 데이터, 미발견/발견/달성 상태, 빈 분류, 17번째 항목의 페이지 경계와 칸 간격을 검증합니다. 기존 `PeaceSign.Fishing.October26`, `PeaceSign.Inventory.DataAndUI`도 사용합니다.

실제 게임 메뉴 흐름은 별도 `-UserDir`의 `PS.UI.BookJournalSmoke`로 확인합니다. F6 제거, 인벤토리 도감 탭 클릭, 분류 전환, 인벤토리 복귀, I/Esc 전체 닫기, 일시정지와 단축바 복귀를 확인하고 Saved/Automation/BookJournal에 여러 해상도의 PNG를 저장합니다. 테스트 발견 기록은 실제 사용자 저장에 기록하지 않습니다.
