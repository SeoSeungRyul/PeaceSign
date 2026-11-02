# 나무 데이터 테이블과 CSV 가져오기

2026-11-02. `피스사인 데이터 11-2.xlsx`의 Item, TreeCutDrop, TreeShakeDrop을 사용한다. 코드와 에셋 이름은 `TreeShakeDrop`으로 통일한다.

## 테이블과 Row Type

| CSV | 권장 에셋 경로 | Unreal Import Row Type | 데이터 행 수 |
| --- | --- | --- | --- |
| Item.csv | /Game/DataBase/DT_Item | PSItemTableRow | 66 |
| TreeCutDrop.csv | /Game/DataBase/DT_TreeCutDrop | PSTreeCutDropRow | 42 |
| TreeShakeDrop.csv | /Game/DataBase/DT_TreeShakeDrop | PSTreeShakeDropRow | 48 |

구조체와 조회·검증·추첨 함수는 `Source/PeaceSign/Data/PSGameDataTables.h`와 `.cpp`에 있다. C++ 타입 이름은 각각 `FPSItemTableRow`, `FPSTreeCutDropRow`, `FPSTreeShakeDropRow`이며 에디터와 Python에서는 F 접두사를 뺀 이름을 사용한다.

원본을 변환한 가져오기용 파일은 `SourceData/TreeTables/`에 준비한다. 원본 엑셀은 수정하지 않는다. Item은 전체 66개 항목의 공통 정보를 보관하며, 현재 나무 드롭의 인벤토리 종류 연결은 원목·씨앗·선인장·사과·밤 14종을 지원한다.

## 엑셀에서 직접 CSV를 만들 때

**현재 원본의 1행은 한글 설명, 2행은 영문 필드명, 3행은 자료형, 4행부터 실제 데이터다. 따라서 1행과 3행을 제외해야 한다. 영문 필드명 행까지 지우면 안 된다.**

시트마다 별도 파일로 `CSV UTF-8 쉼표로 분리` 형식으로 저장한다. CSV 첫 줄에는 아래 헤더 하나만 남기며 데이터는 다음 줄부터 시작한다.

### Item

원본 ID 열을 첫 열의 고유 행 이름으로 사용한다. 첫 머리글 `ID`를 `---`로 바꾸고 나머지 영문 머리글을 유지한다. 별도의 ID 데이터 열을 중복 추가하지 않는다.

```csv
---,Name,Description,IconID,Sellp,BuyP,Store,Isdiscard
030101,참나무 원목,,070202,-1,-1,None,True
030201,참나무 씨앗,,070202,-1,-1,None,True
```

- `Name`, `Description`은 한글 문자열이다. 설명의 빈칸은 허용한다.
- `IconID`, `Store`는 ID다. 값이 없으면 `None`을 사용한다.
- `Sellp`, `BuyP`는 정수다. 미정 가격은 `-1`, 무료로 확정한 가격은 `0`으로 구분한다. 가져오기용 CSV의 숫자 칸을 비우지 않는다.
- `Isdiscard`는 `True` 또는 `False`를 사용한다.

### TreeCutDrop과 TreeShakeDrop

원본의 ID는 여러 후보를 묶는 드롭 테이블 번호다. 같은 값이 여러 행에 반복되므로 첫 열의 행 이름으로 쓰면 중복 오류가 발생한다. **맨 앞에 `---` 열을 추가하고, 각 행에 서로 다른 이름을 넣는다. 기존 ID 열은 두 번째 열로 그대로 남긴다.**

```csv
---,ID,ItemID,DropRate,Count
100101_030101_3,100101,030101,0.25,3
100101_030101_4,100101,030101,0.25,4
100101_030101_5,100101,030101,0.25,5
100101_030101_6,100101,030101,0.25,6
100101_030201_0,100101,030201,0.3333333333,0
100101_030201_1,100101,030201,0.3333333333,1
100101_030201_2,100101,030201,0.3333333333,2
```

위 예시는 참나무 벌목 한 묶음의 전체 후보다. 행 이름은 사람이 알아보기 쉬운 이름이면 되지만 테이블 안에서 반드시 고유해야 한다. 원본 추출기는 `드롭ID_아이템ID_개수` 형태를 자동으로 만든다.

