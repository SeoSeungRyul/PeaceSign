# 링크 방식 물 비교 장면

Water 플러그인의 실제 `Water Body Custom` + `Water Waves Asset`을 비교하는 장면이다.
현재는 GridWorld에도 같은 방식을 연결했다. 기존 저장과 기존 물 머티리얼은 보존한다.
월드 설정/복귀 방법은 `Docs/NativeWorldWater.md`를 참고한다.

## 열어서 비교

1. 빌드 후 에디터를 다시 열고 `/Game/Maps/WaterLinkComparison` 맵을 연다.
2. 플레이하거나 뷰포트 Realtime을 켠다.
3. `Legacy_Water`는 기존 타일 효과, `Link_Water`는 네이티브 물이다.
4. `Link_Water` 선택 → Water Comparison → **Use Link Water** 체크를 끄면 기존 방식으로 복귀한다.
5. 설정 변경 후 **Refresh Comparison**을 누른다. Preview Status에 준비 상태/높이 제한이 표시된다.

물리 파도 조절: `/Game/Art/Tiles/Water/LinkPrototype/WW_LinkWater`를 열어
Waves Source → Gerstner Wave Generator의 Min/Max Amplitude, Min/Max Wavelength,
Num Waves 등을 조절한다. 이 장면의 기본값은 8개, 파장 60~200cm, 진폭 0.3~3cm다.
파도 에셋 수정 후 비교 액터의 Refresh Comparison을 누른다.
물 표면 색/반사: 같은 폴더의 `MI_LinkWater` (부모는 엔진 Water_Material_CustomMesh).
기존 물 효과: 원래의 `/Game/Art/Tiles/Water/MI_WaterSurface`를 그대로 보존한다.

## 실험 구조와 제한

- 5×5 잔디 영역 안에 Set3의 3×3 해안 모양을 고정 배치했다.
- 기존 텍스처를 덮어쓰지 않고 별도 `M_LinkWaterShore`로 밝은 물 부분만 구멍을 낸다.
- 그 아래 실제 Water Body Custom의 세분화된 메시를 둔다.
- 해안 그림은 물 높이보다 20cm 높게 고정되어 작은 파도가 풀을 덮지 않는다.
- 합산 최대 파고가 18cm 이상이면 안전하게 기존 물로 대체한다. 상태 문구를 확인한다.
- 색상 기반 컷아웃이라 원본의 어두운 물 무늬 일부가 테두리와 같이 남을 수 있다.
- 이 비교 액터 자체는 고정 연못이다. 실제 월드에는 별도로 청크별 자동 표면과 찌 높이 표시를 연결했다.
- 물-흙 전용 접합 아트는 아직 별도 작업이다.
- 비교 액터의 충돌은 꺼져 있다. 기존 게임의 물 충돌·낚시 데이터는 변경하지 않았다.
- Water는 엔진 실험적 플러그인이며 활성화 시 Landmass 등 의존 플러그인도 로드한다.

새 지도 생성 스크립트: `Scripts/CreateLinkWaterMap.py`.
새 에셋 생성 스크립트: `Scripts/CreateLinkWaterAssets.py` (기존 파일 덮어쓰기 방지).

## 검증

- 빌드 성공. 네이티브 파도의 시간 0/0.7초 높이 샘플 차이는 최대 약 12.3cm였다.
- 실제 게임 뷰포트의 새 물 내부를 두 시점에 비교해 5,041픽셀 중 4,037픽셀이 달라졌으며 검증이 통과했다.
- `Saved/LinkWaterSmokeUser/Saved/TileDiagnostics/LinkViewport0.png`, `LinkViewport1.png`에 전체 비교 화면을 저장했다.
- 일반 SceneCapture는 Water 전용 view 데이터를 자동으로 받지 않아 파도 애니메이션 검증에 사용하지 않는다.
- `PS.Water.LinkSmoke`는 이 비교 맵의 실제 게임 창에서 실행하는 개발용 검사이며 완료 후 프로세스를 종료한다.
- 검증은 별도 사용자 저장 경로를 사용했고, 기존 테스트 맵과 실제 플레이 세이브는 바꾸지 않았다.
