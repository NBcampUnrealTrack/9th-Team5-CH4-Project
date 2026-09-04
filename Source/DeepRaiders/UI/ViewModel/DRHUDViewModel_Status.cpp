#include "DRHUDViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

namespace DRHUDStatus
{
	const FLinearColor HeatGaugeStartColor = FLinearColor::FromSRGBColor(
		FColor::FromHex(TEXT("D8FCFF")));
	const FLinearColor HeatGaugeGreenColor = FLinearColor::FromSRGBColor(
		FColor::FromHex(TEXT("5CD9AD")));
	const FLinearColor HeatGaugeYellowColor = FLinearColor::FromSRGBColor(
		FColor::FromHex(TEXT("3DD965")));
	const FLinearColor HeatGaugeOrangeColor = FLinearColor::FromSRGBColor(
		FColor::FromHex(TEXT("F9FF53")));
	const FLinearColor HeatGaugeEndColor = FLinearColor::FromSRGBColor(
		FColor::FromHex(TEXT("FF0000")));
	const FLinearColor HeatGaugeBlinkColor = FLinearColor::FromSRGBColor(
		FColor::FromHex(TEXT("FFA200")));

	FLinearColor GetHeatGaugeColor(float GaugeRatio)
	{
		const FLinearColor Colors[] =
		{
			HeatGaugeStartColor,
			HeatGaugeGreenColor,
			HeatGaugeYellowColor,
			HeatGaugeOrangeColor,
			HeatGaugeEndColor
		};
		const int32 ColorCount = static_cast<int32>(UE_ARRAY_COUNT(Colors));
		const float ScaledRatio = FMath::Clamp(GaugeRatio, 0.f, 1.f)
			* (ColorCount - 1);
		const int32 StartIndex = FMath::Min(
			FMath::FloorToInt(ScaledRatio),
			ColorCount - 2);
		const FLinearColor& StartColor = Colors[StartIndex];
		const FLinearColor& EndColor = Colors[StartIndex + 1];
		const float SegmentRatio = ScaledRatio - StartIndex;
		return FLinearColor(
			FMath::Lerp(StartColor.R, EndColor.R, SegmentRatio),
			FMath::Lerp(StartColor.G, EndColor.G, SegmentRatio),
			FMath::Lerp(StartColor.B, EndColor.B, SegmentRatio),
			1.f);
	}
}

