
#include "DRGA_FireHitScan.h"

#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayPrediction.h"

namespace 
{
	constexpr int32 MaxHitScanIterations = 64;	
}

void UDRGA_FireHitScan::OnRangedWeaponActivated()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ActorInfo != nullptr
		&& ActorInfo->IsNetAuthority()
		&& !ActorInfo->IsLocallyControlled())
	{
		RegisterTargetDataDelegate();
	}
}

void UDRGA_FireHitScan::OnRangedWeaponEnded()
{
	UnregisterTargetDataDelegate();
}

bool UDRGA_FireHitScan::SendLocalShotRequest()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ActorInfo == nullptr
		|| !ActorInfo->IsLocallyControlled())
	{
		return false;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;

	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	FHitResult CameraHit;

	if (!TraceCameraAim(ViewLocation, ViewRotation.Vector(), CameraHit))
	{
		return false;
	}

	const FVector CameraAimPoint = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraHit.TraceEnd;

	FVector MuzzleLocation;

	if (!ResolveMuzzleLocation(ViewRotation.Vector(),MuzzleLocation))
	{
		return false;
	}
	
	FVector TraceDirection = CameraAimPoint - MuzzleLocation;
	if (!TraceDirection.Normalize())
	{
		TraceDirection = ViewRotation.Vector();
	}
	
	const FVector TraceEnd = MuzzleLocation + TraceDirection * GetMaxAttackDistance();
	const TArray<FGameplayEffectSpecHandle> EmptyEffectSpecs;
	
	const FVector PresentationTarget = TraceHitScan(MuzzleLocation, TraceEnd, false, EmptyEffectSpecs);
	
	if (!ActorInfo->IsNetAuthority())
	{
		PlayLocalFirePresentation(MuzzleLocation, PresentationTarget);
	}
	
	FGameplayAbilityTargetDataHandle TargetData(new FGameplayAbilityTargetData_SingleTargetHit(CameraHit));
	if (ActorInfo->IsNetAuthority())
	{
		HandleServerTargetData(TargetData, FGameplayTag());
		return true;
	}
	
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
	{
		return false;
	}
	
	// 부모 GA가 생성한 Prediction Key로 TargetData와 예측 Cooldown을 연결
	AbilitySystem->CallServerSetReplicatedTargetData(GetCurrentAbilitySpecHandle(),
		GetCurrentActivationInfo().GetActivationPredictionKey(),
		TargetData, FGameplayTag(), AbilitySystem->ScopedPredictionKey);
	
	return true;
}

void UDRGA_FireHitScan::RegisterTargetDataDelegate()
{
	if (TargetDataDelegateHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();

	if (!IsValid(AbilitySystem))
	{
		return;
	}

	const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();
	const FPredictionKey PredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey();

	TargetDataDelegateHandle = AbilitySystem->AbilityTargetDataSetDelegate(SpecHandle, PredictionKey)
		.AddUObject(this, &ThisClass::HandleServerTargetData);

	AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, PredictionKey);
}

void UDRGA_FireHitScan::UnregisterTargetDataDelegate()
{
	if (!TargetDataDelegateHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->AbilityTargetDataSetDelegate(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo()
			.GetActivationPredictionKey()).Remove(TargetDataDelegateHandle);
	}

	TargetDataDelegateHandle.Reset();
}

