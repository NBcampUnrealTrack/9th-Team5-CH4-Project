#include "DRSilhouetteComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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
}
