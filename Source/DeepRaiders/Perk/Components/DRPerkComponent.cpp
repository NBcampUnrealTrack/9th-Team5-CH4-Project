#include "DRPerkComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerState.h"
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

bool UDRPerkComponent::CanAddPerk(
	const UDRPerkDefinition* PerkDefinition) const
{
	return IsValid(PerkDefinition)
		&& PerkDefinition->PerkEffectClass
		&& !PerkDefinition->EffectValues.IsEmpty()
		&& PerkEntries.Num() < MaxPerkSlotCount;
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

	if (!CanAddPerk(PerkDefinition))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Perk][AddFailed] Player=%s Perk=%s Reason=SlotsFull Total=%d/%d"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PerkDefinition),
			PerkEntries.Num(),
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

	// 퍽 정의와 적용 핸들을 하나의 Entry로 보관한다.
	FDRPerkEntry& PerkEntry = PerkEntries.AddDefaulted_GetRef();
	PerkEntry.PerkDefinition = PerkDefinition;
	PerkEntry.EffectHandle = EffectHandle;
	const int32 SlotIndex = PerkEntries.Num() - 1;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Perk][SlotAdded] Player=%s Slot=%d Perk=%s DuplicateCount=%d Total=%d/%d"),
		*GetNameSafe(PlayerState),
		SlotIndex,
		*GetNameSafe(PerkDefinition),
		GetPerkCount(PerkDefinition),
		PerkEntries.Num(),
		MaxPerkSlotCount);

	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return true;
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

	// 각 퍽이 적용한 GameplayEffect만 ASC에서 회수한다.
	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		if (PerkEntry.EffectHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(
				PerkEntry.EffectHandle);
		}
	}

	PerkEntries.Reset();

	PlayerState->ForceNetUpdate();
	OnPerksChanged.Broadcast();
	return true;
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
		PerkEntries.Num(),
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
