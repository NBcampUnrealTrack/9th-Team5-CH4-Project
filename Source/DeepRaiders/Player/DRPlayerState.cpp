#include "DRPlayerState.h"

#include "DRPlayerCharacter.h"
#include "Net/UnrealNetwork.h"

void ADRPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRPlayerState, bHasDeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, DeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, bHasJetpack);
	DOREPLIFETIME_CONDITION(
		ADRPlayerState,
		CurrentJetpackFuel,
		COND_OwnerOnly);
	DOREPLIFETIME(ADRPlayerState, Coins);
}

bool ADRPlayerState::UpdateDeepestDigLocation(const FVector& Location)
{
	if (!HasAuthority()
		|| (bHasDeepestDigLocation && Location.Z >= DeepestDigLocation.Z))
	{
		return false;
	}

	bHasDeepestDigLocation = true;
	DeepestDigLocation = Location;
	ForceNetUpdate();
	return true;
}

void ADRPlayerState::GrantJetpack()
{
	if (!HasAuthority())
	{
		return;
	}

	bHasJetpack = true;
	CurrentJetpackFuel = MaxJetpackFuel;
	RefreshJetpackVisualOnPawn();
	ForceNetUpdate();
}

bool ADRPlayerState::ConsumeJetpackFuel(float Amount)
{
	if (!HasAuthority()
		|| !bHasJetpack
		|| Amount <= 0.f
		|| CurrentJetpackFuel <= 0.f)
	{
		return false;
	}

	CurrentJetpackFuel = FMath::Max(0.f, CurrentJetpackFuel - Amount);
	return true;
}

bool ADRPlayerState::RefillJetpackFuel()
{
	if (!HasAuthority()
		|| !bHasJetpack
		|| FMath::IsNearlyEqual(CurrentJetpackFuel, MaxJetpackFuel))
	{
		return false;
	}

	CurrentJetpackFuel = MaxJetpackFuel;
	ForceNetUpdate();
	return true;
}

int32 ADRPlayerState::GetCoins() const
{
	return Coins;
}

void ADRPlayerState::SetCoins(int32 NewCoins)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 PreviousCoins = Coins;
	const int32 ClampedCoins = FMath::Max(0, NewCoins);

	if (PreviousCoins == ClampedCoins)
	{
		return;
	}

	Coins = ClampedCoins;
	OnRep_Coins(PreviousCoins);
	ForceNetUpdate();
}

void ADRPlayerState::OnRep_Coins(int32 PreviousCoins)
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Coins changed: Previous=%d New=%d"),
		PreviousCoins,
		Coins);

	OnCoinsChanged.Broadcast(Coins);
}

void ADRPlayerState::OnRep_HasJetpack()
{
	RefreshJetpackVisualOnPawn();
}

void ADRPlayerState::OnRep_JetpackFuel()
{
}

void ADRPlayerState::RefreshJetpackVisualOnPawn()
{
	ADRPlayerCharacter* PlayerCharacter = GetPawn<ADRPlayerCharacter>();

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->RefreshJetpackVisual();
	}
}
