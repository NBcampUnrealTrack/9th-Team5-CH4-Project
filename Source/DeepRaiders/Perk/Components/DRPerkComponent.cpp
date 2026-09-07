#include "DRPerkComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Components/DRSkillComponent.h"
#include "DeepRaiders/Skill/Effects/DRGE_SkillCooldown.h"
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
	const UDRPerkDefinition* PerkDefinition,
	FGameplayTag EquippedSkillId) const
{
	if (!IsValid(PerkDefinition)
		|| FindAvailableSlotIndex() == INDEX_NONE)
	{
		return false;
	}

	if (IsValid(PerkDefinition->ReplacementSkillDefinition)
		&& (PerkDefinition->EffectTarget != EDRPerkEffectTarget::EquippedSkill
			|| GetPerkCount(PerkDefinition) > 0))
	{
		return false;
	}

	const bool IsEffectConfigured = PerkDefinition->PerkEffectClass != nullptr
		|| !PerkDefinition->EffectRules.IsEmpty()
		|| PerkDefinition->PerkTag.IsValid()
		|| IsValid(PerkDefinition->ReplacementSkillDefinition);
	if (PerkDefinition->CompatibleSkillTags.IsEmpty())
	{
		return !EquippedSkillId.IsValid()
			&& IsEffectConfigured;
	}

	return EquippedSkillId.IsValid()
		&& PerkDefinition->CompatibleSkillTags.HasTagExact(EquippedSkillId)
		&& IsEffectConfigured;
}

bool UDRPerkComponent::CanAddPerkAutomatically(
	const UDRPerkDefinition* PerkDefinition) const
{
	if (!IsValid(PerkDefinition))
	{
		return false;
	}

	if (PerkDefinition->CompatibleSkillTags.IsEmpty())
	{
		return CanAddPerk(PerkDefinition);
	}

	const UDRSkillDefinition* SkillDefinition =
		FindUniqueCompatibleEquippedSkill(PerkDefinition);
	return IsValid(SkillDefinition)
		&& CanAddPerk(PerkDefinition, SkillDefinition->SkillId);
}

const UDRSkillDefinition* UDRPerkComponent::FindUniqueCompatibleEquippedSkill(
	const UDRPerkDefinition* PerkDefinition) const
{
	const ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	const UDRSkillComponent* SkillComponent = IsValid(PlayerState)
		? PlayerState->GetSkillComponent()
		: nullptr;

	if (!IsValid(PerkDefinition)
		|| PerkDefinition->CompatibleSkillTags.IsEmpty()
		|| !IsValid(SkillComponent))
	{
		return nullptr;
	}

	const UDRSkillDefinition* CompatibleSkill = nullptr;
	for (int32 SlotIndex = 0;
		SlotIndex < static_cast<int32>(EDRSkillSlot::Count);
		++SlotIndex)
	{
		const UDRSkillDefinition* SkillDefinition =
			SkillComponent->GetCurrentSkill(static_cast<EDRSkillSlot>(SlotIndex));

		if (!IsValid(SkillDefinition)
			|| !SkillDefinition->SkillId.IsValid()
			|| !PerkDefinition->CompatibleSkillTags.HasTagExact(SkillDefinition->SkillId))
		{
			continue;
		}

		if (IsValid(CompatibleSkill))
		{
			return nullptr;
		}

		CompatibleSkill = SkillDefinition;
	}

	return CompatibleSkill;
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
	const UDRPerkDefinition* PerkDefinition,
	bool bApplyPersistentPolicy) const
{
	if (!IsValid(AbilitySystemComponent)
		|| !IsValid(PerkDefinition)
		|| !PerkDefinition->PerkEffectClass)
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
	if (bApplyPersistentPolicy && PerkDefinition->bPersistThroughDeath)
	{
		EffectSpec.Data->AddDynamicAssetTag(DRGameplayTags::Effect_Policy_PersistThroughDeath);
	}
	
	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*EffectSpec.Data.Get());
}

