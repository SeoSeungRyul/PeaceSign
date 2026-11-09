# 새 낚시 UI 연결 방법

대상은 `/Game/UI/Fishing/WBP_Fishing`이다. 부모 클래스는 `PSFishingWidget`을 유지한다. 10월 26일 가져온 이미지로 7칸 미니게임, 가로 투척 게이지, 말풍선, 안내, 획득 결과창을 실제 WBP에 배치했다. 아래 이름과 타입을 유지하면 기존 임시 모양을 새 시안으로 교체할 수 있다. 게임 실행 중에는 Designer의 배치와 이미지를 자동 초기화하지 않는다.

## 기본 원칙

- 아래 이름과 타입은 코드 연결에 사용한다. 이름을 유지하면 크기·간격·배경·글꼴은 자유롭게 편집할 수 있다.
- 이미지를 겹칠 때는 Overlay, 크기 고정은 Size Box, 화면 위치는 Canvas 앵커를 사용한다.
- 상태별 칸 이미지의 원본 크기가 달라도 같은 크기의 Size Box 안에 중앙 정렬한다.
- 패널은 `Border` 타입을 유지하고 그 안에 배경 이미지와 콘텐츠를 배치한다.
- `Class Defaults > Fishing > Layout > Follow Player`가 켜져 있으면 투척 준비·사용 불가 안내는 캐릭터를 기준으로, 입질 말풍선·미니게임은 찌를 기준으로 이동한다. 화면에 고정하려면 끈다. 결과창은 Designer 위치를 사용한다. 낚시 카메라도 투척 확정부터 찌를 중심으로 이동하고 취소·성공·실패 시 캐릭터로 복귀한다.

## 미니게임

`FishMinigamePanel`(Border) 안에 다음을 배치한다.

| 위젯 이름 | 타입 | 역할 |
|---|---|---|
| `StateLabel` | Text Block | 대기·입력 안내 문구 |
| `TimeBar` | Progress Bar | 화살표 미니게임의 남은 시간. 입질을 기다리는 동안에는 숨김. 배경·채움 이미지는 Style에서 지정 |
| `FishDirectionRow` | Horizontal Box | 화살표 칸 행. 미니게임 중 표시 |
| `FishDirectionBox0` ~ `FishDirectionBox6` | Border | 대기·현재·완료·오입력 칸 배경 |
| `DirectionImage0` ~ `DirectionImage6` | Image | 각 칸의 방향 화살표 |
| `DirectionMarker0` ~ `DirectionMarker6` | Image | 각 칸 위의 삼각형. 현재 칸 하나만 표시 |

각 FishDirectionBox 안에 Overlay를 넣어 DirectionImage와 DirectionMarker를 겹치면 된다. 삼각형은 칸 위쪽으로 배치하고 부모의 Clipping 때문에 잘리지 않도록 한다. 기존 `DirectionLabel0` ~ `DirectionLabel5` 체크 표시는 이제 숨기므로 삭제해도 된다.

Class Defaults에서 설정한다.

- `Fishing|Layout > Visible Direction Slots`: 기본 7. 위젯도 0~6까지 필요하다. 값을 바꾸면 해당 개수만큼 위젯을 직접 준비한다.
- `Current Direction Slot`: 기본 3. **0부터 세므로 화면의 네 번째 칸**이다. 기본 구성은 완료 3칸 / 현재 1칸 / 다음 3칸이다. 미니게임 패널은 원본 그림에 맞춘 885×245이고, 각 칸은 같은 95×95 공간을 사용한다. 시작 시 이전 입력이 없는 왼쪽 세 칸은 비어 있다.
- `Fishing|Art > Up/Left/Down/Right Arrow Brush`: `Arrow_Normal` 이미지.
- `Current Arrow Brushes > Up/Left/Down/Right`: `Arrow_Now` 이미지.
- `Complete Arrow Brushes > Up/Left/Down/Right`: `Arrow_O` 이미지. 완료해도 원래 화살표 방향을 유지한다.
- `Pending/Current/Complete/Wrong Slot Brush`: 각 칸 배경. 대기 배경은 시안의 갈색 테두리로 직접 설정하고, 나머지는 `ArrowBox_Now`, `ArrowBox_O`, `ArrowBox_X`에 연결한다.
- `Current Direction Marker Brush`: `NowArrow` 이미지. 또는 이 설정을 비워 두고 각 DirectionMarker의 Designer Brush에 이미지를 지정한다. 브러시 설정에서 Draw As는 Image를 사용한다.
- `Wrong Input Flash Duration`: 기본 0.2초. 오입력 시 현재 칸만 빨갛게 표시한다. 정답 입력·종료 시 즉시 해제되고, 일시정지 시 표시 시간도 멈춘다. 0이면 표시하지 않는다.