void UDRHUDViewModel::TickGaugeInterpolation(float DeltaSeconds)
{
	if (!bInterpolateGauges || DeltaSeconds <= 0.f)
	{
		return;
	}

	const auto InterpolateRatio = [DeltaSeconds](float DisplayRatio, float TargetRatio)
	{
		constexpr float InterpolationSpeed = 8.f;
		constexpr float CompletionTolerance = 0.001f;
		return FMath::IsNearlyEqual(DisplayRatio, TargetRatio, CompletionTolerance)
			? TargetRatio
			: FMath::FInterpTo(DisplayRatio, TargetRatio, DeltaSeconds, InterpolationSpeed);
	};
	const auto InterpolateValue = [DeltaSeconds](float DisplayValue, float TargetValue)
	{
		constexpr float InterpolationSpeed = 8.f;
		constexpr float CompletionTolerance = 0.01f;
		return FMath::IsNearlyEqual(DisplayValue, TargetValue, CompletionTolerance)
			? TargetValue
			: FMath::FInterpTo(DisplayValue, TargetValue, DeltaSeconds, InterpolationSpeed);
	};
	constexpr float HeatGaugeHideDelay = 2.f;
	constexpr float HeatGaugeFadeDuration = 0.3f;
	const bool bHasHeat = TargetHeatGauge > KINDA_SMALL_NUMBER;
	if (bHasHeat)
	{
		bHeatGaugeWasActive = true;
		HeatGaugeZeroDuration = 0.f;
	}
	else if (bHeatGaugeWasActive)
	{
		HeatGaugeZeroDuration += DeltaSeconds;
	}

	const bool bWaitBeforeHiding = bHeatGaugeWasActive
		&& HeatGaugeZeroDuration < HeatGaugeHideDelay;
	const float TargetHeatGaugeOpacity = bHasHeat || bWaitBeforeHiding
		? 1.f
		: 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(
		HeatGaugeOpacity,
		FMath::FInterpConstantTo(
			HeatGaugeOpacity,
			TargetHeatGaugeOpacity,
			DeltaSeconds,
			1.f / HeatGaugeFadeDuration));

	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, InterpolateValue(CurrentHealth, TargetCurrentHealth));
	UE_MVVM_SET_PROPERTY_VALUE(HeatGauge, InterpolateValue(HeatGauge, TargetHeatGauge));
	UE_MVVM_SET_PROPERTY_VALUE(FreezeGauge, InterpolateValue(FreezeGauge, TargetFreezeGauge));
	UE_MVVM_SET_PROPERTY_VALUE(HealthRatio, InterpolateRatio(HealthRatio, TargetHealthRatio));
	UE_MVVM_SET_PROPERTY_VALUE(HeatGaugeRatio, InterpolateRatio(HeatGaugeRatio, TargetHeatGaugeRatio));
	UE_MVVM_SET_PROPERTY_VALUE(HeatIconOpacity, bIsOverheated ? 1.f : HeatGaugeRatio);
	if (bHoldHeatGaugeEndColor
		&& TargetHeatGaugeRatio <= KINDA_SMALL_NUMBER
		&& HeatGaugeRatio <= KINDA_SMALL_NUMBER)
	{
		bHoldHeatGaugeEndColor = false;
	}

	constexpr float HeatGaugeBlinkTransitionDuration = 0.5f;
	HeatGaugeBlinkElapsed = bIsOverheated ? HeatGaugeBlinkElapsed + DeltaSeconds : 0.f;
	const float HeatGaugeBlinkCycleDuration = HeatGaugeBlinkTransitionDuration * 2.f;
	const float HeatGaugeBlinkPhase = FMath::Fmod(
		HeatGaugeBlinkElapsed,
		HeatGaugeBlinkCycleDuration) / HeatGaugeBlinkTransitionDuration;
	const float HeatGaugeBlinkAlpha = HeatGaugeBlinkPhase <= 1.f
		? HeatGaugeBlinkPhase
		: 2.f - HeatGaugeBlinkPhase;
	const FLinearColor OverheatedColor = FMath::Lerp(
		DRHUDStatus::HeatGaugeEndColor,
		DRHUDStatus::HeatGaugeBlinkColor,
		HeatGaugeBlinkAlpha);
	UE_MVVM_SET_PROPERTY_VALUE(
		HeatGaugeColor,
		bIsOverheated
			? OverheatedColor
			: bHoldHeatGaugeEndColor
			? DRHUDStatus::HeatGaugeEndColor
			: DRHUDStatus::GetHeatGaugeColor(HeatGaugeRatio));
	UE_MVVM_SET_PROPERTY_VALUE(
		FreezeGaugeRatio,
		InterpolateRatio(FreezeGaugeRatio, TargetFreezeGaugeRatio));
}

void UDRHUDViewModel::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void UDRHUDViewModel::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void UDRHUDViewModel::HandleSnowGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshSnowGaugeText();
}

void UDRHUDViewModel::HandleHeatGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHeatGauge();
}

void UDRHUDViewModel::HandleMaxHeatGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHeatGauge();
}

void UDRHUDViewModel::HandleOverheatedTagChanged(FGameplayTag Tag, int32 NewCount)
{
	RefreshOverheatedState();
}

void UDRHUDViewModel::HandleFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFreezeGauge();
}

void UDRHUDViewModel::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFreezeGauge();
}

void UDRHUDViewModel::RefreshHealth()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;

	const float NewCurrentHealth = IsValid(AttributeSet) ? AttributeSet->GetHealth() : 0.f;
	const float NewMaxHealth = IsValid(AttributeSet) ? AttributeSet->GetMaxHealth() : 0.f;
	const float NewHealthRatio = NewMaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewCurrentHealth / NewMaxHealth, 0.f, 1.f)
		: 0.f;

	TargetCurrentHealth = NewCurrentHealth;
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth);
	TargetHealthRatio = NewHealthRatio;
	if (!bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, TargetCurrentHealth);
		UE_MVVM_SET_PROPERTY_VALUE(HealthRatio, TargetHealthRatio);
	}
}