FActiveGameplayEffectHandle UDRPerkComponent::ApplySkillEffectRule(
	UAbilitySystemComponent* AbilitySystemComponent,
	const UObject* SourceObject,
	const FDRSkillEffectRule& EffectRule,
	bool bPersistThroughDeath) const
{
	if (!IsValid(AbilitySystemComponent) || !EffectRule.EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(const_cast<UObject*>(SourceObject));

	FGameplayEffectSpecHandle EffectSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			EffectRule.EffectClass,
			1.0f,
			EffectContext);
	if (!EffectSpec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	if (bPersistThroughDeath)
	{
		EffectSpec.Data->AddDynamicAssetTag(
			DRGameplayTags::Effect_Policy_PersistThroughDeath);
	}

	for (const TPair<FGameplayTag, float>& EffectValue : EffectRule.EffectValues)
	{
		EffectSpec.Data->SetSetByCallerMagnitude(
			EffectValue.Key,
			EffectValue.Value);
	}

	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*EffectSpec.Data.Get());
}

void UDRPerkComponent::ApplySkillEffectRules(
	UAbilitySystemComponent* AbilitySystemComponent,
	const UObject* SourceObject,
	const TArray<FDRSkillEffectRule>& EffectRules,
	EDRSkillEffectTrigger Trigger,
	bool bPersistThroughDeath,
	TArray<FActiveGameplayEffectHandle>* OutActiveEffectHandles) const
{
	for (const FDRSkillEffectRule& EffectRule : EffectRules)
	{
		if (EffectRule.Trigger != Trigger)
		{
			continue;
		}

		const FActiveGameplayEffectHandle EffectHandle = ApplySkillEffectRule(
			AbilitySystemComponent,
			SourceObject,
			EffectRule,
			bPersistThroughDeath);
		if (OutActiveEffectHandles != nullptr && EffectHandle.IsValid())
		{
			OutActiveEffectHandles->Add(EffectHandle);
		}
	}
}

bool UDRPerkComponent::HasUsableSkillEffectRule(
	const UDRPerkDefinition* PerkDefinition) const
{
	return IsValid(PerkDefinition)
		&& PerkDefinition->EffectRules.ContainsByPredicate(
			[](const FDRSkillEffectRule& EffectRule)
			{
				return EffectRule.EffectClass != nullptr;
			});
}

bool UDRPerkComponent::AddPerk(
	UDRPerkDefinition* PerkDefinition,
	FGameplayTag EquippedSkillId,
	UDRSkillDefinition* ReplacedSkillDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	// 퍽과 GAS 상태는 서버에서만 변경한다.
	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(PerkDefinition))
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
	if (!CanAddPerk(PerkDefinition, EquippedSkillId) || SlotIndex == INDEX_NONE)
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

	// EffectRules 배열을 쓰는 스킬 장착 퍽은 스킬 발동 시에만 적용한다.
	// 기존 단일 PerkEffectClass 기반 공용 퍽만 장착 즉시 유지 효과를 적용한다.
	const bool bApplyPersistentEffect =
		!HasUsableSkillEffectRule(PerkDefinition)
		&& PerkDefinition->Trigger == EDRPerkTrigger::WhileEquipped
		&& PerkDefinition->EffectTarget == EDRPerkEffectTarget::OwnerCharacter
		&& PerkDefinition->PerkEffectClass != nullptr;
	const FActiveGameplayEffectHandle EffectHandle = bApplyPersistentEffect
		? ApplyPerkEffect(AbilitySystemComponent, PerkDefinition, true)
		: FActiveGameplayEffectHandle();
	if (bApplyPersistentEffect && !EffectHandle.IsValid())
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
	PerkEntry.EquippedSkillId = EquippedSkillId;
	PerkEntry.ReplacedSkillDefinition = ReplacedSkillDefinition;
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