상태별 화살표 이미지가 비어 있으면 일반 화살표를 사용한다. 칸과 화살표의 실제 배치는 Designer가 담당하며 코드는 상태에 따라 브러시와 표시 여부만 변경한다.

## 투척 게이지

`FishCastPanel`(Border) 안에 `CastLabel`(Text Block)과 `FishCastTrack`(Canvas Panel)을 배치한다. CastTrack은 시안처럼 가로로 길게 만든다. 다음 여섯 위젯은 **CastTrack의 직접 자식**이어야 한다.

| 위젯 이름 | 타입 | 역할 |
|---|---|---|
| `CastGreen` | Border | 왼쪽 초록 구간 |
| `CastYellow` | Border | 왼쪽 노랑 구간 |
| `CastRed` | Border | 가운데 빨강 구간 |
| `CastYellowRight` | Border | 오른쪽 노랑 구간, 새로 추가 |
| `CastGreenRight` | Border | 오른쪽 초록 구간, 새로 추가 |
| `CastMarker` | Image | 좌우로 움직이는 파란 막대. 기존 Text Block을 같은 이름의 Image로 교체 |

코드는 다섯 구간의 가로 앵커·여백과 막대의 위치·중앙 정렬을 매번 설정한다. **트랙 전체의 크기와 막대 크기**를 Designer에서 조정한다. 막대 이미지는 `Fish_Casting_Player`를 Designer Brush에 지정하거나 Class Defaults의 `Cast Marker Brush`에 지정한다. 브러시 override를 사용할 때 Draw As는 Image로 설정한다. 막대의 중심이 양끝에 도달하므로 트랙 좌우에 막대 반 너비만큼 여유를 남긴다.

게임 판정과 구간 배치는 `FPSFishingCastBands::GetEdges`를 공유한다. `BP_PlayerController > Fishing|Casting > CastBands`에서 왼쪽부터 다섯 비율을 지정한다. 10월 26일 기본값은 20/15/30/20/15다. 표시와 판정은 준비 시작 때 고정한 동일한 값을 사용한다.

| 가로 위치 | 색상 | 거리 |
|---|---|---:|
| 0~20% | 초록 | 2칸 |
| 20~35% | 노랑 | 4칸 |
| 35~65% | 빨강 | 6칸 |
| 65~85% | 노랑 | 4칸 |
| 85~100% | 초록 | 2칸 |

경계선은 더 높은 등급으로 판정한다. 비율의 합으로 정규화하므로 다섯 값의 합계가 반드시 100일 필요는 없다. 0 이하나 비정상 값은 기획 기본값으로 대체한다.

`Fish_Casting_Background`는 바깥 배경으로 사용할 수 있다. 현재 배치는 `Fish_Casting_Main`을 다섯 구간으로 잘라 각 Border 브러시에 UV 영역으로 지정했다. 코드가 각 구간 너비를 실제 판정값에 맞춰 늘리므로 이미지와 판정이 함께 바뀐다. Main 이미지를 별도의 통짜 Image로 교체하면 **경계값을 나중에 바꿔도 통짜 그림의 경계는 바뀌지 않는다**. 편집 가능한 구성을 원하면 위 다섯 Border로 색 구간을 만들고 바깥 테두리 이미지를 겹친다. 제공된 Main PNG를 그대로 쓰려면 위 구간을 별도로 보이게 겹치지 말고, 이미지의 실제 색 경계와 컨트롤러 설정이 일치하는지 확인한다. 기본 구간은 제공된 이미지의 비율에 가깝게 잡은 값이다.

## 획득 결과

`FishResultPanel`(Border)에 다음 위젯을 배치한다.

- `ResultIcon`(Image): 물고기 아이콘. 아이콘 데이터가 없거나 실패하면 숨긴다.
- `ResultNameLabel`(Text Block): 이름표 위에 배치. 새로 추가한다.
- `ResultSizeLabel`(Text Block): 정보 종이 위에 배치. 새로 추가한다.
- `ResultLabel`(Text Block): 실패 메시지용으로 유지한다.

ResultNameLabel과 ResultSizeLabel을 모두 추가하면 성공 시 기존 ResultLabel은 숨겨 중복 문구를 막는다. 둘을 준비하지 않은 기존 WBP에서는 기존 한 줄 결과를 계속 사용한다. 실패 시 이전 물고기의 이름·크기는 비우고 숨긴다. 배경·이름표·정보 종이와 장식 물고기는 별도 Image로 자유롭게 배치한다.

## 안내와 입질