- `DropRate`는 `0.25`, `0.032`, `0.3333333333`처럼 **0~1 사이 소수**를 넣는다. `25`, `25%`, `=1/3` 문자열을 넣지 않는다. 엑셀 수식은 계산된 숫자로 내보낸다.
- `Count`는 0 이상의 정수다. `0`은 유효한 무보상 수량이므로 삭제하지 않는다.
- 흔들기의 `ItemID=0, Count=0`은 꽝이다. 드롭 ID와 아이템의 ID를 혼동하지 않는다.
- Item에 등록되지 않은 아이템, 음수 확률·수량, 확률 합이 맞지 않는 그룹은 검증에서 실패한다.
- 벌목은 같은 `ID + ItemID`별 확률 합이 1이어야 한다. 원목과 씨앗을 독립적으로 각각 한 번 선택한다.
- 흔들기는 같은 `ID`의 전체 확률 합이 1이어야 한다. 전체 후보에서 한 번 선택한다.

### 공통 주의사항

1. `030101`, `070202` 등 앞자리 0이 있는 ID는 6자리 텍스트로 관리한다. CSV를 엑셀에서 다시 열고 저장하면 0이 사라질 수 있으므로 열 형식을 텍스트로 지정한다. CSV 안에는 작은따옴표를 붙이지 않는다.
2. 첫 열은 DataTable의 고유 Row Name이다. Import Key Field는 기본값인 빈칸으로 둔다. Drop 테이블에서 ID를 Import Key Field로 지정하면 반복 ID 때문에 행이 유실된다.
3. 빈 제목 행, 한글 설명 행, 자료형 행, 합계 행을 포함하지 않는다. 쉼표 대신 탭이나 세미콜론으로 내보내지 않는다.
4. 문자열에 쉼표·줄바꿈·큰따옴표가 있으면 CSV 규칙에 맞게 인용해야 한다. 엑셀의 CSV 내보내기 또는 제공한 변환기를 사용하면 처리된다.
5. 이미 가져온 파일은 같은 원본 경로를 유지한 뒤 DataTable의 Reimport로 갱신한다. 갱신 후 Validate Tree Tables를 실행한다.

## 에디터에서 연결

1. C++ 빌드 완료 후 에디터를 실행한다. 새 Row Type이 목록에 없으면 에디터를 다시 시작한다.
2. 세 CSV를 각각 Content/DataBase에 가져오고 위 표의 Row Type을 선택한다. Import 경고를 확인하고, Item 66행·Cut 42행·Shake 48행인지 확인한다.
3. TestMap의 GridWorld를 선택하고 Gathering 컴포넌트의 Item Table, Tree Cut Drop Table, Tree Shake Drop Table에 에셋을 지정한다. 세 테이블은 함께 지정한다.
4. Axe Table 연결은 유지한다. 별도의 수종 테이블 연결은 필요 없다. 나무 HP는 Gathering > Trees > Tree Max Health에서 조절한다. 수종별 드롭 그룹 번호는 아래 표와 같이 `PSGathering::GetTree`에서 지정한다. 보상 수량과 확률은 CSV에서 편집하고, 그룹 번호 자체를 변경할 경우에는 이 수종 매핑도 함께 수정한다.

| 수종 | CutDropID | ShakeDropID |
| --- | --- | --- |
| 참나무 Oak | 100101 | 100201 |
| 단풍나무 Maple | 100102 | 100202 |
| 벚나무 Cherry | 100103 | 100203 |
| 야자수 Palm | 100104 | 100204 |
| 자작나무 Birch | 100105 | 100205 |
| 선인장 Cactus | 100106 | 100206 |

`Scripts/ImportTreeTables.py`를 Unreal 에디터 Python에서 실행하면 테이블을 최초 생성하고 TestMap에 연결한다. 기존 테이블의 편집 내용을 덮어쓰지 않는다. 새 CSV 내용을 기존 테이블에 반영하려면 Reimport를 사용한다.

## 코드와 Blueprint에서 사용

`PSGameDataLibrary`에서 제공하는 Blueprint 함수는 다음과 같다.

- **Find Item**: Item Table과 ItemID를 넣어 이름·설명·아이콘 ID·판매가·구매가·상점 ID·버리기 여부를 조회한다.
- **Validate Tree Tables**: 세 테이블의 행 구조, 참조 아이템, 수량과 확률 합을 검증한다. 실패 시 Error에 이유를 반환한다.
- **Roll Tree Cut Drop**: Item Table, 벌목 테이블, DropID와 RandomSeed를 넣는다. 원목과 씨앗을 별도로 추첨한 Rewards 배열을 반환한다.
- **Roll Tree Shake Drop**: 흔들기 그룹에서 한 후보를 추첨한다. 반환값 True이고 Rewards가 비어 있으면 정상적인 꽝이다. False는 데이터 오류다.
- **Canonical Item ID**: 기존 `Log.Oak`, `Log.Maple`, `TreeSeed.Oak`, `TreeSeed.Maple`을 새 숫자 ID로 변환한다.

