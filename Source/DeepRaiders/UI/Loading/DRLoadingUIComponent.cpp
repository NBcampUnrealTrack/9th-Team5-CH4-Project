#include "DRLoadingUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/Components/DRSnowJoinComponent.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/ViewModel/DRLoadingViewModel.h"
#include "Engine/LocalPlayer.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

UDRLoadingUIComponent::UDRLoadingUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UDRLoadingUIComponent::BeginPlay()
{
	Super::BeginPlay();
	const ADRPlayerController* PC = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	AddTickPrerequisiteActor(GetOwner());
	AddTickPrerequisiteComponent(PC->GetSnowJoinComponent());
	SetComponentTickEnabled(true);
	RefreshLoadingScreen();
}

void UDRLoadingUIComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshLoadingScreen();
}

void UDRLoadingUIComponent::RefreshLoadingScreen()
{
	const ADRPlayerController* PC = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		CloseLoadingScreen();
		return;
	}

	const UDRSnowJoinComponent* SnowJoin = PC->GetSnowJoinComponent();
	const EDRSnowJoinLoadingPhase Phase = SnowJoin->GetSnowJoinLoadingPhase();
	// 서버의 checkpoint 생성/압축이 끝나기 전에도 씬 진입 대기 화면을 보여준다.
	// 호스트/독립 실행에는 적용하지 않고 최초 로컬 조종 준비까지만 유지한다.
	const APawn* Pawn = PC->GetPawn();
	const bool bWaitingForInitialControl = GetNetMode() == NM_Client
		&& Phase == EDRSnowJoinLoadingPhase::Idle
		&& (PC->GetStateName() != NAME_Playing || !IsValid(Pawn) || !Pawn->IsLocallyControlled());
	if ((Phase == EDRSnowJoinLoadingPhase::Idle && !bWaitingForInitialControl)
		|| Phase == EDRSnowJoinLoadingPhase::Complete
		|| Phase == EDRSnowJoinLoadingPhase::Failed)
	{
		CloseLoadingScreen();
		bReportedSetupError = false;
		return;
	}

	// LocalPlayer/Configure가 늦게 준비되는 경우 다음 갱신에서 다시 시도한다.
	if (!IsValid(UIManager))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
		}
	}
	const UDRUIConfig* Config = IsValid(UIManager) ? UIManager->GetUIConfig() : nullptr;
	if (!IsValid(Config))
	{
		return;
	}
	const FDRUIScreenDefinition* Definition = Config->FindScreen(DRGameplayTags::UI_Screen_Loading);
	if (!Definition || !Definition->WidgetClass)
	{
		if (!bReportedSetupError)
		{
			UE_LOG(LogTemp, Error, TEXT("Configure UI.Screen.Loading in DA_UIConfig."));
			bReportedSetupError = true;
		}
		return;
	}

	if (!IsValid(LoadingViewModel))
	{
		LoadingViewModel = NewObject<UDRLoadingViewModel>(this);
	}
	LoadingViewModel->SetProgress(bWaitingForInitialControl ? 0.f : SnowJoin->GetSnowJoinSnapshotProgress());
	switch (Phase)
	{
	case EDRSnowJoinLoadingPhase::Idle:
		LoadingViewModel->SetLoadingMessage(TEXT("서버 동기화를 준비하는 중..."));
		break;
	case EDRSnowJoinLoadingPhase::ReceivingSnapshot:
		LoadingViewModel->SetLoadingMessage(TEXT("지형 데이터를 받는 중..."));
		break;
	case EDRSnowJoinLoadingPhase::ApplyingSnapshot:
		LoadingViewModel->SetLoadingMessage(TEXT("지형 데이터를 적용하는 중..."));
		break;
	case EDRSnowJoinLoadingPhase::WaitingForControl:
		LoadingViewModel->SetLoadingMessage(TEXT("플레이어 준비를 기다리는 중..."));
		break;
	default:
		break;
	}

	if (!IsValid(LoadingWidget) || UIManager->GetScreen(DRGameplayTags::UI_Screen_Loading) != LoadingWidget)
	{
		LoadingWidget = UIManager->PushScreen(DRGameplayTags::UI_Screen_Loading);
		bViewModelBound = false;
		if (!IsValid(LoadingWidget))
		{
			return;
		}
		UIManager->RegisterCloseHandler(LoadingWidget,
			FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseRequested));
	}
	if (!bViewModelBound)
	{
		UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(LoadingWidget);
		bViewModelBound = View && View->SetViewModel(TEXT("DRLoadingViewModel"), LoadingViewModel);
		if (!bViewModelBound && !bReportedSetupError)
		{
			UE_LOG(LogTemp, Error, TEXT("Loading widget requires a Manual MVVM source named DRLoadingViewModel."));
			bReportedSetupError = true;
		}
	}
}

void UDRLoadingUIComponent::HandleCloseRequested()
{
	// ESC/PopTopScreen은 소비한다. 로딩 상태 종료 시에만 PopScreen으로 닫는다.
}

void UDRLoadingUIComponent::CloseLoadingScreen()
{
	if (IsValid(LoadingWidget))
	{
		if (IsValid(UIManager))
		{
			// 맵 전환 후 새 컨트롤러가 만든 화면을 이전 컴포넌트가 닫지 않는다.
			UIManager->UnregisterCloseHandler(LoadingWidget);
			if (UIManager->GetScreen(DRGameplayTags::UI_Screen_Loading) == LoadingWidget)
			{
				UIManager->PopScreen(DRGameplayTags::UI_Screen_Loading);
			}
		}
		else
		{
			LoadingWidget->RemoveFromParent();
		}
	}
	LoadingWidget = nullptr;
	LoadingViewModel = nullptr;
	bViewModelBound = false;
}

void UDRLoadingUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);
	CloseLoadingScreen();
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}
