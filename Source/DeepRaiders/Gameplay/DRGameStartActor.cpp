#include "DRGameStartActor.h"

#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADRGameStartActor::ADRGameStartActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteractionMesh"));
	InteractionMesh->SetupAttachment(SceneRoot);
	InteractionMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionMesh->SetCollisionResponseToChannel(DRCollisionChannels::Interaction, ECR_Overlap);
}

void ADRGameStartActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshLocalReadyColor();
}

void ADRGameStartActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, ReadyPlayers);
	DOREPLIFETIME(ThisClass, bGameStarted);
	DOREPLIFETIME(ThisClass, CountdownSecondsRemaining);
}

bool ADRGameStartActor::CanInteract_Implementation(APawn* Interactor) const
{
	return !bGameStarted && IsValid(Interactor) && IsValid(Interactor->GetPlayerState());
}

bool ADRGameStartActor::Interact_Implementation(APawn* Interactor)
{
	if (!HasAuthority() || !Execute_CanInteract(this, Interactor))
	{
		return false;
	}

	APlayerState* PlayerState = Interactor->GetPlayerState();
	if (ReadyPlayers.Contains(PlayerState))
	{
		ReadyPlayers.Remove(PlayerState);
	}
	else
	{
		ReadyPlayers.Add(PlayerState);
	}

	RefreshLocalReadyColor();
	RefreshReadyState();
	return true;
}

bool ADRGameStartActor::GetInteractionPromptData_Implementation(
	APawn* Interactor,
	FDRInteractionPromptData& OutPromptData) const
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}

	OutPromptData.TitleText =
		NSLOCTEXT("DRGameStart", "Title", "준비 상태");
	OutPromptData.ActionText = IsPlayerReady(Interactor->GetPlayerState())
		? NSLOCTEXT("DRGameStart", "CancelReady", "준비 해제")
		: NSLOCTEXT("DRGameStart", "Ready", "준비");
	return true;
}

bool ADRGameStartActor::IsPlayerReady(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState) && ReadyPlayers.Contains(PlayerState);
}

TArray<APlayerState*> ADRGameStartActor::GetReadyPlayers() const
{
	TArray<APlayerState*> Players;
	Players.Reserve(ReadyPlayers.Num());
	for (APlayerState* PlayerState : ReadyPlayers)
	{
		if (IsValid(PlayerState))
		{
			Players.Add(PlayerState);
		}
	}
	return Players;
}

int32 ADRGameStartActor::GetReadyPlayerCount() const
{
	return GetReadyPlayers().Num();
}

int32 ADRGameStartActor::GetTotalPlayerCount() const
{
	return GetEligiblePlayerCount();
}

void ADRGameStartActor::OnRep_ReadyPlayers()
{
	RefreshLocalReadyColor();
	BroadcastReadyStatus();
}

void ADRGameStartActor::OnRep_GameStarted()
{
	BroadcastReadyStatus();
	if (bGameStarted)
	{
		OnAllPlayersReady.Broadcast();
	}
}

void ADRGameStartActor::OnRep_CountdownSecondsRemaining()
{
	BroadcastReadyStatus();
}

void ADRGameStartActor::RefreshReadyState()
{
	ReadyPlayers.RemoveAll([](const TObjectPtr<APlayerState>& PlayerState)
	{
		return !IsValid(PlayerState) || PlayerState->IsOnlyASpectator();
	});

	const int32 TotalPlayerCount = GetEligiblePlayerCount();
	const bool bAllPlayersReady = TotalPlayerCount > 0 && ReadyPlayers.Num() >= TotalPlayerCount;
	BroadcastReadyStatus();
	ForceNetUpdate();

	if (bAllPlayersReady)
	{
		if (!GetWorldTimerManager().IsTimerActive(GameStartTimerHandle))
		{
			CountdownSecondsRemaining = FMath::Max(1, GameStartCountdownSeconds);
			BroadcastReadyStatus();
			ForceNetUpdate();
			GetWorldTimerManager().SetTimer(
				GameStartTimerHandle,
				this,
				&ThisClass::HandleGameStartCountdown,
				1.f,
				true);
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(GameStartTimerHandle);
		CountdownSecondsRemaining = 0;
		BroadcastReadyStatus();
		ForceNetUpdate();
	}
}

void ADRGameStartActor::BroadcastReadyStatus()
{
	const int32 TotalPlayerCount = GetEligiblePlayerCount();
	const bool bAllPlayersReady = TotalPlayerCount > 0 && ReadyPlayers.Num() >= TotalPlayerCount;
	OnReadyStateChanged.Broadcast(ReadyPlayers.Num(), TotalPlayerCount, bAllPlayersReady);
	OnGameStartCountdownChanged.Broadcast(CountdownSecondsRemaining);
}

void ADRGameStartActor::HandleGameStartCountdown()
{
	ReadyPlayers.RemoveAll([](const TObjectPtr<APlayerState>& PlayerState)
	{
		return !IsValid(PlayerState) || PlayerState->IsOnlyASpectator();
	});

	const int32 TotalPlayerCount = GetEligiblePlayerCount();
	if (TotalPlayerCount <= 0 || ReadyPlayers.Num() < TotalPlayerCount)
	{
		GetWorldTimerManager().ClearTimer(GameStartTimerHandle);
		CountdownSecondsRemaining = 0;
		BroadcastReadyStatus();
		ForceNetUpdate();
		return;
	}

	--CountdownSecondsRemaining;
	if (CountdownSecondsRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(GameStartTimerHandle);
		StartGame();
		return;
	}

	BroadcastReadyStatus();
	ForceNetUpdate();
}

void ADRGameStartActor::RefreshLocalReadyColor()
{
	if (!IsValid(InteractionMesh))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* LocalController = IsValid(World)
		? World->GetFirstPlayerController()
		: nullptr;
	const APlayerState* LocalPlayerState = IsValid(LocalController)
		? LocalController->PlayerState
		: nullptr;
	const FLinearColor Color = IsPlayerReady(LocalPlayerState) ? ReadyColor : NotReadyColor;
	InteractionMesh->SetVectorParameterValueOnMaterials(
		ReadyColorParameterName,
		FVector(Color.R, Color.G, Color.B));
}

void ADRGameStartActor::StartGame()
{
	if (bGameStarted)
	{
		return;
	}

	ADRMiningGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>();
	if (!IsValid(GameMode) || !GameMode->StartGame())
	{
		return;
	}

	bGameStarted = true;
	BroadcastReadyStatus();
	OnAllPlayersReady.Broadcast();
	ForceNetUpdate();
}

void ADRGameStartActor::ResetForNextGame()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(GameStartTimerHandle);
	ReadyPlayers.Reset();
	CountdownSecondsRemaining = 0;
	bGameStarted = false;
	RefreshLocalReadyColor();
	BroadcastReadyStatus();
	ForceNetUpdate();
}

int32 ADRGameStartActor::GetEligiblePlayerCount() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return 0;
	}

	int32 PlayerCount = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (IsValid(PlayerState) && !PlayerState->IsOnlyASpectator())
		{
			++PlayerCount;
		}
	}
	return PlayerCount;
}
