#include "DRSkillSlotViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Skill/Components/DRSkillComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "TimerManager.h"

namespace
{
	constexpr float CooldownRefreshInterval = 1.0f / 60.0f;
}

void UDRSkillSlotViewModel::Initialize(
	ADRPlayerCharacter* InPlayerCharacter,
	EDRSkillSlot InSkillSlot)
{
	Deinitialize();

	if (!IsValid(InPlayerCharacter) || InSkillSlot == EDRSkillSlot::Count)
	{
		return;
	}

	PlayerCharacter = InPlayerCharacter;
	AbilitySystemComponent = InPlayerCharacter->GetAbilitySystemComponent();
	const ADRPlayerState* PlayerState = InPlayerCharacter->GetPlayerState<ADRPlayerState>();
	SkillComponent = IsValid(PlayerState) ? PlayerState->GetSkillComponent() : nullptr;
	PerkComponent = IsValid(PlayerState) ? PlayerState->GetPerkComponent() : nullptr;
	SkillSlot = InSkillSlot;

	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillChanged.AddDynamic(
			this,
			&ThisClass::HandleSkillChanged);
	}
	if (PerkComponent.IsValid())
	{
		PerkComponent->OnPerksChanged.AddDynamic(
			this,
			&ThisClass::HandlePerksChanged);
	}

	RefreshSkill();
	RefreshCooldown();
}

void UDRSkillSlotViewModel::Deinitialize()
{
	StopCooldownTimer();

	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillChanged.RemoveDynamic(
			this,
			&ThisClass::HandleSkillChanged);
	}
	if (PerkComponent.IsValid())
	{
		PerkComponent->OnPerksChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePerksChanged);
	}

	if (AbilitySystemComponent.IsValid()
		&& CooldownTag.IsValid()
		&& CooldownTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::AnyCountChange).Remove(CooldownTagChangedHandle);
	}

	PlayerCharacter.Reset();
	AbilitySystemComponent.Reset();
	SkillComponent.Reset();
	PerkComponent.Reset();
	SkillSlot = EDRSkillSlot::Count;
	CooldownTag = FGameplayTag();
	CooldownTagChangedHandle.Reset();

	UE_MVVM_SET_PROPERTY_VALUE(Icon, nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(IconTint, FLinearColor::White);
	UE_MVVM_SET_PROPERTY_VALUE(InputKeyText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(CooldownText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRatio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(IsOnCooldown, false);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentCharges, 1);
	UE_MVVM_SET_PROPERTY_VALUE(MaxCharges, 1);
	UE_MVVM_SET_PROPERTY_VALUE(UsesCharges, false);
	UE_MVVM_SET_PROPERTY_VALUE(IsVisible, false);
}

void UDRSkillSlotViewModel::HandleSkillChanged()
{
	RefreshSkill();
}

void UDRSkillSlotViewModel::HandlePerksChanged()
{
	RefreshCooldown();
}

void UDRSkillSlotViewModel::HandleCooldownTagChanged(FGameplayTag, int32)
{
	RefreshCooldown();
}

void UDRSkillSlotViewModel::RefreshSkill()
{
	UDRSkillDefinition* SkillDefinition = SkillComponent.IsValid()
		? SkillComponent->GetCurrentSkill(SkillSlot)
		: nullptr;
	const bool IsSkillEquipped = IsValid(SkillDefinition);
	const FGameplayTag NewCooldownTag = IsSkillEquipped
		? SkillDefinition->CooldownTag
		: FGameplayTag();
	UpdateCooldownTag(NewCooldownTag);

	UE_MVVM_SET_PROPERTY_VALUE(
		Icon,
		IsSkillEquipped ? SkillDefinition->Icon : nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(IsVisible, IsSkillEquipped);

	if (IsSkillEquipped)
	{
		RefreshInputKey();
	}
	else
	{
		UE_MVVM_SET_PROPERTY_VALUE(InputKeyText, FText::GetEmpty());
	}

	RefreshCooldown();
}

void UDRSkillSlotViewModel::UpdateCooldownTag(FGameplayTag NewCooldownTag)
{
	if (CooldownTag == NewCooldownTag)
	{
		return;
	}

	StopCooldownTimer();
	if (AbilitySystemComponent.IsValid()
		&& CooldownTag.IsValid()
		&& CooldownTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::AnyCountChange).Remove(CooldownTagChangedHandle);
	}

	CooldownTag = NewCooldownTag;
	CooldownTagChangedHandle.Reset();
	if (AbilitySystemComponent.IsValid() && CooldownTag.IsValid())
	{
		CooldownTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::AnyCountChange).AddUObject(
				this,
				&ThisClass::HandleCooldownTagChanged);
	}
}

void UDRSkillSlotViewModel::RefreshInputKey()
{
	const ADRPlayerController* PlayerController = PlayerCharacter.IsValid()
		? Cast<ADRPlayerController>(PlayerCharacter->GetController())
		: nullptr;
	const ULocalPlayer* LocalPlayer = IsValid(PlayerController)
		? PlayerController->GetLocalPlayer()
		: nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	const UInputAction* SkillInputAction = IsValid(PlayerController)
		? PlayerController->GetSkillInputAction(SkillSlot)
		: nullptr;
	const TArray<FKey> Keys = IsValid(InputSubsystem) && IsValid(SkillInputAction)
		? InputSubsystem->QueryKeysMappedToAction(SkillInputAction)
		: TArray<FKey>();
	const FText NewInputKeyText = Keys.IsEmpty()
		? FText::GetEmpty()
		: Keys[0].GetDisplayName();

	UE_MVVM_SET_PROPERTY_VALUE(InputKeyText, NewInputKeyText);
}