bool UDRPerkComponent::AddPerkToSkill(
	UDRPerkDefinition* PerkDefinition,
	const UDRSkillDefinition* SkillDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UDRSkillComponent* SkillComponent = IsValid(PlayerState)
		? PlayerState->GetSkillComponent()
		: nullptr;

	if (!IsValid(PerkDefinition)
		|| !IsValid(SkillDefinition)
		|| !SkillDefinition->SkillId.IsValid()
		|| !IsValid(SkillComponent)
		|| SkillComponent->GetCurrentSkill(SkillDefinition->SkillSlot) != SkillDefinition
		|| !CanAddPerk(PerkDefinition, SkillDefinition->SkillId))
	{
		return false;
	}

	UDRSkillDefinition* ReplacementSkillDefinition = PerkDefinition->ReplacementSkillDefinition;
	if (!IsValid(ReplacementSkillDefinition))
	{
		return AddPerk(PerkDefinition, SkillDefinition->SkillId);
	}

	if (ReplacementSkillDefinition->SkillSlot != SkillDefinition->SkillSlot
		|| !SkillComponent->CanEquipSkill(ReplacementSkillDefinition)
		|| !SkillComponent->EquipSkill(ReplacementSkillDefinition))
	{
		return false;
	}

	UDRSkillDefinition* MutableSkillDefinition = const_cast<UDRSkillDefinition*>(SkillDefinition);
	if (AddPerk(
		PerkDefinition,
		SkillDefinition->SkillId,
		MutableSkillDefinition))
	{
		return true;
	}

	SkillComponent->EquipSkill(MutableSkillDefinition);
	return false;
}

bool UDRPerkComponent::AddPerkAutomatically(
	UDRPerkDefinition* PerkDefinition)
{
	if (!IsValid(PerkDefinition))
	{
		return false;
	}

	if (PerkDefinition->CompatibleSkillTags.IsEmpty())
	{
		return AddPerk(PerkDefinition);
	}

	const UDRSkillDefinition* SkillDefinition =
		FindUniqueCompatibleEquippedSkill(PerkDefinition);
	return IsValid(SkillDefinition)
		&& AddPerkToSkill(PerkDefinition, SkillDefinition);
}

void UDRPerkComponent::HandleSkillCommitted(
	const UDRSkillDefinition* SkillDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;

	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(SkillDefinition)
		|| !SkillDefinition->SkillId.IsValid())
	{
		return;
	}

	const FGameplayTag SkillId = SkillDefinition->SkillId;
	ApplySkillEffectRules(
		AbilitySystemComponent,
		SkillDefinition,
		SkillDefinition->BaseEffectRules,
		EDRSkillEffectTrigger::OnSkillCommitted,
		false,
		nullptr);

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
		if (!IsValid(PerkDefinition)
			|| PerkEntry.EquippedSkillId != SkillId
			|| PerkDefinition->EffectTarget != EDRPerkEffectTarget::OwnerCharacter)
		{
			continue;
		}

		if (HasUsableSkillEffectRule(PerkDefinition))
		{
			ApplySkillEffectRules(
				AbilitySystemComponent,
				PerkDefinition,
				PerkDefinition->EffectRules,
				EDRSkillEffectTrigger::OnSkillCommitted,
				false,
				nullptr);
		}
		else if (PerkDefinition->Trigger == EDRPerkTrigger::OnSkillCommitted)
		{
			ApplyPerkEffect(AbilitySystemComponent, PerkDefinition, false);
		}
	}
}

void UDRPerkComponent::HandleSkillActivated(
	const UDRSkillDefinition* SkillDefinition,
	TArray<FActiveGameplayEffectHandle>& OutActiveEffectHandles)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(SkillDefinition)
		|| !SkillDefinition->SkillId.IsValid())
	{
		return;
	}

	ApplySkillEffectRules(
		AbilitySystemComponent,
		SkillDefinition,
		SkillDefinition->BaseEffectRules,
		EDRSkillEffectTrigger::WhileSkillActive,
		false,
		&OutActiveEffectHandles);

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
		if (!IsValid(PerkDefinition)
			|| PerkEntry.EquippedSkillId != SkillDefinition->SkillId
			|| PerkDefinition->EffectTarget != EDRPerkEffectTarget::OwnerCharacter)
		{
			continue;
		}

		ApplySkillEffectRules(
			AbilitySystemComponent,
			PerkDefinition,
			PerkDefinition->EffectRules,
			EDRSkillEffectTrigger::WhileSkillActive,
			PerkDefinition->bPersistThroughDeath,
			&OutActiveEffectHandles);
	}
}