- `FishNoticePanel`(Border), `NoticeLabel`(Text Block): 낚시 불가 이유. 기존 X 문자를 지우고 `Fish_Ballon_Cant` 이미지를 배치할 수 있다.
- `FishBitePanel`(Border): 입질 때 `Fish_Ballon_Catch` 느낌표 이미지만 표시한다. 2026-11-03 사용자 지시로 `F / 우클릭` 안내와 그 작은 배경은 제거했다. 실제 F/우클릭 챔질 입력은 유지한다. `Scripts/RemoveFishingBiteHint.py`는 해당 안내만 기존 WBP에서 제거하며 다른 이미지와 배치는 유지한다.
- 느낌표 위치는 Class Defaults의 `Fishing|Layout > Bite Offset`으로 조절한다. 기본값은 `(0, -24)`로 찌 바로 위다. 사용 불가 안내의 `Notice Offset`과 별도이며, 작은 화면에서 패널이 잘리지 않도록 화면 가장자리 안쪽으로 보정한다. `Follow Player`가 꺼져 있으면 Designer의 고정 위치를 그대로 사용한다.
- 입질 대기(1~10초)에는 미니게임 패널과 시간 바 전체를 숨긴다. 대기 시간 자체를 없애지는 않는다. 입질 때 말풍선이 나오며 미니게임에 들어가면 시간 바가 다시 표시된다.
- `Rod Notice Brush`와 `Water Notice Brush`에는 Fish_Tbg_Cant1/2를 연결했다. 해당 안내는 완성된 문구 이미지를 보여 주고 NoticeLabel은 숨겨 중복을 막는다. 일반 안내는 빈 종이와 NoticeLabel을 사용한다.
- `Fish_Tbg_Cant1/2`에는 문구가 포함돼 있다. 범용 안내를 편집하려면 글자 없는 배경과 NoticeLabel을 사용한다. 이미 완성된 문구 이미지를 쓸 경우 그 위에 같은 문구를 중복 배치하지 않는다.

## 확인 순서

1. 낚시 시작 후 막대가 좌우 왕복하고, 양끝·중간·중앙에서 각각 2/4/6칸으로 던지는지 확인한다.
2. 미니게임 시작 시 네 번째 칸에 현재 화살표와 삼각형이 나오는지 확인한다.
3. 정답 입력 시 완료 화살표가 왼쪽에 남고, 오답 시 현재 칸만 잠깐 빨갛게 표시되는지 확인한다.
4. 성공 시 이름·크기·아이콘, 실패 시 실패 문구만 보이는지 확인한다.
5. 화면 해상도와 UI 배율을 바꿔 칸 정렬·막대 끝점·말풍선 위치를 확인한다.

## 이번 배치와 이후 수정

- 크기·간격·사진은 WBP Designer의 각 Image, Border, Canvas Slot에서 조절한다. 사진을 바꿀 때 화살표·칸·삼각형·이동 막대는 Class Defaults의 Fishing|Art도 함께 확인한다.
- 성공 화면은 FishResultSuccessContent, 실패 화면은 ResultFailureContent를 펼쳐 편집한다. 실제 물고기 아이콘은 기존 Icon 데이터 테이블의 이미지를 사용하며, 이미지가 없으면 아이콘만 숨긴다.
- 모든 가져온 UI 텍스처는 Texture Group UI, UI용 RGBA 압축, No Mipmaps로 맞췄다. 원본 PNG와 PSD는 수정하지 않는다.
- Scripts/ApplyFishingArtwork.py는 새 배치의 최초 작성용이다. 다시 실행하면 Designer 배치를 교체하므로 직접 수정한 뒤에는 실행하지 않는다. CreateFishingWidget.py와 정상 게임 실행은 편집한 레이아웃을 유지한다.

2026-10-28: 도감은 `WBP_Encyclopedia`에서 편집한다. FishList는 UniformGridPanel, FishStar는 Image로 변경했으며 상세 위젯 이름과 적용 절차는 `EncyclopediaUI.md`를 참고한다. I로 인벤토리를 연 뒤 도감 탭으로 접근하고 I 또는 Esc로 책 전체를 닫는다. F6 진입과 닫기는 제거했다.

패널·컨테이너 이름은 Fish 접두사를 사용한다. FishCastPanel, FishCastTrack, FishMinigamePanel, FishDirectionRow, FishDirectionBox0~6, FishBitePanel, FishNoticePanel, FishResultPanel, FishResultSuccessContent, FishResultFailureContent다. FishingRoot는 이미 Fish로 시작하므로 유지한다. 기존 위젯은 Scripts/PrefixFishingPanelNames.py로 이름만 이관하며 Designer의 배치·이미지·애니메이션은 유지한다.

사용자 최종 확정: 2026-10-27에 7칸·4번째 입력 위치로 지정했다. 기존 기획서의 6칸·3번째 표기보다 이 지시를 우선한다. 기존 배치는 Scripts/UseSevenFishingSlots.py로 마지막 한 칸만 추가한다.
