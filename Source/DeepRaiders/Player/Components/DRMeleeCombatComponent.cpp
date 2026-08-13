#include "DRMeleeCombatComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

UDRMeleeCombatComponent::UDRMeleeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	/*
	 * Component 자체에서 Client -> Server RPC를 사용하므로
	 * replicated component로 생성한다.
	 */
	SetIsReplicatedByDefault(true);
}

ADRPlayerCharacter* UDRMeleeCombatComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(GetOwner());
}

void UDRMeleeCombatComponent::RequestAttack()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		Character->IsDead() ||
		!Character->HasHeldItemAction(
			EDRItemActionType::MeleeAttack))
	{
		return;
	}

	ServerRequestAttack();
}

bool UDRMeleeCombatComponent::CanStartAttack() const
{
	const ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		Character->IsDead())
	{
		return false;
	}

	if (!Character->HasHeldItemAction(
			EDRItemActionType::MeleeAttack))
	{
		return false;
	}

	if (bIsAttacking)
	{
		return false;
	}

	return true;
}

void UDRMeleeCombatComponent::ServerRequestAttack_Implementation()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!CanStartAttack() ||
		!IsValid(Character))
	{
		return;
	}

	bIsAttacking = true;

	/*
	 * 공격 1회 시작.
	 * 중복 타격 기록은 Notify Window가 아니라
	 * 공격 단위로 관리한다.
	 */
	AlreadyHitActors.Reset();

	Character->PlayMeleeWorldPresentationFromServer();

	if (TraceMode ==
		EDRMeleeTraceMode::ViewLine)
	{
		GetWorld()->GetTimerManager().SetTimer(
			MeleeHitTimerHandle,
			this,
			&ThisClass::PerformHitCheck,
			MeleeAttackHitTime,
			false);
	}

	GetWorld()->GetTimerManager().SetTimer(
		MeleeFinishTimerHandle,
		this,
		&ThisClass::FinishAttack,
		MeleeAttackDuration,
		false);
}

void UDRMeleeCombatComponent::PerformHitCheck()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		!bIsAttacking ||
		TraceMode !=
			EDRMeleeTraceMode::ViewLine)
	{
		return;
	}

	PerformLineTrace();
}

void UDRMeleeCombatComponent::PerformLineTrace()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	UWorld* World = GetWorld();

	if (!IsValid(Character) ||
		!IsValid(World))
	{
		return;
	}

	const FVector TraceStart =
		Character->GetPawnViewLocation();

	const FRotator AimRotation =
		Character->GetBaseAimRotation();

	const FVector TraceEnd =
		TraceStart +
		AimRotation.Vector() *
		MeleeAttackRange;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MeleeAttackLineTrace),
		false,
		Character);

	QueryParams.AddIgnoredActor(Character);

	FHitResult HitResult;

	const bool bHit =
		World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(
			World,
			TraceStart,
			TraceEnd,
			bHit
				? FColor::Green
				: FColor::Red,
			false,
			1.5f,
			0,
			2.f);
	}
#endif

	if (!bHit)
	{
		return;
	}

	ProcessHit(HitResult);
}

void UDRMeleeCombatComponent::StartSweepWindow()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		!bIsAttacking ||
		TraceMode !=
			EDRMeleeTraceMode::WeaponSweep ||
		bIsSweepActive)
	{
		return;
	}

	UStaticMeshComponent* WeaponMesh =
		Character->GetWorldHandEquipmentMesh();

	if (!IsValid(WeaponMesh) ||
		!WeaponMesh->DoesSocketExist(
			MeleeSweepBaseSocketName) ||
		!WeaponMesh->DoesSocketExist(
			MeleeSweepTipSocketName))
	{
		return;
	}

	bIsSweepActive = true;

	PreviousBaseLocation =
		WeaponMesh->GetSocketLocation(
			MeleeSweepBaseSocketName);

	PreviousTipLocation =
		WeaponMesh->GetSocketLocation(
			MeleeSweepTipSocketName);

	SweepSegment(
		PreviousBaseLocation,
		PreviousTipLocation);
}

void UDRMeleeCombatComponent::UpdateSweepWindow()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		!bIsAttacking ||
		!bIsSweepActive)
	{
		return;
	}

	UStaticMeshComponent* WeaponMesh =
		Character->GetWorldHandEquipmentMesh();

	if (!IsValid(WeaponMesh))
	{
		return;
	}

	const FVector CurrentBaseLocation =
		WeaponMesh->GetSocketLocation(
			MeleeSweepBaseSocketName);

	const FVector CurrentTipLocation =
		WeaponMesh->GetSocketLocation(
			MeleeSweepTipSocketName);

	SweepWeaponMotionFixedSamples(
		PreviousBaseLocation,
		PreviousTipLocation,
		CurrentBaseLocation,
		CurrentTipLocation);

	PreviousBaseLocation =
		CurrentBaseLocation;

	PreviousTipLocation =
		CurrentTipLocation;
}

void UDRMeleeCombatComponent::EndSweepWindow()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	bIsSweepActive = false;
}

void UDRMeleeCombatComponent::SweepSegment(
	const FVector& Start,
	const FVector& End)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	UWorld* World = GetWorld();

	if (!IsValid(Character) ||
		!IsValid(World))
	{
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MeleeWeaponSweep),
		false,
		Character);

	QueryParams.AddIgnoredActor(Character);

	TArray<FHitResult> HitResults;

	const bool bHit =
		World->SweepMultiByChannel(
			HitResults,
			Start,
			End,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(
				MeleeSweepRadius),
			QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(
			World,
			Start,
			End,
			bHit
				? FColor::Green
				: FColor::Red,
			false,
			0.15f,
			0,
			2.f);

		DrawDebugSphere(
			World,
			End,
			MeleeSweepRadius,
			12,
			bHit
				? FColor::Green
				: FColor::Red,
			false,
			0.15f);
	}