void UDRPerkComponent::HandleSkillCompleted(
	const UDRSkillDefinition* SkillDefinition)
{
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(SkillDefinition)
		|| !SkillDefinition->SkillId.IsValid())
	{
		return;
	}

	ApplySkillEffectRules(
		AbilitySystemComponent,
		SkillDefinition,
		SkillDefinition->BaseEffectRules,
		EDRSkillEffectTrigger::OnSkillCompleted,
		false,
		nullptr);

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
		if (!IsValid(PerkDefinition)
			|| PerkEntry.EquippedSkillId != SkillDefinition->SkillId
			|| PerkDefinition->EffectTarget != EDRPerkEffectTarget::OwnerCharacter)
		{
			continue;
		}

		ApplySkillEffectRules(
			AbilitySystemComponent,
			PerkDefinition,
			PerkDefinition->EffectRules,
			EDRSkillEffectTrigger::OnSkillCompleted,
			false,
			nullptr);
	}
}

bool UDRPerkComponent::HasSkillPerk(
	FGameplayTag SkillId,
	FGameplayTag PerkTag) const
{
	return SkillId.IsValid()
		&& PerkTag.IsValid()
		&& PerkEntries.ContainsByPredicate(
			[SkillId, PerkTag](const FDRPerkEntry& PerkEntry)
			{
				return IsValid(PerkEntry.PerkDefinition)
					&& PerkEntry.EquippedSkillId == SkillId
					&& PerkEntry.PerkDefinition->EffectTarget
						== EDRPerkEffectTarget::EquippedSkill
					&& PerkEntry.PerkDefinition->PerkTag == PerkTag;
			});
}

float UDRPerkComponent::GetSkillEffectValue(
	FGameplayTag SkillId,
	EDRSkillEffectTrigger Trigger,
	FGameplayTag EffectValueTag) const
{
	if (!SkillId.IsValid() || !EffectValueTag.IsValid())
	{
		return 0.0f;
	}

	float TotalValue = 0.0f;
	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
		if (!IsValid(PerkDefinition)
			|| PerkEntry.EquippedSkillId != SkillId
			|| PerkDefinition->EffectTarget != EDRPerkEffectTarget::EquippedSkill)
		{
			continue;
		}

		for (const FDRSkillEffectRule& EffectRule : PerkDefinition->EffectRules)
		{
			if (EffectRule.Trigger != Trigger)
			{
				continue;
			}

			const float* EffectValue = EffectRule.EffectValues.Find(EffectValueTag);
			if (EffectValue != nullptr)
			{
				TotalValue += *EffectValue;
			}
		}
	}

	return TotalValue;
}

float UDRPerkComponent::GetSkillPerkEffectValue(
	FGameplayTag SkillId,
	FGameplayTag PerkTag,
	EDRSkillEffectTrigger Trigger,
	FGameplayTag EffectValueTag) const
{
	if (!SkillId.IsValid() || !PerkTag.IsValid() || !EffectValueTag.IsValid())
	{
		return 0.0f;
	}

	for (const FDRPerkEntry& PerkEntry : PerkEntries)
	{
		const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
		if (!IsValid(PerkDefinition)
			|| PerkEntry.EquippedSkillId != SkillId
			|| PerkDefinition->EffectTarget != EDRPerkEffectTarget::EquippedSkill
			|| PerkDefinition->PerkTag != PerkTag)
		{
			continue;
		}

		for (const FDRSkillEffectRule& EffectRule : PerkDefinition->EffectRules)
		{
			if (EffectRule.Trigger == Trigger)
			{
				const float* EffectValue = EffectRule.EffectValues.Find(EffectValueTag);
				return EffectValue != nullptr ? *EffectValue : 0.0f;
			}
		}
	}

	return 0.0f;
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
	if (!RestoreReplacedSkill(PerkEntries[PerkIndex]))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Perk][RemoveFailed] PerkId=%s Reason=SkillRestoreFailed"),
			*PerkInstanceId.ToString());
		return false;
	}

	const FActiveGameplayEffectHandle EffectHandle = PerkEntries[PerkIndex].EffectHandle;
	if (EffectHandle.IsValid() && !AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Perk][RemoveFailed] PerkId=%s Reason=EffectRemovalFailed"),
			*PerkInstanceId.ToString());
		return false;
	}

	NormalizeChargeCooldownOnRemoval(PerkEntries[PerkIndex]);

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
		if (!RestoreReplacedSkill(PerkEntry))
		{
			IsResetSucceeded = false;
			continue;
		}

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

