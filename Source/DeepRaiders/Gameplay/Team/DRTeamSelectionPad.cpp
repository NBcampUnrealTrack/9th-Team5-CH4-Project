#include "DRTeamSelectionPad.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ADRTeamSelectionPad::ADRTeamSelectionPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(SceneRoot);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
}

void ADRTeamSelectionPad::BeginPlay()
{
	Super::BeginPlay();
	RefreshTeamColor();
}

void ADRTeamSelectionPad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, TeamId);
}

void ADRTeamSelectionPad::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	TeamId = FMath::Clamp(NewTeamId, 0, 1);
	RefreshTeamColor();
	ForceNetUpdate();
}

void ADRTeamSelectionPad::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	const ADRMiningGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>();
	if (IsValid(GameMode) && GameMode->IsGameStarted())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	ADRPlayerState* PlayerState = IsValid(Pawn)
		? Pawn->GetPlayerState<ADRPlayerState>()
		: nullptr;
	if (IsValid(PlayerState))
	{
		PlayerState->SetTeamId(TeamId);
	}
}

void ADRTeamSelectionPad::OnRep_TeamId()
{
	RefreshTeamColor();
}

void ADRTeamSelectionPad::RefreshTeamColor()
{
	const FLinearColor TeamColor = TeamId == 0 ? Team0Color : Team1Color;
	TArray<UMeshComponent*> MeshComponents;
	GetComponents(MeshComponents);

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent))
		{
			MeshComponent->SetVectorParameterValueOnMaterials(
				TeamColorParameterName,
				FVector(TeamColor.R, TeamColor.G, TeamColor.B));
		}
	}
}