void UDRHUDViewModel::RefreshHeatGauge()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewHeatGauge = IsValid(AttributeSet) ? AttributeSet->GetHeatGauge() : 0.f;
	const float NewMaxHeatGauge = IsValid(AttributeSet) ? AttributeSet->GetMaxHeatGauge() : 0.f;
	const float NewHeatGaugeRatio = NewMaxHeatGauge > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewHeatGauge / NewMaxHeatGauge, 0.f, 1.f)
		: 0.f;

	TargetHeatGauge = NewHeatGauge;
	UE_MVVM_SET_PROPERTY_VALUE(MaxHeatGauge, NewMaxHeatGauge);
	TargetHeatGaugeRatio = NewHeatGaugeRatio;
	if (NewHeatGauge > KINDA_SMALL_NUMBER)
	{
		bHeatGaugeWasActive = true;
		HeatGaugeZeroDuration = 0.f;
	}

	if (NewHeatGaugeRatio >= 1.f - KINDA_SMALL_NUMBER)
	{
		// 완전히 찬 뒤에는 게이지가 비워질 때까지 마지막 경고색을 유지한다.
		bHoldHeatGaugeEndColor = true;
	}
	else if (!bInterpolateGauges && NewHeatGauge <= KINDA_SMALL_NUMBER)
	{
		bHoldHeatGaugeEndColor = false;
	}

	if (!bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(HeatGauge, TargetHeatGauge);
		UE_MVVM_SET_PROPERTY_VALUE(HeatGaugeRatio, TargetHeatGaugeRatio);
		UE_MVVM_SET_PROPERTY_VALUE(HeatIconOpacity, bIsOverheated ? 1.f : HeatGaugeRatio);
		UE_MVVM_SET_PROPERTY_VALUE(HeatGaugeOpacity, NewHeatGauge > KINDA_SMALL_NUMBER ? 1.f : 0.f);
		UE_MVVM_SET_PROPERTY_VALUE(
			HeatGaugeColor,
			bHoldHeatGaugeEndColor
				? DRHUDStatus::HeatGaugeEndColor
				: DRHUDStatus::GetHeatGaugeColor(HeatGaugeRatio));
	}
}

void UDRHUDViewModel::RefreshSnowGaugeText()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewSnowGauge = IsValid(AttributeSet) ? AttributeSet->GetSnowGauge() : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeText, FText::AsNumber(FMath::RoundToInt(NewSnowGauge)));
}

void UDRHUDViewModel::RefreshOverheatedState()
{
	const bool bNewOverheated = AbilitySystemComponent.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Overheated);
	if (bIsOverheated != bNewOverheated)
	{
		HeatGaugeBlinkElapsed = 0.f;
	}
	if (!bNewOverheated)
	{
		bHoldHeatGaugeEndColor = false;
		UE_MVVM_SET_PROPERTY_VALUE(
			HeatGaugeColor,
			DRHUDStatus::GetHeatGaugeColor(HeatGaugeRatio));
	}

	UE_MVVM_SET_PROPERTY_VALUE(bIsOverheated, bNewOverheated);
	UE_MVVM_SET_PROPERTY_VALUE(HeatIconOpacity, bNewOverheated ? 1.f : HeatGaugeRatio);
}

void UDRHUDViewModel::RefreshFreezeGauge()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewFreezeGauge = IsValid(AttributeSet) ? AttributeSet->GetFreezeGauge() : 0.f;
	const float NewFreezeGaugeRatio = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewFreezeGauge / MaxHealth, 0.f, 1.f)
		: 0.f;

	TargetFreezeGauge = NewFreezeGauge;
	TargetFreezeGaugeRatio = NewFreezeGaugeRatio;
	if (!bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(FreezeGauge, TargetFreezeGauge);
		UE_MVVM_SET_PROPERTY_VALUE(FreezeGaugeRatio, TargetFreezeGaugeRatio);
	}
}
