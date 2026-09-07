#include "DRSkillSlotViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/Abilities/Skill/DRGA_StackedSpearThrowSkill.h"
#include "DeepRaiders/Skill/Components/DRSkillComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
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
	SkillSlot = InSkillSlot;

	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillChanged.AddDynamic(
			this,
			&ThisClass::HandleSkillChanged);
	}

	RefreshSkill();
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
	if (StackedSpearAbility.IsValid())
	{
		StackedSpearAbility->GetStackChangedDelegate().RemoveAll(this);
	}

	if (AbilitySystemComponent.IsValid()
		&& CooldownTag.IsValid()
		&& CooldownTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved).Remove(CooldownTagChangedHandle);
	}

	PlayerCharacter.Reset();
	AbilitySystemComponent.Reset();
	StackedSpearAbility.Reset();
	SkillComponent.Reset();
	SkillSlot = EDRSkillSlot::Count;
	CooldownTag = FGameplayTag();
	CooldownTagChangedHandle.Reset();

	UE_MVVM_SET_PROPERTY_VALUE(Icon, nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(IconTint, FLinearColor::White);
	UE_MVVM_SET_PROPERTY_VALUE(InputKeyText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(CooldownText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRatio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(IsOnCooldown, false);
	UE_MVVM_SET_PROPERTY_VALUE(StackText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(IsStackVisible, false);
	UE_MVVM_SET_PROPERTY_VALUE(IsVisible, false);
}

void UDRSkillSlotViewModel::HandleSkillChanged()
{
	RefreshSkill();
}

void UDRSkillSlotViewModel::HandleStackChanged()
{
	RefreshStack();
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
	UpdateStackAbility(SkillDefinition);
	UpdateCooldownTag(NewCooldownTag);
	RefreshStack();

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
			EGameplayTagEventType::NewOrRemoved).Remove(CooldownTagChangedHandle);
	}

	CooldownTag = NewCooldownTag;
	CooldownTagChangedHandle.Reset();
	if (AbilitySystemComponent.IsValid() && CooldownTag.IsValid())
	{
		CooldownTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
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
		return;
	}

	FGameplayTagContainer CooldownTags(CooldownTag);
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<TPair<float, float>> Cooldowns =
		AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query);

	float RemainingSeconds = 0.f;
	float DurationSeconds = 0.f;
	for (const TPair<float, float>& Cooldown : Cooldowns)
	{
		if (Cooldown.Key > RemainingSeconds)
		{
			RemainingSeconds = Cooldown.Key;
			DurationSeconds = Cooldown.Value;
		}
	}

	const bool IsNewOnCooldown = RemainingSeconds > 0.f;
	const float NewCooldownRatio = DurationSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(RemainingSeconds / DurationSeconds, 0.f, 1.f)
		: 0.f;
	const FText NewCooldownText = IsNewOnCooldown
		? FText::AsNumber(FMath::CeilToInt(RemainingSeconds))
		: FText::GetEmpty();

	UE_MVVM_SET_PROPERTY_VALUE(CooldownText, NewCooldownText);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRatio, NewCooldownRatio);
	UE_MVVM_SET_PROPERTY_VALUE(IsOnCooldown, IsNewOnCooldown);
	UE_MVVM_SET_PROPERTY_VALUE(
		IconTint,
		IsNewOnCooldown
			? FLinearColor(0.2f, 0.2f, 0.2f, 1.f)
			: FLinearColor::White);

	UWorld* World = PlayerCharacter.IsValid() ? PlayerCharacter->GetWorld() : nullptr;
	if (IsNewOnCooldown && IsValid(World))
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

void UDRSkillSlotViewModel::RefreshStack()
{
	const bool IsNewStackVisible = StackedSpearAbility.IsValid();
	const FText NewStackText = IsNewStackVisible
		? FText::Format(
			NSLOCTEXT("DRSkillSlot", "StackCount", "{0}/{1}"),
			StackedSpearAbility->GetCurrentStackCount(),
			StackedSpearAbility->GetMaximumStackCount())
		: FText::GetEmpty();

	UE_MVVM_SET_PROPERTY_VALUE(StackText, NewStackText);
	UE_MVVM_SET_PROPERTY_VALUE(IsStackVisible, IsNewStackVisible);
}

void UDRSkillSlotViewModel::UpdateStackAbility(
	const UDRSkillDefinition* SkillDefinition)
{
	if (StackedSpearAbility.IsValid())
	{
		StackedSpearAbility->GetStackChangedDelegate().RemoveAll(this);
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent.IsValid()
		&& IsValid(SkillDefinition)
		? AbilitySystemComponent->FindAbilitySpecFromClass(
			TSubclassOf<UGameplayAbility>(SkillDefinition->SkillAbility.Get()))
		: nullptr;
	StackedSpearAbility = AbilitySpec != nullptr
		? Cast<UDRGA_StackedSpearThrowSkill>(AbilitySpec->GetPrimaryInstance())
		: nullptr;

	if (StackedSpearAbility.IsValid())
	{
		StackedSpearAbility->GetStackChangedDelegate().AddUObject(
			this,
			&ThisClass::HandleStackChanged);
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