void UDRGA_FireHitScan::HandleServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
	FGameplayTag ApplicationTag)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const FGameplayAbilityTargetDataHandle TargetDataCopy = TargetData;

	if (!ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get())
		{
			AbilitySystem->ConsumeClientReplicatedTargetData(GetCurrentAbilitySpecHandle(),
				GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}

	FVector ServerViewLocation;
	FVector ValidatedAimDirection;

	if (!ValidateTargetData(TargetDataCopy, ServerViewLocation, ValidatedAimDirection))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);

		return;
	}

	// Cost, CoolDown 처리, CommitAbility 호출 시도
	if (!TryCommitServerShot())
	{
		return;
	}

	FHitResult CameraHit;

	// 카메라로 가리킨 타겟과 캐릭터 총구 조정
	if (!TraceCameraAim(ServerViewLocation,	ValidatedAimDirection,CameraHit))
	{
		return;
	}
	
	const FVector CameraAimPoint = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraHit.TraceEnd;

	FVector MuzzleLocation;

	if (!ResolveMuzzleLocation(ValidatedAimDirection,MuzzleLocation))
	{
		return;
	}

	FVector TraceDirection = CameraAimPoint - MuzzleLocation;

	if (!TraceDirection.Normalize())
	{
		TraceDirection = ValidatedAimDirection;
	}

	const FVector TraceEnd = MuzzleLocation + TraceDirection * GetMaxAttackDistance();
	
	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);

	const FVector PresentationTarget = TraceHitScan(MuzzleLocation, TraceEnd, true, ImpactEffectSpecs);
	
	PlayServerFirePresentation(MuzzleLocation, PresentationTarget);
}

bool UDRGA_FireHitScan::ValidateTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutServerViewLocation, FVector& OutAimDirection) const
{
	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);

	const FHitResult* ClientHitResult =	Data != nullptr ? Data->GetHitResult() : nullptr;

	if (ClientHitResult == nullptr 
		|| ClientHitResult->TraceStart.ContainsNaN() 
		|| ClientHitResult->TraceEnd.ContainsNaN())
	{
		return false;
	}

	FVector ClientAimDirection = ClientHitResult->TraceEnd - ClientHitResult->TraceStart;

	if (!ClientAimDirection.Normalize())
	{
		return false;
	}

	FRotator ServerViewRotation;

	if (!GetViewPoint(OutServerViewLocation, ServerViewRotation))
	{
		return false;
	}

	const FVector ServerViewDirection =	ServerViewRotation.Vector().GetSafeNormal();

	// 서버와 클라이언트 회전 오차 허용 범위
	const float MinimumAimDot = FMath::Cos(FMath::DegreesToRadians(MaxServerAimDeviationDegrees));

	if (FVector::DotProduct(ServerViewDirection, ClientAimDirection) < MinimumAimDot)
	{
		return false;
	}

	OutAimDirection = ClientAimDirection;
	return true;
}

FVector UDRGA_FireHitScan::TraceHitScan(const FVector& TraceStart, const FVector& TraceEnd, bool bApplyServerEffects,
	const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return TraceEnd;
	}

	FCollisionQueryParams QueryParams;
	BuildWeaponTraceQueryParams(QueryParams);

	FVector PresentationTarget = TraceEnd;

	for (int32 Iteration = 0; Iteration < MaxHitScanIterations; ++Iteration)
	{
		FHitResult HitResult;

		const bool bBlockingHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, 
			DRCollisionChannels::Projectile,	QueryParams);

		if (!bBlockingHit)
		{
			return PresentationTarget;
		}

		PresentationTarget = HitResult.ImpactPoint;

		AActor* HitActor = HitResult.GetActor();

		if (IsValid(HitActor) && IsFriendlyTarget(HitActor))
		{
			QueryParams.AddIgnoredActor(HitActor);
			continue;
		}

		UAbilitySystemComponent* TargetAbilitySystem = IsValid(HitActor) ? 
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor)	: nullptr;

		DrawDebugLine(GetWorld(), TraceStart, HitResult.ImpactPoint, FColor::Red, false, 1.0f, 0, 3);
		
		if (IsValid(TargetAbilitySystem))
		{
			if (bApplyServerEffects)
			{
				ApplyImpactEffectSpecs(TargetAbilitySystem,	HitResult, ImpactEffectSpecs);

				ExecuteImpactGameplayCue(HitResult);
			}

			// 관통에 대한 처리
			if (bCanPenetrateTargets)
			{
				QueryParams.AddIgnoredActor(HitActor);
				continue;
			}

			return PresentationTarget;
		}

		if (bApplyServerEffects)
		{
			TryApplyBreakableDamage(HitResult);
			ExecuteImpactGameplayCue(HitResult);
		}

		return PresentationTarget;
	}

	return PresentationTarget;
}
