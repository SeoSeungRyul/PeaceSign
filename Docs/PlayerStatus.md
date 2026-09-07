# 플레이어 상태 시스템 구현

## 파일별 역할

- `Source/PeaceSign/PSPlayerStatsComponent.*`: HP와 SP 변경, 대기 중 SP 회복, 사망 알림, 생존 관련 수치를 관리합니다.
- `Source/PeaceSign/UI/PSPlayerStatusWidget.*`: 상태 변경 이벤트에 따라 텍스트와 게이지를 갱신합니다.
- `Content/UI/WBP_PlayerStatus.uasset`: C++ 위젯 클래스를 부모로 사용하는 위젯 블루프린트이며 `BP_PlayerController`에 지정되어 있습니다.
- `Scripts/CreatePlayerStatusWidget.py`: 위젯 에셋을 생성하고 컨트롤러에 지정하는 에디터 스크립트입니다.

기본 레이아웃은 실행 중 C++에서 생성됩니다. 위젯 블루프린트 디자이너에서 레이아웃을 교체하려면 `HealthBar`, `StaminaBar`, `HungerBar`, `MentalBar`라는 이름의 ProgressBar 4개와 `HealthLabel`, `StaminaLabel`, `HungerLabel`, `MentalLabel`이라는 이름의 TextBlock 4개를 배치해야 합니다. 필요한 위젯이 하나라도 없으면 C++ 기본 레이아웃을 사용합니다. 현재 상태창의 위치와 크기는 `PSPlayerController`에서 설정합니다.

## 액션 연동

이동하면서 Shift를 누르면 달리며 초당 SP 5를 소모합니다. 왼쪽 또는 오른쪽 Ctrl을 누르면 SP 10을 소모하고 현재 입력 중인 방향으로 구릅니다. 이동 입력이 없으면 마지막으로 이동했던 방향을 사용합니다. 임시 구르기는 0.3초 동안 450유닛을 이동합니다. 구르는 동안 캐릭터 캡슐 충돌을 끄고 일반 이동과 SP 회복을 막으며, 구르기가 끝나면 이전 충돌 설정을 복구합니다. 구르기 거리와 시간, 달리기 및 구르기 SP 소모량은 캐릭터 블루프린트에서 수정할 수 있습니다.

HP 피해는 캐릭터의 `TakeDamage` 재정의 함수에 연결되어 있습니다. `RestoreHealth`는 살아 있는 캐릭터의 HP를 회복합니다. `OnHealthDepleted`는 이후 구현될 사망 및 부활 시스템에 HP 소진을 알리고, `ResetStats`는 모든 수치를 명시적으로 복구합니다. 인벤토리 아이템 드롭, 돈 감소, 부활, 사망 시 이동 제한은 아직 구현하지 않았습니다.

캐릭터가 살아 있고 움직이거나 행동하지 않을 때 SP가 초당 10씩 연속으로 회복됩니다. 현재 캐릭터는 위치를 직접 변경하는 방식이라 속도만으로 이동 여부를 확인할 수 없습니다. 따라서 WASD 입력도 함께 확인합니다. 이후 이동 또는 입력 방식을 변경하면 이 판정도 함께 수정해야 합니다.

## 결정이 필요한 사항

배고픔과 정신력은 임시로 0부터 100까지의 표시 범위를 사용합니다. 자동 감소, 회복 수단, 최대 수치 감소 페널티와 페널티 중첩 방식은 추후 결정해야 합니다. 현재 생존 관련 페널티는 적용되지 않습니다. 상태 수치는 저장하거나 네트워크로 복제하지 않습니다.

## 검증

UE 5.8에서 `PeaceSignEditor Win64 Development` 구성을 빌드하고 `PeaceSign.` 자동화 테스트 그룹을 실행합니다. `PSPlayerStatsTests`는 SP 부족 및 음수 비용 처리, SP 소모 프레임과 행동 중 회복 차단, 회복 속도, HP 회복 상한, 치명적인 피해, 명시적 부활, 생존 수치 범위를 검사합니다. 기존 그리드 테스트는 회귀 여부를 확인합니다. NullRHI 자동화 테스트에서는 PIE 화면의 실제 위젯 배치를 검증하지 않습니다.
