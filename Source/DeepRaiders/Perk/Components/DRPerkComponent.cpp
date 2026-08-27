#include "DRPerkComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

UDRPerkComponent::UDRPerkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRPerkComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UDRPerkComponent,
		PerkEntries,
		COND_OwnerOnly);
}

int32 UDRPerkComponent::GetPerkCount(
	const UDRPerkDefinition* PerkDefinition) const
{
	int32 PerkCount = 0;

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		if (PerkEntry.PerkDefinition == PerkDefinition)
		{
			++PerkCount;
		}
	}

	return PerkCount;
}

int32 UDRPerkComponent::GetTotalPerkCount() const
{
	int32 TotalPerkCount = 0;

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		if (IsValid(PerkEntry.PerkDefinition))
		{
			++TotalPerkCount;
		}
	}

	return TotalPerkCount;
}

bool UDRPerkComponent::CanAddPerk(
	const UDRPerkDefinition* PerkDefinition) const
{
	return IsValid(PerkDefinition)
		&& PerkDefinition->PerkEffectClass
		&& !PerkDefinition->EffectValues.IsEmpty()
		&& FindAvailableSlotIndex() != INDEX_NONE;
}

int32 UDRPerkComponent::FindAvailableSlotIndex() const
{
	for (int32 SlotIndex = 0; SlotIndex < MaxPerkSlotCount; ++SlotIndex)
	{
		if (!PerkEntries.IsValidIndex(SlotIndex)
			|| !IsValid(PerkEntries[SlotIndex].PerkDefinition))
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

int32 UDRPerkComponent::FindPerkIndex(FGuid PerkInstanceId) const
{
	if (!PerkInstanceId.IsValid())
	{
		return INDEX_NONE;
	}

	return PerkEntries.IndexOfByPredicate(
		[PerkInstanceId](const FDRPerkEntry& PerkEntry)
		{
			return IsValid(PerkEntry.PerkDefinition)
				&& PerkEntry.PerkInstanceId == PerkInstanceId;
		});
}

FActiveGameplayEffectHandle UDRPerkComponent::ApplyPerkEffect(
	UAbilitySystemComponent* AbilitySystemComponent,
	const UDRPerkDefinition* PerkDefinition) const
{
	if (!IsValid(AbilitySystemComponent)
		|| !IsValid(PerkDefinition)
		|| !PerkDefinition->PerkEffectClass
		|| PerkDefinition->EffectValues.IsEmpty())
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(PerkDefinition);

	FGameplayEffectSpecHandle EffectSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			PerkDefinition->PerkEffectClass,
			1.0f,
			EffectContext);
	if (!EffectSpec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	
	/* 퍽을 통해 적용되는 모든 GE는 사망과 리스폰을 통과한다.
	 * 
	 * GE 에셋 자체가 아니라 현재 Spec에만 추가하므로,
	 * 동일한 GE 클래스를 다른 시스템에서 사용해도 해당 효과에는 적용되지 않는다.
	 */
	if (PerkDefinition->bPersistThroughDeath)
	{
		EffectSpec.Data->AddDynamicAssetTag(DRGameplayTags::Effect_Policy_PersistThroughDeath);
	}
	
	for (const TPair<FGameplayTag, float>& EffectValue
		: PerkDefinition->EffectValues)
	{
		EffectSpec.Data->SetSetByCallerMagnitude(
			EffectValue.Key,
			EffectValue.Value);
	}

	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*EffectSpec.Data.Get());
}

bool UDRPerkComponent::AddPerk(UDRPerkDefinition* PerkDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	// 퍽과 GAS 상태는 서버에서만 변경한다.
	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(PerkDefinition)
		|| !PerkDefinition->PerkEffectClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][AddFailed] Player=%s Perk=%s Reason=InvalidStateOrDefinition"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	const int32 SlotIndex = FindAvailableSlotIndex();
	if (!CanAddPerk(PerkDefinition) || SlotIndex == INDEX_NONE)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][AddFailed] Player=%s Perk=%s Reason=SlotsFull Total=%d/%d"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition),
			GetTotalPerkCount(),
			MaxPerkSlotCount);
		return false;
	}

	const FActiveGameplayEffectHandle EffectHandle =
		ApplyPerkEffect(AbilitySystemComponent, PerkDefinition);
	if (!EffectHandle.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][EffectFailed] Player=%s Perk=%s Reason=InvalidEffectHandle"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition));
		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][EffectApplied] Player=%s Perk=%s"),
		*GetNameSafe(PlayerState),
		*GetNameSafe(PerkDefinition));

	if (!PerkEntries.IsValidIndex(SlotIndex))
	{
		PerkEntries.SetNum(SlotIndex + 1);
	}

	// 퍽 정의와 적용 핸들을 하나의 Entry로 보관한다.
	FDRPerkEntry& PerkEntry = PerkEntries[SlotIndex];
	do
	{
		PerkEntry.PerkInstanceId = FGuid::NewGuid();
	}
	while (FindPerkIndex(PerkEntry.PerkInstanceId) != INDEX_NONE);
	PerkEntry.PerkDefinition = PerkDefinition;
	PerkEntry.EffectHandle = EffectHandle;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][SlotAdded] Player=%s Slot=%d Perk=%s DuplicateCount=%d Total=%d/%d"),
		*GetNameSafe(PlayerState),
		SlotIndex,
		*GetNameSafe(PerkDefinition),
		GetPerkCount(PerkDefinition),
		GetTotalPerkCount(),
		MaxPerkSlotCount);

	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return true;
}