void UDRSkillSlotViewModel::RefreshCooldown()
{
	if (!AbilitySystemComponent.IsValid() || !CooldownTag.IsValid())
	{
		StopCooldownTimer();
		UE_MVVM_SET_PROPERTY_VALUE(CurrentCharges, 1);
		UE_MVVM_SET_PROPERTY_VALUE(MaxCharges, 1);
		UE_MVVM_SET_PROPERTY_VALUE(UsesCharges, false);
		return;
	}

	const UDRSkillDefinition* SkillDefinition = SkillComponent.IsValid()
		? SkillComponent->GetCurrentSkill(SkillSlot)
		: nullptr;
	const bool bUsesCharges = IsValid(SkillDefinition)
		&& PerkComponent.IsValid()
		&& PerkComponent->HasSkillPerk(
			SkillDefinition->SkillId,
			DRGameplayTags::Perk_Skill_Charges);
	const float ConfiguredMaxCharges = bUsesCharges
		? PerkComponent->GetSkillPerkEffectValue(
			SkillDefinition->SkillId,
			DRGameplayTags::Perk_Skill_Charges,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Perk_Charges_Max)
		: 1.0f;
	const int32 NewMaxCharges = bUsesCharges && ConfiguredMaxCharges > 0.0f
		? FMath::Max(1, FMath::RoundToInt(ConfiguredMaxCharges))
		: (bUsesCharges ? 3 : 1);

	FGameplayTagContainer CooldownTags(CooldownTag);
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<TPair<float, float>> Cooldowns =
		AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query);
	int32 ConsumedCharges = 0;
	if (bUsesCharges)
	{
		for (const FActiveGameplayEffectHandle EffectHandle
			: AbilitySystemComponent->GetActiveEffects(Query))
		{
			ConsumedCharges +=
				AbilitySystemComponent->GetCurrentStackCount(EffectHandle);
		}
	}

	float RemainingSeconds = 0.f;
	float DurationSeconds = 0.f;
	for (const TPair<float, float>& Cooldown : Cooldowns)
	{
		if ((bUsesCharges && (RemainingSeconds <= 0.0f || Cooldown.Key < RemainingSeconds))
			|| (!bUsesCharges && Cooldown.Key > RemainingSeconds))
		{
			RemainingSeconds = Cooldown.Key;
			DurationSeconds = Cooldown.Value;
		}
	}
	if (bUsesCharges && IsValid(SkillDefinition))
	{
		// 큐 뒤쪽 GE의 전체 Duration은 누적 시간이므로, 다음 한 칸의
		// 게이지 분모에는 스킬의 단일 충전 시간을 사용한다.
		DurationSeconds = SkillDefinition->CooldownDuration;
	}

	const int32 NewCurrentCharges = bUsesCharges
		? FMath::Clamp(NewMaxCharges - ConsumedCharges, 0, NewMaxCharges)
		: (RemainingSeconds > 0.0f ? 0 : 1);
	const bool IsNewOnCooldown = bUsesCharges
		? NewCurrentCharges <= 0
		: RemainingSeconds > 0.f;
	const float NewCooldownRatio = DurationSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(RemainingSeconds / DurationSeconds, 0.f, 1.f)
		: 0.f;
	const FText NewCooldownText = IsNewOnCooldown
		? FText::AsNumber(FMath::CeilToInt(RemainingSeconds))
		: FText::GetEmpty();

	UE_MVVM_SET_PROPERTY_VALUE(CooldownText, NewCooldownText);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRatio, NewCooldownRatio);
	UE_MVVM_SET_PROPERTY_VALUE(IsOnCooldown, IsNewOnCooldown);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentCharges, NewCurrentCharges);
	UE_MVVM_SET_PROPERTY_VALUE(MaxCharges, NewMaxCharges);
	UE_MVVM_SET_PROPERTY_VALUE(UsesCharges, bUsesCharges);
	UE_MVVM_SET_PROPERTY_VALUE(
		IconTint,
		IsNewOnCooldown
			? FLinearColor(0.2f, 0.2f, 0.2f, 1.f)
			: FLinearColor::White);

	UWorld* World = PlayerCharacter.IsValid() ? PlayerCharacter->GetWorld() : nullptr;
	const bool bNeedsCooldownRefresh = bUsesCharges
		? ConsumedCharges > 0
		: IsNewOnCooldown;
	if (bNeedsCooldownRefresh && IsValid(World))
	{
		if (!World->GetTimerManager().IsTimerActive(CooldownTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				CooldownTimerHandle,
				this,
				&ThisClass::RefreshCooldown,
				CooldownRefreshInterval,
				true);
		}
	}
	else
	{
		StopCooldownTimer();
	}
}

void UDRSkillSlotViewModel::StopCooldownTimer()
{
	UWorld* World = PlayerCharacter.IsValid() ? PlayerCharacter->GetWorld() : nullptr;
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}

	CooldownTimerHandle.Invalidate();
}
