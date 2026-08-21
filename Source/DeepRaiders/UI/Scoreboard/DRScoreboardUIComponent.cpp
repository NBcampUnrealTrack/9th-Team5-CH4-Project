#include "DRScoreboardUIComponent.h"

#include "Engine/LocalPlayer.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Scoreboard/DRScoreboardWidget.h"

UDRScoreboardUIComponent::UDRScoreboardUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRScoreboardUIComponent::BeginPlay()
{
	Super::BeginPlay();

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsValid(PlayerController))
	{
		return;
	}

	/*
	 * Dedicated Server의 PlayerController에서도
	 * Component BeginPlay는 돌 수 있다.
	 *
	 * UI는 Local Controller에서만 생성.
	 */
	if (!PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();

	if (!IsValid(UIManager))
	{
		return;
	}

	/*
	 * DA_UIConfig의
	 * UI.Screen.Scoreboard에 등록된 Widget 생성.
	 */
	ScoreboardWidget = Cast<UDRScoreboardWidget>(UIManager->PushScreen(DRGameplayTags::UI_Screen_Scoreboard));

	if (!IsValid(ScoreboardWidget))
	{
		UE_LOG(LogTemp, Error, TEXT( "Failed to create Scoreboard screen."));

		return;
	}

	/*
	 * Widget이 자기 ViewModel을 만든다.
	 */
	ScoreboardWidget->InitializeScoreboard(PlayerController);

	/*
	 * Widget은 미리 만들어두되
	 * 처음에는 숨긴다.
	 */
	UIManager->SetManagedWidgetVisible(ScoreboardWidget, false);
}

void UDRScoreboardUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(UIManager))
	{
		UIManager->PopScreen(DRGameplayTags::UI_Screen_Scoreboard);
	}
	else if (IsValid(ScoreboardWidget))
	{
		ScoreboardWidget->RemoveFromParent();
	}

	/*
	 * Widget의 NativeDestruct에서
	 * ScoreboardViewModel::Deinitialize()가 호출된다.
	 *
	 * 따라서 UIComponent는 ViewModel을 만지지 않는다.
	 */
	ScoreboardWidget = nullptr;
	UIManager = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UDRScoreboardUIComponent::ShowScoreboard()
{
	if (!IsValid(UIManager) || !IsValid(ScoreboardWidget))
	{
		return;
	}

	/*
	 * 열 때 현재 PlayerArray를 다시 읽는다.
	 *
	 * 중간 입장/퇴장 반영.
	 */
	ScoreboardWidget->RefreshPlayers();

	UIManager->SetManagedWidgetVisible(ScoreboardWidget, true);
}

void UDRScoreboardUIComponent::HideScoreboard()
{
	if (!IsValid(UIManager) || !IsValid(ScoreboardWidget))
	{
		return;
	}

	UIManager->SetManagedWidgetVisible(ScoreboardWidget, false);
}

bool UDRScoreboardUIComponent::IsScoreboardVisible() const
{
	return IsValid(UIManager) && UIManager->IsScreenOpen(DRGameplayTags::UI_Screen_Scoreboard);
}