기존 벌목·흔들기 실행 코드는 이미 이 함수를 사용한다. 꽝도 7일 쿨타임을 적용하고, 확정한 보상을 저장하여 재접속 시 다시 추첨하지 않는다. 도끼 추가 목재 효과는 원목 추첨 후 한 번 적용한다. 테이블을 일부만 연결했거나 데이터가 잘못되면 작업을 막는다. 세 테이블을 모두 비워둔 기존 개발용 월드만 이전 고정 보상을 유지한다.

Item의 이름·설명은 기존 인벤토리와 단축바 표시에서 조회한다. 원목·씨앗 12종의 IconID는 해당 아이템 ID와 같으며, `DT_GatheringIcon`의 같은 행에서 투명 텍스처를 조회한다. `Scripts/ImportGatheringIcons.py`가 텍스처 가져오기와 TestMap의 Item Icon Table 연결을 처리한다. 가격·상점·판매·버리기 동작은 후속 연결 대상이며, 물고기의 기존 아이콘·도감 설명 처리는 유지한다.

## 원본 엑셀에서 다시 추출

`Scripts/ExportTreeTables.py`에 엑셀 경로를 전달하면 세 CSV를 다시 만든다. `openpyxl`이 설치된 Python 환경을 사용한다.

```powershell
python Scripts/ExportTreeTables.py "C:/Users/main/Downloads/피스사인 데이터 11-2.xlsx"
```

이 명령은 원본 엑셀을 읽기만 하고 `SourceData/TreeTables/`의 세 CSV를 갱신한다. 원본의 수식 캐시가 없으면 임의 계산하지 않고 오류를 표시한다. 이 경우 엑셀에서 재계산·저장한 뒤 다시 실행한다. 수식에 들어 있는 문장을 명령으로 실행하지 않는다.

현재 원본에 비어 있는 가격은 -1, 상점은 None으로 내보낸다. `=1/3`은 저장된 숫자 값 0.3333333333으로 내보낸다. 드롭의 무보상 행과 아이템 ID의 앞자리 0도 보존한다.

원본 엑셀의 IconID는 공용 임시 값이므로, 추출 스크립트가 원목 `030101~030106`과 씨앗 `030201~030206`만 프로젝트의 개별 아이콘 ID로 변환한다. 따라서 CSV 재추출·재가져오기 후에도 12종의 아이콘 연결이 유지된다. 사과·밤은 별도 아트가 준비될 때 추가한다.

2026-11-02 정리: `DT_Tree` 에셋과 컴포넌트 참조·생성 코드를 제거했다. 기존 에셋은 참나무·단풍나무의 HP 60과 이전 고정 보상을 담고 있었으며 맞춤 값은 없었다. 맵 참조를 해제하고 다른 에셋 참조가 없음을 확인한 뒤 삭제했다. 현재 사용하는 테이블은 Item·TreeCutDrop·TreeShakeDrop과 도끼용 Axe다.

정리 후 빌드와 전체 자동화 테스트 28개가 성공했다. 새 나무에 컴포넌트의 HP 75 설정이 적용되고, 설정을 90으로 바꿔도 기존 저장의 남은 HP 50은 유지되는 것을 검증했다. 실행 코드·생성 스크립트에 이전 테이블 참조가 없고 에셋 파일도 제거됐음을 확인했다. 관련 기록은 `.codex-tmp/tree-retirement-audit.log`, `tree-retirement-remove.log`, `tree-retirement-build.log`, `tree-retirement-tests.log`에 있다.

## 검증 기록

2026-11-02: UE 5.8 PeaceSignEditor Win64 Development 빌드 성공. 세 CSV의 실제 에디터 가져오기·에셋 저장·TestMap 연결을 확인했다. 실제 렌더링 환경에서 PeaceSign 자동화 테스트 28개가 모두 성공했다. 신규 `PeaceSign.Gathering.CSVTablesAndWeightedDrops`는 CSV 행 수와 한국어, 확률 추첨 범위, 0개 씨앗과 꽝, 잘못된 확률·아이템 참조, 재접속 후 보상 고정, 꽝 쿨타임과 기존 ID 합치기를 검증한다.

TestMap의 실제 게임 실행에서도 CSV 기반 벌목·드롭과 이동 잠금 해제를 확인했고 `PS.Gathering.Smoke`가 성공했다. 빌드·가져오기·테스트·게임 실행 기록은 `.codex-tmp/tree-tables-build.log`, `tree-tables-import.log`, `tree-tables-regression.log`, `tree-tables-smoke.log`에 있다.
