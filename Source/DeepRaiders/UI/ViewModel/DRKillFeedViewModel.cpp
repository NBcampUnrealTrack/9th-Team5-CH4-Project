#include "DRKillFeedViewModel.h"

#include "Engine/World.h"
#include "TimerManager.h"

void UDRKillFeedEntryViewModel::Initialize(
	const FString& InKillerName,
	const int32 InKillerTeamId,
	const FString& InVictimName,
	const int32 InVictimTeamId,
	const EDRKillFeedCause InCause)
{
	const bool bSnowDeath = InCause == EDRKillFeedCause::Snow;
	const FString KillerDisplayName = bSnowDeath ? TEXT("SNOW") : InKillerName;

	UE_MVVM_SET_PROPERTY_VALUE(KillerName, FText::FromString(KillerDisplayName));
	UE_MVVM_SET_PROPERTY_VALUE(KillerTeamId, InKillerTeamId);
	UE_MVVM_SET_PROPERTY_VALUE(VictimName, FText::FromString(InVictimName));
	UE_MVVM_SET_PROPERTY_VALUE(VictimTeamId, InVictimTeamId);
	UE_MVVM_SET_PROPERTY_VALUE(Cause, InCause);
	UE_MVVM_SET_PROPERTY_VALUE(KillerColor, ResolveTeamColor(InKillerTeamId));
	UE_MVVM_SET_PROPERTY_VALUE(VictimColor, ResolveTeamColor(InVictimTeamId));
	UE_MVVM_SET_PROPERTY_VALUE(bIsSnowDeath, bSnowDeath);
}

FLinearColor UDRKillFeedEntryViewModel::ResolveTeamColor(const int32 TeamId)
{
	switch (TeamId)
	{
	case 0:
		return FLinearColor::Red;

	case 1:
		return FLinearColor::Blue;

	default:
		return FLinearColor::White;
	}
}

void UDRKillFeedViewModel::Initialize(ADRPlayerController* InPlayerController)
{
	Deinitialize();

	if (!IsValid(InPlayerController))
	{
		return;
	}

	PlayerController = InPlayerController;
	InPlayerController->OnKillFeedEntry.AddDynamic(
		this,
		&ThisClass::HandleKillFeedEntry);
}

void UDRKillFeedViewModel::Deinitialize()
{
	if (PlayerController.IsValid())
	{
		PlayerController->OnKillFeedEntry.RemoveDynamic(
			this,
			&ThisClass::HandleKillFeedEntry);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	PlayerController.Reset();

	TArray<TObjectPtr<UDRKillFeedEntryViewModel>> EmptyEntries;
	UE_MVVM_SET_PROPERTY_VALUE(Entries, MoveTemp(EmptyEntries));
}

void UDRKillFeedViewModel::HandleKillFeedEntry(
	FString KillerName,
	const int32 KillerTeamId,
	FString VictimName,
	const int32 VictimTeamId,
	const EDRKillFeedCause Cause)
{
	UDRKillFeedEntryViewModel* Entry =
		NewObject<UDRKillFeedEntryViewModel>(this);

	if (!IsValid(Entry))
	{
		return;
	}

	Entry->Initialize(
		KillerName,
		KillerTeamId,
		VictimName,
		VictimTeamId,
		Cause);

	TArray<TObjectPtr<UDRKillFeedEntryViewModel>> NewEntries = Entries;
	while (NewEntries.Num() >= MaxEntries)
	{
		NewEntries.RemoveAt(0);
	}
	NewEntries.Add(Entry);

	UE_MVVM_SET_PROPERTY_VALUE(Entries, MoveTemp(NewEntries));

	if (UWorld* World = GetWorld())
	{
		FTimerHandle ExpireTimerHandle;
		World->GetTimerManager().SetTimer(
			ExpireTimerHandle,
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::RemoveEntry,
				Entry),
			EntryLifetimeSeconds,
			false);
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[KillFeed][VM] Killer='%s' KillerTeam=%d Victim='%s' VictimTeam=%d Cause=%d"),
		*KillerName,
		KillerTeamId,
		*VictimName,
		VictimTeamId,
		static_cast<int32>(Cause));
}

void UDRKillFeedViewModel::RemoveEntry(UDRKillFeedEntryViewModel* Entry)
{
	if (!IsValid(Entry))
	{
		return;
	}

	TArray<TObjectPtr<UDRKillFeedEntryViewModel>> NewEntries = Entries;
	if (NewEntries.Remove(Entry) <= 0)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Entries, MoveTemp(NewEntries));
}
