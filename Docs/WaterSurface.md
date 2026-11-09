# 물 표면 애니메이션

이 문서는 첫 번째 물 머티리얼 방식이다. 현재 월드의 기본값은 두 번째 네이티브 방식이며,
GridWorld에서 Use Native Water를 끄고 Refresh Water Rendering을 누르면 이 효과로 복귀한다.

기존 물 Set1~12 연결·회전·그리드 판정은 그대로 유지한다.
`MI_WaterSurface` → `M_WaterSurface`를 물 HISM에만 사용한다.
흙·풀·밭 머티리얼과 낚시/충돌 데이터는 바꾸지 않는다.

텍스처 UV와 메시 위치를 움직이지 않고, 밝은 청록색 물 픽셀에만
월드 좌표 + Time 기반의 잔물결 명암과 작은 반짝임을 더한다.
잔물결 위상은 월드 좌표를 사용해서 타일/청크마다 재시작되지 않는다.
밝고 어두운 물결과 반짝임 모두 월드 +X 방향으로 함께 이동한다.
기본 탑다운 카메라에서는 화면 아래→위이며 카메라를 회전하면 화면상 방향은 달라진다.
풀 및 어두운 파란 해안 그림자는 색상 마스크로 제외한다.
새 그림의 색상이 바뀌면 마스크도 재검증해야 한다. 이는 물리 파동이나 Niagara가 아니다.

콘텐츠 브라우저 `/Game/Art/Tiles/Water/MI_WaterSurface`에서 Scalar Parameter를 조절한다:

- RippleStrength: 잔물결 강도, 현재 0.28 (0으로 끄기)
- RippleSpeed: 속도, 현재 1.6 (0으로 정지)
- RippleScale: 월드 공간 빈도, 현재 0.05 (작을수록 큰 물결)
- ShimmerStrength: 반짝임 강도, 현재 0.14 (0으로 끄기)

시인성을 높이기 위해 초기 강도 0.08/속도 0.7에서 위 값으로 조정했다.
작고 드문 점 반짝임을 대신해 휘어진 밝은 물결 선이 이동하도록 변경했다.
원본 그림과 풀/해안 색상 마스크, 타일 실루엣은 그대로다.

RippleStrength와 ShimmerStrength를 모두 0으로 하면 원본 물 그림으로 돌아간다.
CPU Tick, Niagara 액터, 실시간 RenderTarget을 추가하지 않는다.
에셋을 못 찾으면 기존 정적 물 머티리얼로 대체한다.

`Scripts/CreateWaterSurfaceMaterial.py`는 최초 에셋 생성용으로 기존 에셋을 덮어쓰지 않는다.
현재 방식은 물 영역의 실루엣을 그대로 보존하지만 GPU 색/반짝임 효과 자체는
기존 원본 텍스처 기반 경계 검사와 별도로 실제 렌더링 확인이 필요하다.

## 검증

빌드 및 관련 자동 테스트 20개 성공. `-RenderOffscreen -ExportWaterSurfaceGPU`로
실제 GPU의 둥근 해안 타일을 효과 끔/강조 강도 상태로 비교했다.
변한 물 픽셀 27,203개, 변한 풀 0/30,206개, 변한 어두운 그림자 0/2,956개였다.
캡처 `Saved/TileDiagnostics/WaterSurfaceBefore.png`, `WaterSurfaceAfter.png`는
GPU 검사용 강도 RippleStrength=0.25, ShimmerStrength=0.1을 사용한다.
현재 플레이 강도는 위 표처럼 이보다 조금 높다. 위 픽셀 수는 최초 구현 시점의 기록이다.
이 GPU 검사는 효과 마스크 범위 검증이며 시간 경과 영상이나 모든 해안 타일의 GPU 전수 검사는 아니다.
기본 원본 텍스처 연결 전수 검사는 흙/물 각각 8,192개 배치에서 큰 불일치 0개였다.