bool UDRPerkComponent::RestoreReplacedSkill(const FDRPerkEntry& PerkEntry) const
{
	if (!IsValid(PerkEntry.ReplacedSkillDefinition))
	{
		return true;
	}

	const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UDRSkillComponent* SkillComponent = IsValid(PlayerState)
		? PlayerState->GetSkillComponent()
		: nullptr;
	if (!IsValid(PerkDefinition)
		|| !IsValid(PerkDefinition->ReplacementSkillDefinition)
		|| !IsValid(SkillComponent))
	{
		return false;
	}

	if (SkillComponent->GetCurrentSkill(PerkEntry.ReplacedSkillDefinition->SkillSlot)
		!= PerkDefinition->ReplacementSkillDefinition)
	{
		return true;
	}

	return SkillComponent->EquipSkill(PerkEntry.ReplacedSkillDefinition);
}

void UDRPerkComponent::NormalizeChargeCooldownOnRemoval(
	const FDRPerkEntry& PerkEntry) const
{
	const UDRPerkDefinition* PerkDefinition = PerkEntry.PerkDefinition;
	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = IsValid(PlayerState)
		? PlayerState->GetAbilitySystemComponent()
		: nullptr;
	UDRSkillComponent* SkillComponent = IsValid(PlayerState)
		? PlayerState->GetSkillComponent()
		: nullptr;
	if (!IsValid(PerkDefinition)
		|| PerkDefinition->PerkTag != DRGameplayTags::Perk_Skill_Charges
		|| !PerkEntry.EquippedSkillId.IsValid()
		|| !IsValid(AbilitySystemComponent)
		|| !IsValid(SkillComponent))
	{
		return;
	}

	const UDRSkillDefinition* SkillDefinition = nullptr;
	for (int32 SlotIndex = 0;
		SlotIndex < static_cast<int32>(EDRSkillSlot::Count);
		++SlotIndex)
	{
		const UDRSkillDefinition* Candidate = SkillComponent->GetCurrentSkill(
			static_cast<EDRSkillSlot>(SlotIndex));
		if (IsValid(Candidate)
			&& Candidate->SkillId == PerkEntry.EquippedSkillId)
		{
			SkillDefinition = Candidate;
			break;
		}
	}

	if (!IsValid(SkillDefinition) || !SkillDefinition->CooldownTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer CooldownTags(SkillDefinition->CooldownTag);
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	float NextChargeRemaining = 0.f;
	for (const TPair<float, float>& TimeAndDuration
		: AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (TimeAndDuration.Key > 0.f
			&& (NextChargeRemaining <= 0.f
				|| TimeAndDuration.Key < NextChargeRemaining))
		{
			NextChargeRemaining = TimeAndDuration.Key;
		}
	}

	AbilitySystemComponent->RemoveActiveEffects(Query);
	if (NextChargeRemaining <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(const_cast<UDRSkillDefinition*>(SkillDefinition));
	FGameplayEffectSpecHandle CooldownSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			UDRGE_SkillCooldown::StaticClass(),
			1.f,
			EffectContext);
	if (!CooldownSpec.IsValid())
	{
		return;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Cooldown_Duration,
		NextChargeRemaining);
	CooldownSpec.Data->DynamicGrantedTags.AddTag(
		SkillDefinition->CooldownTag);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*CooldownSpec.Data.Get());
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
