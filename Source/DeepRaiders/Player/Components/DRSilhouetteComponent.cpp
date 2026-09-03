#include "DRSilhouetteComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRSilhouetteComponent::UDRSilhouetteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UDRSilhouetteComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshTeamSilhouette();
}

void UDRSilhouetteComponent::RefreshTeamSilhouette()
{
	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());
	const APlayerController* LocalController = GetWorld()->GetFirstPlayerController();
	const ADRPlayerState* LocalPlayerState =
		IsValid(LocalController) ? LocalController->GetPlayerState<ADRPlayerState>() : nullptr;
	const ADRPlayerState* TargetPlayerState =
		IsValid(Character) ? Character->GetPlayerState<ADRPlayerState>() : nullptr;

	if (!IsValid(Character) || !IsValid(LocalPlayerState) || !IsValid(TargetPlayerState))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	UStaticMeshComponent* EquipmentMesh = Character->GetWorldHandEquipmentMesh();
	if (!IsValid(CharacterMesh) || !IsValid(EquipmentMesh))
	{
		return;
	}

	const bool IsLocalPlayer = Character->IsLocallyControlled();
	const bool IsTeammate =
		!IsLocalPlayer &&
		LocalPlayerState->GetTeamId() == TargetPlayerState->GetTeamId();

	CharacterMesh->SetCustomDepthStencilValue(IsTeammate ? TeamStencilValue : 0);
	CharacterMesh->SetRenderCustomDepth(IsTeammate);

	const int32 EquipmentStencilValue =
		IsLocalPlayer ? BlockerStencilValue : (IsTeammate ? TeamStencilValue : 0);
	EquipmentMesh->SetCustomDepthStencilValue(EquipmentStencilValue);
	EquipmentMesh->SetRenderCustomDepth(IsLocalPlayer || IsTeammate);

	if (!SearchRevealRequests.IsEmpty())
	{
		ApplySearchReveal();
	}
}

void UDRSilhouetteComponent::StartSearchReveal(
	FGuid RevealId,
	float Duration,
	int32 StencilValue)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !RevealId.IsValid() || Duration <= 0.0f)
	{
		return;
	}

	if (SearchRevealRequests.IsEmpty())
	{
		CaptureSearchRevealState();
	}

	FDRSearchRevealRequest& Request = SearchRevealRequests.FindOrAdd(RevealId);
	Request.EndTime = FMath::Max(Request.EndTime, World->GetTimeSeconds() + Duration);
	Request.StencilValue = StencilValue;
	ApplySearchReveal();
	RefreshSearchRevealTimer();
}

void UDRSilhouetteComponent::StopSearchReveal(FGuid RevealId)
{
	if (!RevealId.IsValid() || SearchRevealRequests.Remove(RevealId) == 0)
	{
		return;
	}

	if (SearchRevealRequests.IsEmpty())
	{
		RestoreSearchRevealState();
		return;
	}

	ApplySearchReveal();
	RefreshSearchRevealTimer();
}

void UDRSilhouetteComponent::CaptureSearchRevealState()
{
	SearchRevealStates.Reset();
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetOwner()->GetComponents(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		FDRSearchRevealState& State = SearchRevealStates.AddDefaulted_GetRef();
		State.Component = PrimitiveComponent;
		State.IsRenderCustomDepthEnabled = PrimitiveComponent->bRenderCustomDepth;
		State.StencilValue = PrimitiveComponent->CustomDepthStencilValue;
	}
}

void UDRSilhouetteComponent::ApplySearchReveal()
{
	if (SearchRevealRequests.IsEmpty())
	{
		return;
	}

	const int32 SearchStencilValue = SearchRevealRequests.CreateConstIterator().Value().StencilValue;
	for (const FDRSearchRevealState& State : SearchRevealStates)
	{
		UPrimitiveComponent* PrimitiveComponent = State.Component.Get();
		if (IsValid(PrimitiveComponent))
		{
			PrimitiveComponent->SetCustomDepthStencilValue(SearchStencilValue);
			PrimitiveComponent->SetRenderCustomDepth(true);
		}
	}
}

void UDRSilhouetteComponent::RestoreSearchRevealState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SearchRevealTimerHandle);
	}

	for (const FDRSearchRevealState& State : SearchRevealStates)
	{
		UPrimitiveComponent* PrimitiveComponent = State.Component.Get();
		if (IsValid(PrimitiveComponent))
		{
			PrimitiveComponent->SetCustomDepthStencilValue(State.StencilValue);
			PrimitiveComponent->SetRenderCustomDepth(State.IsRenderCustomDepthEnabled);
		}
	}

	SearchRevealStates.Reset();
	SearchRevealTimerHandle.Invalidate();
}

void UDRSilhouetteComponent::RefreshSearchRevealTimer()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || SearchRevealRequests.IsEmpty())
	{
		return;
	}

	double NextEndTime = TNumericLimits<double>::Max();
	for (const TPair<FGuid, FDRSearchRevealRequest>& Request : SearchRevealRequests)
	{
		NextEndTime = FMath::Min(NextEndTime, Request.Value.EndTime);
	}

	World->GetTimerManager().SetTimer(
		SearchRevealTimerHandle,
		this,
		&ThisClass::HandleSearchRevealExpired,
		FMath::Max(static_cast<float>(NextEndTime - World->GetTimeSeconds()), KINDA_SMALL_NUMBER),
		false);
}

void UDRSilhouetteComponent::HandleSearchRevealExpired()
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	for (auto Iterator = SearchRevealRequests.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value().EndTime <= CurrentTime)
		{
			Iterator.RemoveCurrent();
		}
	}

	if (SearchRevealRequests.IsEmpty())
	{
		RestoreSearchRevealState();
		return;
	}

	ApplySearchReveal();
	RefreshSearchRevealTimer();
}
