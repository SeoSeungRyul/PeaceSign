# 공통 UI 글꼴

2026-10-28 사용자 지정: 일반 게임 UI 텍스트는 나눔스퀘어라운드 Regular를 사용한다. 글자가 포함된 UI 이미지는 원본 표시를 유지하며 폰트 통일 대상에서 제외한다.

- Font 에셋: `/Game/UI/Font/NanumFontSetup_TTF_SQUARE_ROUND/NanumSquareRoundR_Font`
- 공통 코드: `UPSUIWidget`에서 에셋을 하드 참조한다. 패키징에서도 Font와 Font Face가 포함되도록 참조를 유지한다.
- 상태창, 계절/시각, 낚시, 도감, 인벤토리, 단축바는 공통 UI 부모를 사용한다. PreConstruct에서 중첩 WBP까지 글꼴을 맞춘다. 크기, 색, 자간, 외곽선, 레이아웃은 유지한다.
- 단축바 키와 아이템 수량처럼 Slate로 직접 그리는 글씨도 `MakeProjectFont`를 사용한다. 새 도감 항목처럼 실행 중 추가하는 텍스트는 생성 시 `WithProjectFont`를 적용한다. 매 프레임 전체 위젯을 순회하지 않는다.
- 단축바·가방의 아이템 이름/빈 슬롯/잠금 슬롯 툴팁도 공통 글꼴을 사용한다. `SetProjectToolTipText`는 기존 Slate 툴팁의 배경·여백·글자 크기를 유지하고 글꼴만 변경한다. Slate 재생성 시에도 다시 적용하며, 이미지 등이 들어간 커스텀 툴팁 위젯은 교체하지 않는다.
- `Scripts/ApplyProjectUIFont.py`는 `/Game/UI`의 기존 WBP에 Font 설정만 저장한다. 위젯 트리와 위치, 이름, 이미지, 바인딩을 재생성하지 않는다. 이후 추가하는 게임 WBP도 `PSUIWidget` 또는 기존 게임 UI 클래스를 부모로 삼으면 공통 글꼴을 사용한다.
- 낚시 불가 안내의 Rod/Water 배너는 글자·밑줄·물방울 등 원본 이미지 전체를 그대로 표시한다. 별도 TextBlock은 숨겨 글자가 중복되지 않게 한다. 일반 안내처럼 이미지에 글자가 없는 경우에만 TextBlock과 공통 폰트를 사용한다. X 위치와 표시 시간은 유지한다. 글자가 있는 UI 이미지를 빈 배경과 텍스트로 대체하지 않는다.
- 인벤토리의 글씨 포함 이미지도 같은 예외다. Font 적용은 TextBlock과 코드로 그리는 일반 글자에만 한정한다. 이미지 파일, UImage 브러시 참조, 이미지가 포함된 커스텀 툴팁은 유지한다. 이미 공통 글꼴인 WBP는 적용 스크립트가 다시 저장하지 않는다.

글꼴 교체가 필요하면 `PSUIWidget.cpp`의 공통 Font 에셋 참조를 바꾸고 Designer 적용 스크립트를 실행한다. 글자 크기와 색상은 각 WBP에서 수정한다. 에디터 메뉴, 콘솔, 엔진의 디버그 출력과 타일 편집 도구의 월드 라벨은 게임 UMG UI와 별개다.

`PeaceSign.UI.ProjectFont`는 에셋 형식, Slate 기본 글꼴 교체, 크기/자간/외곽선 유지, 중첩 위젯의 런타임 적용과 기존 WBP의 저장된 Font를 검사한다.