#endif

	if (!bHit)
	{
		return;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		ADRPlayerCharacter* HitPlayer =
			Cast<ADRPlayerCharacter>(
				HitResult.GetActor());

		if (!IsValid(HitPlayer) ||
			HitPlayer == Character)
		{
			continue;
		}

		ProcessHit(HitResult);
	}
}

void UDRMeleeCombatComponent::SweepWeaponMotionFixedSamples(
    const FVector& PreviousBase,
    const FVector& PreviousTip,
    const FVector& CurrentBase,
    const FVector& CurrentTip)
{
    /*
     * Base와 Tip 중 더 많이 움직인 쪽을 기준으로
     * 필요한 Substep 수를 계산한다.
     *
     * 예:
     * 최대 이동거리 42cm
     * SampleSpacing 15cm
     *
     * ceil(42 / 15) = 3
     *
     * 즉 해당 NotifyTick 구간을
     * 3개의 작은 Sweep 구간으로 나눈다.
     */
    const float BaseTravelDistance =
        FVector::Distance(
            PreviousBase,
            CurrentBase);

    const float TipTravelDistance =
        FVector::Distance(
            PreviousTip,
            CurrentTip);

    const float MaxTravelDistance =
        FMath::Max(
            BaseTravelDistance,
            TipTravelDistance);

    const float SafeSampleSpacing =
        FMath::Max(
            MeleeSweepSampleSpacing,
            1.f);

    int32 SubstepCount =
        FMath::Max(
            1,
            FMath::CeilToInt(
                MaxTravelDistance /
                SafeSampleSpacing));

    SubstepCount =
        FMath::Min(
            SubstepCount,
            MaxSweepSubstepsPerUpdate);

    FVector SamplePreviousBase =
        PreviousBase;

    FVector SamplePreviousTip =
        PreviousTip;

    for (int32 SampleIndex = 1;
         SampleIndex <= SubstepCount;
         ++SampleIndex)
    {
        const float Alpha =
            static_cast<float>(SampleIndex) /
            static_cast<float>(SubstepCount);

        /*
         * 직전 Notify Pose와 현재 Notify Pose 사이에
         * 일정 간격의 중간 Weapon Pose 생성.
         */
        const FVector SampleCurrentBase =
            FMath::Lerp(
                PreviousBase,
                CurrentBase,
                Alpha);

        const FVector SampleCurrentTip =
            FMath::Lerp(
                PreviousTip,
                CurrentTip,
                Alpha);

        const FVector SamplePreviousMiddle =
            (SamplePreviousBase +
             SamplePreviousTip) * 0.5f;

        const FVector SampleCurrentMiddle =
            (SampleCurrentBase +
             SampleCurrentTip) * 0.5f;

        /*
         * Weapon의 이동 궤적.
         */
        SweepSegment(
            SamplePreviousBase,
            SampleCurrentBase);

        SweepSegment(
            SamplePreviousMiddle,
            SampleCurrentMiddle);

        SweepSegment(
            SamplePreviousTip,
            SampleCurrentTip);

        /*
         * 해당 Sample 시점에서
         * 검날 전체 공간 검사.
         */
        SweepSegment(
            SampleCurrentBase,
            SampleCurrentTip);

        SamplePreviousBase =
            SampleCurrentBase;

        SamplePreviousTip =
            SampleCurrentTip;
    }
}

void UDRMeleeCombatComponent::ProcessHit(
	const FHitResult& HitResult)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	ADRPlayerCharacter* HitPlayer =
		Cast<ADRPlayerCharacter>(
			HitResult.GetActor());

	if (!IsValid(Character) ||
		!IsValid(HitPlayer) ||
		HitPlayer == Character ||
		!bIsAttacking)
	{
		return;
	}

	/*
	 * 공격 1회당 같은 Actor는 딱 한 번만.
	 *
	 * Sweep Segment가 여러 개이거나
	 * Notify Window가 재진입해도 여기서 최종 차단한다.
	 */
	if (AlreadyHitActors.Contains(HitPlayer))
	{
		return;
	}

	/*
	 * Damage보다 먼저 기록한다.
	 * 같은 프레임의 다른 Sweep Segment가 다시 들어와도
	 * 중복 처리를 못 하게 한다.
	 */
	AlreadyHitActors.Add(HitPlayer);

	const float AppliedDamage =
		UGameplayStatics::ApplyDamage(
			HitPlayer,
			MeleeAttackDamage,
			Character->GetController(),
			Character,
			UDamageType::StaticClass());

	if (AppliedDamage <= 0.f)
	{
		return;
	}

	const bool bKilled =
		HitPlayer->IsDead();

	Character->PlayMeleeHitPresentationFromServer(
		HitPlayer,
		bKilled,
		HitResult.ImpactPoint);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[Melee] Attacker=%s "
			"Target=%s Damage=%.1f"),
		*GetNameSafe(Character),
		*GetNameSafe(HitPlayer),
		AppliedDamage);
}

void UDRMeleeCombatComponent::FinishAttack()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	EndSweepWindow();

	bIsAttacking = false;
}

void UDRMeleeCombatComponent::CancelAttack()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			MeleeHitTimerHandle);

		World->GetTimerManager().ClearTimer(
			MeleeFinishTimerHandle);
	}

	EndSweepWindow();

	bIsAttacking = false;
}