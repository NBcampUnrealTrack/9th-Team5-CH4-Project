#include "DRPlayerNameplateComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "MVVMSubsystem.h"
#include "TimerManager.h"
#include "View/MVVMView.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/UI/ViewModel/DRPlayerNameplateViewModel.h"
#include "DeepRaiders/UI/Nameplate/DRPlayerNameplateWidget.h"

UDRPlayerNameplateComponent::UDRPlayerNameplateComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetPivot(FVector2D(0.5f, 1.f));

	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	SetVisibility(false);
}

void UDRPlayerNameplateComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();

	if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		SetVisibility(false);
		return;
	}

	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(TargetCharacter))
	{
		SetVisibility(false);
		return;
	}

	/*
	 * 스켈레톤 Bone에 의존하지 않고
	 * Capsule 높이를 기준으로 이름표를 배치한다.
	 */
	if (const UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent())
	{
		SetRelativeLocation(FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + HeightPadding));
	}

	InitWidget();

	UDRPlayerNameplateWidget* NameplateWidget = Cast<UDRPlayerNameplateWidget>(GetUserWidgetObject());

	if (!IsValid(NameplateWidget))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Nameplate] Widget must derive from "
				"UDRPlayerNameplateWidget. Character=%s Widget=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetUserWidgetObject()));

		return;
	}

	NameplateViewModel = NewObject<UDRPlayerNameplateViewModel>(this);

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(NameplateWidget);

	if (!IsValid(View) || !View->SetViewModel(ViewModelName, NameplateViewModel))
	{
		UE_LOG(LogTemp, Error, TEXT( "[Nameplate] ViewModel registration " "failed. Widget=%s Name=%s"), *GetNameSafe(NameplateWidget), *ViewModelName.ToString());

		NameplateViewModel = nullptr;
		return;
	}

	RefreshDisplayData();
	RefreshVisibility();

	World->GetTimerManager().SetTimer(VisibilityRefreshTimer, this, &ThisClass::RefreshVisibility, FMath::Max(VisibilityRefreshInterval, 0.02f), true);
}

void UDRPlayerNameplateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VisibilityRefreshTimer);
	}

	NameplateViewModel = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UDRPlayerNameplateComponent::RefreshDisplayData()
{
	if (!IsValid(NameplateViewModel))
	{
		return;
	}

	const ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(GetOwner());

	const ADRPlayerState* TargetPlayerState = IsValid(TargetCharacter) ? TargetCharacter->GetPlayerState<ADRPlayerState>() : nullptr;

	if (!IsValid(TargetCharacter) || !IsValid(TargetPlayerState))
	{
		return;
	}

	/*
	 * 현재는 APlayerState 기본 PlayerName 사용.
	 * 계정 닉네임이 별도 데이터라면 이 한 줄만 교체하면 된다.
	 */
	const FText DisplayName = FText::FromString(TargetPlayerState->GetPlayerName());

	NameplateViewModel->SetDisplayData(DisplayName, TargetCharacter->GetTeamDisplayColor());
}

void UDRPlayerNameplateComponent::RefreshVisibility()
{
	UWorld* World = GetWorld();

	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(GetOwner());

	ADRPlayerController* LocalController = IsValid(World) ? Cast<ADRPlayerController>(World->GetFirstPlayerController()) : nullptr;

	ADRPlayerState* LocalPlayerState = IsValid(LocalController) ? LocalController->GetPlayerState<ADRPlayerState>() : nullptr;

	ADRPlayerState* TargetPlayerState = IsValid(TargetCharacter) ? TargetCharacter->GetPlayerState<ADRPlayerState>() : nullptr;

	if (!IsValid(LocalController) || !LocalController->IsLocalController() || !IsValid(LocalPlayerState) || !IsValid(TargetCharacter) || !IsValid(TargetPlayerState) || LocalController->GetPawn() == TargetCharacter || TargetCharacter->IsDead())
	{
		SetVisibility(false);
		return;
	}

	RefreshDisplayData();

	const bool bSameTeam = LocalPlayerState->GetTeamId() == TargetPlayerState->GetTeamId();

	const bool bEligible = bSameTeam || LocalController->IsEnemyNameRevealActive(TargetPlayerState);

	/*
	 * 표시 대상도 아닌 적에게는
	 * LOS Trace를 수행하지 않는다.
	 */
	if (!bEligible)
	{
		SetVisibility(false);
		return;
	}

	const bool bHasLineOfSight = HasNameplateLineOfSight(LocalController, TargetCharacter);

	SetVisibility(bHasLineOfSight);
}

bool UDRPlayerNameplateComponent::HasNameplateLineOfSight(const ADRPlayerController* LocalController, const ADRPlayerCharacter* TargetCharacter) const
{
	if (!IsValid(LocalController) || !IsValid(TargetCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	const UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent();

	if (!IsValid(World) || !IsValid(Capsule))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;

	LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector CapsuleCenter = Capsule->GetComponentLocation();

	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	const FVector HeadPoint = CapsuleCenter + FVector::UpVector * CapsuleHalfHeight * FMath::Clamp(HeadPointRatio, 0.f, 1.f);

	const FVector ChestPoint = CapsuleCenter + FVector::UpVector * CapsuleHalfHeight * FMath::Clamp(ChestPointRatio, 0.f, 1.f);

	const FVector ToTarget = (HeadPoint - ViewLocation).GetSafeNormal();

	/*
	 * 카메라 뒤쪽 대상은 Trace 전에 제외한다.
	 */
	if (ToTarget.IsNearlyZero() || FVector::DotProduct(ViewRotation.Vector(), ToTarget) <= 0.f)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRPlayerNameplateVisibility), false);

	QueryParams.AddIgnoredActor(LocalController->GetPawn());

	QueryParams.AddIgnoredActor(TargetCharacter);

	/*
	 * 머리 또는 가슴 중 하나라도 노출되면 표시한다.
	 */
	return IsTracePointVisible(ViewLocation, HeadPoint, QueryParams) || IsTracePointVisible(ViewLocation, ChestPoint, QueryParams);
}

bool UDRPlayerNameplateComponent::IsTracePointVisible(const FVector& ViewLocation, const FVector& TargetPoint, const FCollisionQueryParams& QueryParams) const
{
	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	const bool bBlocked = World->LineTraceTestByChannel(ViewLocation, TargetPoint, OcclusionTraceChannel, QueryParams);

	return !bBlocked;
}
