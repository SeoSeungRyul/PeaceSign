# 맵 나무 이미지와 배치

2026-11-03: 긴 기둥 시안의 6종을 각각 256×256 RGBA PNG로 분리하고 TestMap의 실제 나무에 적용했다. 수관은 비율을 유지하며 아래 줄기의 중심을 픽셀 (128,240)에 맞췄다.

## 파일과 에셋

- 개별 PNG: `SourceArt/Gathering/TreeSprites/256/T_Tree_Oak.png`, `Maple`, `Cherry`, `Palm`, `Cactus`, `Birch`.
- 묶음: `SourceArt/Gathering/TreeSprites/TreeSprites-256.zip`.
- Unreal 에셋: `/Game/Art/Gathering/Trees`의 `T_Tree_*` 텍스처, `S_Tree_*` 서 있는 나무와 `S_Stump_*` 그루터기 스프라이트.
- `Scripts/ExportTreeSprites.ps1`은 원본 시안을 잘라 투명 PNG를 만든다. 기존 출력 폴더가 있으면 덮어쓰지 않는다. 현재 시안 1254×1254의 분리 영역을 사용하므로 원본을 교체하면 분리 영역도 확인한다.
- `Scripts/ImportTreeSprites.py`는 텍스처·스프라이트와 TestMap 연결을 만든다. 재실행은 기존 스프라이트와 배치 ID를 유지한다.

## 위치와 표시

TestMap의 캐릭터 시작 셀은 (2,-1)이다. 그 아래쪽에 3개씩 두 줄의 배치 기준점을 지정했다. 위쪽 줄은 참나무·단풍나무·벚나무, 아래쪽 줄은 야자수·선인장·자작나무다. 실제 생성 시 물·작물·돌·도장·거점과 통로를 피하고 기준점 근처의 빈 땅을 찾는다. 기존 지형과 저장된 아이템을 덮어쓰지 않는다.

GridWorld > Gathering > Trees > Placed Trees에서 기준점을 편집한다. `PlacementId`는 같은 나무를 식별하는 저장 키이므로 기존 배치의 재생성을 의도하지 않았다면 유지한다. 새 게임과 이미 초기화된 저장 모두 누락된 배치를 한 번만 추가하며, 베어진 그루터기를 새 나무로 다시 만들지 않는다.

GridWorld > Gathering > Art의 Tree Sprites와 Sprite Height In Cells에서 표시를 조절한다. 기본 그림 높이는 3.2칸이다. 스프라이트는 탑뷰 바닥 평면에 맞춰 표시하고 줄기 기준점에 기존 물리 충돌을 유지한다. 수관 자체는 이동을 막지 않는다.

## 조작

나무에 접근해 줄기 아래쪽 셀에 커서를 놓는다. 단축바 9번의 도끼를 선택하고 우클릭하면 벌목한다. 맨손이나 나무 씨앗 이외의 다른 장비로 우클릭하면 흔든다. 쓰러짐 애니메이션이 끝나면 나무가 사라지고 칸을 비운다. 드롭은 가까이 가서 수집한다.

## 검증

- UE 5.8 PeaceSignEditor Win64 Development 빌드 성공.
- 전체 자동화 테스트 31개 성공.
- `PeaceSign.Gathering.PlacedSpritesAndInteractions`: 6종 아트, 256×256 원본, 줄기 피벗, 탑뷰 방향, 수관 비충돌, 물 지형 보존, 흔들기·벌목·드롭 수집과 제거한 배치의 저장을 확인한다.
- 별도 저장으로 실제 TestMap을 실행할 때 `-PSTreeSpritesOverview` 옵션을 주고 `PS.Gathering.Smoke` 명령으로 배치와 벌목 후 칸 비우기, 보상 수집을 확인한다. 검증용 카메라는 이 옵션에서만 적용한다.

로컬 실행 기록은 `.codex-tmp/tree-sprites-tests-verified.log`, `.codex-tmp/tree-sprites-smoke-verified.log`이며 실제 게임 캡처는 `Saved/Reviews`에 복사한다.
