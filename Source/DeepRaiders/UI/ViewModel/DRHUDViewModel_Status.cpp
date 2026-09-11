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
	// PlayerArray와 TeamId의 복제 순서에 상관없이 준비 인원을 다시 확인한다.
	if (!bGameStarted && !bGameEnded)
	{
		TeamRosterRefreshElapsed += DeltaSeconds;
		if (TeamRosterRefreshElapsed >= 0.25f)
		{
			TeamRosterRefreshElapsed = 0.f;
			RefreshTeamTexts();
		}
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
	const auto InterpolateSnowGauge = [DeltaSeconds](float DisplayValue, float TargetValue)
	{
		// 데디케이티드 서버에서는 여러 번의 획득이 한 Replication 값으로 합쳐져
		// 도착한다. FInterpTo는 그 첫 프레임에 큰 폭으로 뛰므로, 숫자 게이지는
		// 초당 일정 수치로만 따라가게 해 +1 단위로 자연스럽게 보이게 한다.
		constexpr float UnitsPerSecond = 60.f;
		return FMath::FInterpConstantTo(DisplayValue, TargetValue, DeltaSeconds, UnitsPerSecond);
	};
	constexpr float SnowGaugeHideDelay = 2.f;
	constexpr float SnowGaugeFadeDuration = 0.3f;
	if (bSnowGaugeFadeActive)
	{
		SnowGaugeIdleDuration += DeltaSeconds;
	}
	const float TargetSnowGaugeOpacity = bSnowGaugeFadeActive
		&& SnowGaugeIdleDuration < SnowGaugeHideDelay
		? 1.f
		: 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(
		SnowGaugeOpacity,
		FMath::FInterpConstantTo(
			SnowGaugeOpacity,
			TargetSnowGaugeOpacity,
			DeltaSeconds,
			1.f / SnowGaugeFadeDuration));
	if (bSnowGaugeFadeActive
		&& SnowGaugeIdleDuration >= SnowGaugeHideDelay
		&& SnowGaugeOpacity <= KINDA_SMALL_NUMBER)
	{
		bSnowGaugeFadeActive = false;
	}

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
	// 눈 수량은 표시값만 보간하고 정수가 바뀔 때 텍스트를 갱신한다.
	const int32 PreviousSnowCount = FMath::RoundToInt(DisplaySnowGauge);
	DisplaySnowGauge = InterpolateSnowGauge(DisplaySnowGauge, TargetSnowGauge);
	const int32 NewSnowCount = FMath::RoundToInt(DisplaySnowGauge);
	if (PreviousSnowCount != NewSnowCount)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeText, FText::AsNumber(NewSnowCount));
	}
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
	UE_MVVM_SET_PROPERTY_VALUE(
		FreezeScreenEffectRatio,
		InterpolateRatio(FreezeScreenEffectRatio, TargetScreenEffectRatio));
}

void UDRHUDViewModel::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void UDRHUDViewModel::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
	RefreshShield();
}

void UDRHUDViewModel::HandleShieldChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshShield();
}

void UDRHUDViewModel::HandleSnowGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (bHasSnowGaugePresentation)
	{
		return;
	}

	bSnowGaugeFadeActive = true;
	SnowGaugeIdleDuration = 0.f;
	RefreshSnowGaugeText();
}

void UDRHUDViewModel::ReceiveSnowGaugePresentation(const float SnowGauge, const uint32 Sequence)
{
	if (Sequence <= LastSnowGaugePresentationSequence)
	{
		return;
	}

	LastSnowGaugePresentationSequence = Sequence;
	bHasSnowGaugePresentation = true;
	// 이 값은 서버가 확정한 현재 보유량이다. 이후 일반 Attribute 복제는
	// 배치 상태 동기화 전용으로 취급해, 늦은 값이 HUD를 되돌리지 않게 한다.
	TargetSnowGauge = FMath::Max(0.f, SnowGauge);
	DisplaySnowGauge = TargetSnowGauge;
	bSnowGaugeFadeActive = true;
	SnowGaugeIdleDuration = 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeText, FText::AsNumber(FMath::RoundToInt(DisplaySnowGauge)));
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
	
	// 체력이 변경될 때 빙결 스크린 이펙트 비율(FreezeGauge / CurrentHealth)도 함께 재계산
	RefreshFreezeGauge();
}

void UDRHUDViewModel::RefreshShield()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewShield = IsValid(AttributeSet)
		? AttributeSet->GetShield()
		: 0.f;
	const float NewMaxHealth = IsValid(AttributeSet)
		? AttributeSet->GetMaxHealth()
		: 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(CurrentShield, NewShield);
	UE_MVVM_SET_PROPERTY_VALUE(
		ShieldRatio,
		NewMaxHealth > KINDA_SMALL_NUMBER
			? FMath::Max(0.f, NewShield / NewMaxHealth)
			: 0.f);
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
	if (bHasSnowGaugePresentation)
	{
		return;
	}
	// 빠른 HUD 스냅샷이 아직 한 번도 오지 않은 초기화/복구 경로다.
	// 이때만 일반 Attribute 복제값을 표시 원본으로 사용한다.
	if (NewSnowGauge < TargetSnowGauge - KINDA_SMALL_NUMBER)
	{
		DisplaySnowGauge = FMath::Min(DisplaySnowGauge, NewSnowGauge);
		UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeText, FText::AsNumber(FMath::RoundToInt(DisplaySnowGauge)));
	}
	TargetSnowGauge = NewSnowGauge;
	if (!bInterpolateGauges)
	{
		DisplaySnowGauge = TargetSnowGauge;
		UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeText, FText::AsNumber(FMath::RoundToInt(DisplaySnowGauge)));
	}
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

void UDRHUDViewModel::HandleFrozenTagChanged(FGameplayTag, int32)
{
	RefreshFreezeGauge();
}

void UDRHUDViewModel::RefreshFreezeGauge()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	
	const float NewFreezeGauge = IsValid(AttributeSet) ? AttributeSet->GetFreezeGauge() : 0.f;
	const float NewFreezeGaugeRatio = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewFreezeGauge / MaxHealth, 0.f, HealthRatio)
		: 0.f;
	const float NewScreenEffectRatio = CurrentHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewFreezeGauge / CurrentHealth, 0.f, 1.f)
		: 0.f;
	
	
	TargetFreezeGauge = NewFreezeGauge;
	
	const bool bIsFrozen = AbilitySystemComponent.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Frozen);
	
	// 얼음 게이지
	TargetFreezeGaugeRatio = bIsFrozen ? 
		HealthRatio :	// 최대 보유 체력과 일치 
		NewFreezeGaugeRatio;
	
	// 화면 효과 Opacity Ratio
	TargetScreenEffectRatio = bIsFrozen ? 
		1.f : // 얼면 1
		NewScreenEffectRatio;	// 안얼었으면 Ratio
	
	
	// 빙결 진입 시 보간을 기다리지 않고 최대 효과를 유지한다.
	
	if (bIsFrozen || !bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(FreezeGaugeRatio, TargetFreezeGaugeRatio);
		UE_MVVM_SET_PROPERTY_VALUE(FreezeScreenEffectRatio, TargetScreenEffectRatio);
	}
	if (!bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(FreezeGauge, TargetFreezeGauge);
	}
	
	
}
