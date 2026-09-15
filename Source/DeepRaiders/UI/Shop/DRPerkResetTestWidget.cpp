#include "DRPerkResetTestWidget.h"

#include "Components/Button.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"

void UDRPerkResetTestWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯이 화면에 생성될 때 버튼 이벤트를 연결한다.
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleResetButtonClicked);
	}
}

void UDRPerkResetTestWidget::NativeDestruct()
{
	// 재사용 시 중복 호출되지 않도록 이벤트 연결을 해제한다.
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleResetButtonClicked);
	}

	Super::NativeDestruct();
}

void UDRPerkResetTestWidget::HandleResetButtonClicked()
{
	// 로컬 PlayerState의 PerkComponent를 통해 서버 초기화를 요청한다.
	ADRPlayerState* PlayerState = GetOwningPlayerState<ADRPlayerState>();
	UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;

	if (IsValid(PerkComponent))
	{
		PerkComponent->RequestResetPerks();
	}
}