bool UDRPerkComponent::TryRemovePerk(FGuid PerkInstanceId)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState) || !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent) || !PerkInstanceId.IsValid())
	{
		return false;
	}

	const int32 PerkIndex = FindPerkIndex(PerkInstanceId);
	if (!PerkEntries.IsValidIndex(PerkIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Perk][RemoveFailed] PerkId=%s Reason=NotFound"),
			*PerkInstanceId.ToString());
		return false;
	}

	const UDRPerkDefinition* RemovedPerkDefinition =
		PerkEntries[PerkIndex].PerkDefinition;
	const FActiveGameplayEffectHandle EffectHandle = PerkEntries[PerkIndex].EffectHandle;
	if (EffectHandle.IsValid() && !AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Perk][RemoveFailed] PerkId=%s Reason=EffectRemovalFailed"),
			*PerkInstanceId.ToString());
		return false;
	}

	PerkEntries[PerkIndex] = FDRPerkEntry();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][Removed] Player=%s PerkId=%s Perk=%s Slot=%d"),
		*GetNameSafe(PlayerState),
		*PerkInstanceId.ToString(),
		*GetNameSafe(RemovedPerkDefinition),
		PerkIndex);
	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return true;
}

UDRPerkDefinition* UDRPerkComponent::FindPerkDefinition(FGuid PerkInstanceId) const
{
	const int32 PerkIndex = FindPerkIndex(PerkInstanceId);
	return PerkEntries.IsValidIndex(PerkIndex)
		? PerkEntries[PerkIndex].PerkDefinition.Get()
		: nullptr;
}

bool UDRPerkComponent::ResetPerks()
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent))
	{
		return false;
	}

	bool IsResetSucceeded = true;
	for (FDRPerkEntry& PerkEntry : PerkEntries)
	{
		if (PerkEntry.EffectHandle.IsValid()
			&& !AbilitySystemComponent->RemoveActiveGameplayEffect(PerkEntry.EffectHandle))
		{
			IsResetSucceeded = false;
			continue;
		}

		PerkEntry = FDRPerkEntry();
	}

	if (IsResetSucceeded)
	{
		PerkEntries.Reset();
	}

	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return IsResetSucceeded;
}

void UDRPerkComponent::RequestResetPerks()
{
	// PlayerState 소유 클라이언트에서 서버 RPC를 호출한다.
	ServerResetPerks();
}

void UDRPerkComponent::ServerResetPerks_Implementation()
{
	ResetPerks();
}

void UDRPerkComponent::OnRep_PerkEntries()
{
	// 복제 완료 후 소유 클라이언트의 퍽 UI를 갱신한다.
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][SlotsReplicated] Player=%s Total=%d/%d"),
		*GetNameSafe(GetOwner()),
		GetTotalPerkCount(),
		MaxPerkSlotCount);

	for (int32 SlotIndex = 0;
		SlotIndex < PerkEntries.Num();
		++SlotIndex)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Perk][Slot] Player=%s Slot=%d Perk=%s"),
			*GetNameSafe(GetOwner()),
			SlotIndex,
			*GetNameSafe(PerkEntries[SlotIndex].PerkDefinition));
	}

	OnPerksChanged.Broadcast();
}
