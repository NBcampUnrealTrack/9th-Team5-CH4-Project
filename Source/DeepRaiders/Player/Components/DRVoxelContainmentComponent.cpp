#include "DRVoxelContainmentComponent.h"

#include "DeepRaiders/Player/GAS/Effects/DRGE_VoxelContained.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelData/VoxelDataUtilities.h"
#include "VoxelWorld.h"

UDRVoxelContainmentComponent::UDRVoxelContainmentComponent()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	PrimaryComponentTick.bCanEverTick = false;
#else
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
#endif

	static ConstructorHelpers::FClassFinder<UGameplayEffect> FreezeGainEffectFinder(
		TEXT("/Game/DeepRaiders/GAS/Effects/BP_GE_FreezeGain"));
	if (FreezeGainEffectFinder.Succeeded())
	{
		FreezeGainEffectClass = FreezeGainEffectFinder.Class;
	}
}

void UDRVoxelContainmentComponent::EvaluateVoxelContainment(AVoxelWorld* VoxelWorld)
{
	if (IsValid(VoxelWorld))
	{
		DebugVoxelWorld = VoxelWorld;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!IsValid(GetAbilitySystemComponent()))
	{
		return;
	}

	const FVoxelCapsuleOccupancy Occupancy =
		GetVoxelCapsuleOccupancy(*VoxelWorld);
	const bool bInternalContained =
		Occupancy.FullySurroundedLayerCount >=
		FMath::Clamp(RequiredSurroundedLayers, 1, ContainmentLayerCount);
	const FCapsuleEscapeProbeResult EscapeProbe =
		GetCapsuleEscapeProbe(*VoxelWorld);
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const bool bCanFallThroughVerticalShaft = EscapeProbe.bDownwardEscapePathFound
		&& !EscapeProbe.bNonDownwardEscapePathFound
		&& IsValid(Character)
		&& IsValid(Character->GetCharacterMovement())
		&& Character->GetCharacterMovement()->IsFalling();
	if (!bCanFallThroughVerticalShaft &&
		(bInternalContained ||
			(EscapeProbe.bValid && !EscapeProbe.bNonDownwardEscapePathFound)))
	{
		EnterVoxelContainedMode(*VoxelWorld);
	}
}

void UDRVoxelContainmentComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!bDrawContainmentDebug)
	{
		return;
	}

	DebugDrawElapsedTime += DeltaTime;
	const float DrawInterval = FMath::Max(0.01f, DebugDrawInterval);
	if (DebugDrawElapsedTime < DrawInterval)
	{
		return;
	}
	DebugDrawElapsedTime = FMath::Fmod(DebugDrawElapsedTime, DrawInterval);

	if (AVoxelWorld* VoxelWorld = ResolveDebugVoxelWorld())
	{
		DrawContainmentDebug(*VoxelWorld);
	}
#endif
}

void UDRVoxelContainmentComponent::BindAbilitySystem(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	BoundAbilitySystemComponent = AbilitySystemComponent;
}

void UDRVoxelContainmentComponent::BeginPlay()
{
	Super::BeginPlay();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SetComponentTickEnabled(bDrawContainmentDebug);
#endif
}

void UDRVoxelContainmentComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearContainmentState();
	BoundAbilitySystemComponent.Reset();
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* UDRVoxelContainmentComponent::GetAbilitySystemComponent() const
{
	return BoundAbilitySystemComponent.Get();
}

UDRVoxelContainmentComponent::FVoxelCapsuleOccupancy
UDRVoxelContainmentComponent::GetVoxelCapsuleOccupancy(
	AVoxelWorld& VoxelWorld) const
{
	FVoxelCapsuleOccupancy Result;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return Result;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		return Result;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float CylinderHalfHeight =
		FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
	const FVector Center = Capsule->GetComponentLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Right = Character->GetActorRightVector();
	const FVector HorizontalOffsets[ContainmentSamplesPerLayer] =
	{
		FVector::ZeroVector,
		Forward * Radius * 0.55f,
		-Forward * Radius * 0.55f,
		Right * Radius * 0.55f,
		-Right * Radius * 0.55f
	};
	const float VerticalOffsets[ContainmentLayerCount] =
	{
		-CylinderHalfHeight * 0.75f,
		0.f,
		CylinderHalfHeight * 0.75f
	};

	const FVoxelIntBox WorldBounds = VoxelWorld.GetWorldBounds();
	FIntVector SamplePositions[UE_ARRAY_COUNT(VerticalOffsets)][UE_ARRAY_COUNT(HorizontalOffsets)];
	FVoxelIntBoxWithValidity LockBounds;
	for (int32 VerticalIndex = 0; VerticalIndex < UE_ARRAY_COUNT(VerticalOffsets); ++VerticalIndex)
	{
		for (int32 HorizontalIndex = 0; HorizontalIndex < UE_ARRAY_COUNT(HorizontalOffsets); ++HorizontalIndex)
		{
			const FVector SampleLocation =
				Center + HorizontalOffsets[HorizontalIndex] +
				FVector::UpVector * VerticalOffsets[VerticalIndex];
			Result.SampleLocations[VerticalIndex][HorizontalIndex] = SampleLocation;
			const FIntVector VoxelPosition = VoxelWorld.GlobalToLocal(SampleLocation);
			SamplePositions[VerticalIndex][HorizontalIndex] = VoxelPosition;
			if (WorldBounds.Contains(VoxelPosition))
			{
				Result.bSampleInsideWorld[VerticalIndex][HorizontalIndex] = true;
				LockBounds += VoxelPosition;
			}
		}
	}

	if (!LockBounds.IsValid())
	{
		return Result;
	}

	FVoxelData& Data = VoxelWorld.GetData();
	FVoxelReadScopeLock Lock(Data, LockBounds.GetBox(), FUNCTION_FNAME);

	constexpr uint8 FullySurroundedMask =
		(1 << UE_ARRAY_COUNT(HorizontalOffsets)) - 1;
	for (int32 VerticalIndex = 0; VerticalIndex < UE_ARRAY_COUNT(VerticalOffsets); ++VerticalIndex)
	{
		uint8 SolidLayerMask = 0;
		for (int32 HorizontalIndex = 0; HorizontalIndex < UE_ARRAY_COUNT(HorizontalOffsets); ++HorizontalIndex)
		{
			const FIntVector& VoxelPosition = SamplePositions[VerticalIndex][HorizontalIndex];
			if (WorldBounds.Contains(VoxelPosition) &&
				!Data.GetValue(VoxelPosition, 0).IsEmpty())
			{
				SolidLayerMask |= 1 << HorizontalIndex;
			}
		}
		Result.SolidLayerMasks[VerticalIndex] = SolidLayerMask;

		// 높이별 결과를 합치지 않는다. 경사진 한쪽 벽이 서로 다른 높이에서
		// 반대편 표본까지 채운 것처럼 보이는 오탐을 막는다.
		if ((SolidLayerMask & FullySurroundedMask) == FullySurroundedMask)
		{
			++Result.FullySurroundedLayerCount;
		}
	}

	return Result;
}

AVoxelWorld* UDRVoxelContainmentComponent::ResolveDebugVoxelWorld()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return nullptr;
#else
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(World))
	{
		return nullptr;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	auto ContainsCharacter = [&CharacterLocation](AVoxelWorld* Candidate)
	{
		return IsValid(Candidate) && Candidate->IsCreated() &&
			Candidate->GetWorldBounds().Contains(Candidate->GlobalToLocal(CharacterLocation));
	};

	if (AVoxelWorld* CachedWorld = DebugVoxelWorld.Get(); ContainsCharacter(CachedWorld))
	{
		return CachedWorld;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (ContainsCharacter(*It))
		{
			DebugVoxelWorld = *It;
			return *It;
		}
	}

	DebugVoxelWorld.Reset();
	return nullptr;
#endif
}

UDRVoxelContainmentComponent::FCapsuleEscapeProbeResult
UDRVoxelContainmentComponent::GetCapsuleEscapeProbe(AVoxelWorld& VoxelWorld) const
{
	FCapsuleEscapeProbeResult Result;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(Capsule) || !IsValid(World))
	{
		return Result;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float CylinderHalfHeight = FMath::Max(0.f, HalfHeight - Radius);
	const float StepDistance = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() * EscapeProbeStepDistanceScale);
	const int32 StepCount = FMath::Clamp(EscapeProbeStepCount, 1, EscapeProbeMaxStepCount);
	const FVector Start = Capsule->GetComponentLocation();

	constexpr float Diagonal = 0.70710678f;
	const FVector Directions[EscapeProbeDirectionCount] =
	{
		FVector(1.f, 0.f, 0.f),
		FVector(-1.f, 0.f, 0.f),
		FVector(0.f, 1.f, 0.f),
		FVector(0.f, -1.f, 0.f),
		FVector(Diagonal, Diagonal, 0.f),
		FVector(Diagonal, -Diagonal, 0.f),
		FVector(-Diagonal, Diagonal, 0.f),
		FVector(-Diagonal, -Diagonal, 0.f),
		FVector::UpVector,
		-FVector::UpVector
	};
	const FVector RingDirections[] =
	{
		FVector(1.f, 0.f, 0.f),
		FVector(Diagonal, Diagonal, 0.f),
		FVector(0.f, 1.f, 0.f),
		FVector(-Diagonal, Diagonal, 0.f),
		FVector(-1.f, 0.f, 0.f),
		FVector(-Diagonal, -Diagonal, 0.f),
		FVector(0.f, -1.f, 0.f),
		FVector(Diagonal, -Diagonal, 0.f)
	};
	const float VerticalOffsets[] =
	{
		-CylinderHalfHeight * 0.75f,
		0.f,
		CylinderHalfHeight * 0.75f
	};

	FBox ProbeWorldBounds(ForceInit);
	const float MaxTravelDistance = StepDistance * static_cast<float>(StepCount);
	const FVector ProbeExtent(
		MaxTravelDistance + Radius,
		MaxTravelDistance + Radius,
		MaxTravelDistance + HalfHeight);
	ProbeWorldBounds += Start - ProbeExtent;
	ProbeWorldBounds += Start + ProbeExtent;
	FVoxelIntBoxWithValidity LockBoundsBuilder;
	LockBoundsBuilder += VoxelWorld.GlobalToLocal(ProbeWorldBounds.Min);
	LockBoundsBuilder += VoxelWorld.GlobalToLocal(ProbeWorldBounds.Max);
	if (!LockBoundsBuilder.IsValid())
	{
		return Result;
	}
	const FVoxelIntBox LockBounds = LockBoundsBuilder.GetBox().Extend(2);

	FVoxelData& Data = VoxelWorld.GetData();
	FVoxelReadScopeLock Lock(Data, LockBounds, FUNCTION_FNAME);
	const auto InterpolatedData = FVoxelDataUtilities::MakeBilinearInterpolatedData(Data);
	const FVoxelIntBox WorldBounds = VoxelWorld.GetWorldBounds();
	const float SampleRingRadius = Radius * 0.8f;

	auto GetCapsuleOccupancyScore = [&](const FVector& CapsuleCenter)
	{
		float TotalSolidness = 0.f;
		int32 SampleCount = 0;
		auto AccumulateSample = [&](const FVector& WorldLocation)
		{
			const FVoxelVector LocalPosition = VoxelWorld.GlobalToLocalFloat(WorldLocation);
			const FIntVector MinPosition(
				FMath::FloorToInt(LocalPosition.X),
				FMath::FloorToInt(LocalPosition.Y),
				FMath::FloorToInt(LocalPosition.Z));
			const FIntVector MaxPosition = MinPosition + FIntVector(1);
			if (!WorldBounds.Contains(MinPosition) || !WorldBounds.Contains(MaxPosition))
			{
				TotalSolidness += 1.f;
			}
			else
			{
				const float Density = InterpolatedData.GetValue(LocalPosition, 0);
				TotalSolidness += FMath::Clamp(-Density, 0.f, 1.f);
			}
			++SampleCount;
		};

		for (const float VerticalOffset : VerticalOffsets)
		{
			const FVector LayerCenter = CapsuleCenter + FVector::UpVector * VerticalOffset;
			AccumulateSample(LayerCenter);
			for (const FVector& RingDirection : RingDirections)
			{
				AccumulateSample(LayerCenter + RingDirection * SampleRingRadius);
			}
		}

		return SampleCount > 0 ? TotalSolidness / static_cast<float>(SampleCount) : 1.f;
	};

	const float OpenScore = FMath::Clamp(EscapeProbeOpenScore, 0.f, 1.f);
	const float AllowedIncrease = FMath::Clamp(EscapeProbeAllowedScoreIncrease, 0.f, 0.25f);
	Result.StartOccupancyScore = GetCapsuleOccupancyScore(Start);
	Result.bStartFits = Result.StartOccupancyScore <= OpenScore;

	for (int32 DirectionIndex = 0; DirectionIndex < EscapeProbeDirectionCount; ++DirectionIndex)
	{
		bool bPathConnected = true;
		float LowestScoreAlongPath = Result.StartOccupancyScore;

		for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
		{
			const FVector StepLocation =
				Start + Directions[DirectionIndex] * StepDistance * static_cast<float>(StepIndex + 1);
			const float MaximumSubstepDistance = FMath::Max(1.f, VoxelWorld.VoxelSize * 0.5f);
			const int32 SubstepCount = FMath::Max(
				1,
				FMath::CeilToInt(StepDistance / MaximumSubstepDistance));
			float OccupancyScore = LowestScoreAlongPath;
			for (int32 SubstepIndex = 0; SubstepIndex < SubstepCount; ++SubstepIndex)
			{
				const float Distance =
					StepDistance * (
						static_cast<float>(StepIndex) +
						static_cast<float>(SubstepIndex + 1) / static_cast<float>(SubstepCount));
				const FVector SubstepLocation =
					Start + Directions[DirectionIndex] * Distance;
				OccupancyScore = GetCapsuleOccupancyScore(SubstepLocation);
				if (OccupancyScore > LowestScoreAlongPath + AllowedIncrease)
				{
					bPathConnected = false;
					break;
				}
				LowestScoreAlongPath = FMath::Min(LowestScoreAlongPath, OccupancyScore);
			}
			if (!bPathConnected)
			{
				OccupancyScore = GetCapsuleOccupancyScore(StepLocation);
			}
			const bool bDestinationFits = OccupancyScore <= OpenScore;
			const bool bStepConnected = bPathConnected;

			Result.StepLocations[DirectionIndex][StepIndex] = StepLocation;
			Result.StepOccupancyScores[DirectionIndex][StepIndex] = OccupancyScore;
			Result.bStepFits[DirectionIndex][StepIndex] = bDestinationFits;
			Result.bStepPathConnected[DirectionIndex][StepIndex] = bStepConnected;
		}

		Result.BestEndOccupancyScore = FMath::Min(
			Result.BestEndOccupancyScore,
			Result.StepOccupancyScores[DirectionIndex][StepCount - 1]);
		if (Result.bStepPathConnected[DirectionIndex][StepCount - 1] &&
			Result.bStepFits[DirectionIndex][StepCount - 1])
		{
			Result.bEscapePathFound = true;
			Result.EscapeDirectionIndex = DirectionIndex;
			if (DirectionIndex == EscapeProbeDirectionCount - 1)
			{
				Result.bDownwardEscapePathFound = true;
			}
			else
			{
				Result.bNonDownwardEscapePathFound = true;
			}
		}
	}

	Result.bValid = true;
	return Result;
}

void UDRVoxelContainmentComponent::DrawContainmentDebug(AVoxelWorld& VoxelWorld) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!IsValid(Capsule) || !IsValid(World))
	{
		return;
	}

	const FVoxelCapsuleOccupancy Occupancy = GetVoxelCapsuleOccupancy(VoxelWorld);
	const FCapsuleEscapeProbeResult EscapeProbe = GetCapsuleEscapeProbe(VoxelWorld);
	const uint8 FullySurroundedMask = (1 << ContainmentSamplesPerLayer) - 1;
	const float Duration = FMath::Max(0.01f, DebugDrawInterval) * 1.5f;
	const float PointRadius = FMath::Max(4.f, VoxelWorld.VoxelSize * 0.12f);
	const FVector Center = Capsule->GetComponentLocation();

	DrawDebugCapsule(
		World,
		Center,
		Capsule->GetScaledCapsuleHalfHeight(),
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetComponentQuat(),
		FColor::Cyan,
		false,
		Duration,
		1,
		1.5f);

	FString LayerSummary;
	for (int32 LayerIndex = 0; LayerIndex < ContainmentLayerCount; ++LayerIndex)
	{
		const uint8 SolidMask = Occupancy.SolidLayerMasks[LayerIndex];
		const bool bLayerSurrounded = (SolidMask & FullySurroundedMask) == FullySurroundedMask;
		int32 SolidSampleCount = 0;
		for (int32 SampleIndex = 0; SampleIndex < ContainmentSamplesPerLayer; ++SampleIndex)
		{
			SolidSampleCount += (SolidMask & (1 << SampleIndex)) != 0 ? 1 : 0;
		}
		LayerSummary += FString::Printf(
			TEXT(" L%d:%d/5%s"),
			LayerIndex,
			SolidSampleCount,
			bLayerSurrounded ? TEXT(" PASS") : TEXT(""));

		for (int32 SampleIndex = 0; SampleIndex < ContainmentSamplesPerLayer; ++SampleIndex)
		{
			const bool bInsideWorld = Occupancy.bSampleInsideWorld[LayerIndex][SampleIndex];
			const bool bSolid = (SolidMask & (1 << SampleIndex)) != 0;
			const FColor Color = !bInsideWorld
				? FColor::Yellow
				: (bSolid ? FColor::Red : FColor::Green);
			const FVector& SampleLocation = Occupancy.SampleLocations[LayerIndex][SampleIndex];

			// Foreground depth priority로 지형 내부의 고체 표본도 가려지지 않게 한다.
			DrawDebugSphere(World, SampleLocation, PointRadius, 8, Color, false, Duration, 1, 1.5f);
			if (SampleIndex > 0)
			{
				DrawDebugLine(
					World,
					Occupancy.SampleLocations[LayerIndex][0],
					SampleLocation,
					Color,
					false,
					Duration,
					1,
					0.75f);
			}
		}
	}

	const int32 StepCount = FMath::Clamp(EscapeProbeStepCount, 1, EscapeProbeMaxStepCount);
	const float ProbeRadius = FMath::Max(3.f, PointRadius * 0.65f);
	for (int32 DirectionIndex = 0; DirectionIndex < EscapeProbeDirectionCount; ++DirectionIndex)
	{
		FVector PreviousLocation = Center;
		for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
		{
			const FVector& StepLocation = EscapeProbe.StepLocations[DirectionIndex][StepIndex];
			const bool bFits = EscapeProbe.bStepFits[DirectionIndex][StepIndex];
			const bool bConnected = EscapeProbe.bStepPathConnected[DirectionIndex][StepIndex];
			const FColor Color = bConnected
				? FColor::Green
				: (bFits ? FColor::Orange : FColor::Red);

			DrawDebugLine(World, PreviousLocation, StepLocation, Color, false, Duration, 1, 1.5f);
			DrawDebugSphere(World, StepLocation, ProbeRadius, 8, Color, false, Duration, 1, 1.5f);

			const bool bSelectedEscapeEndpoint =
				EscapeProbe.bEscapePathFound &&
				DirectionIndex == EscapeProbe.EscapeDirectionIndex &&
				StepIndex == StepCount - 1;
			if (bSelectedEscapeEndpoint)
			{
				DrawDebugCapsule(
					World,
					StepLocation,
					Capsule->GetScaledCapsuleHalfHeight(),
					Capsule->GetScaledCapsuleRadius(),
					Capsule->GetComponentQuat(),
					Color,
					false,
					Duration,
					1,
					1.f);
			}

			PreviousLocation = StepLocation;
		}
	}

	const int32 RequiredLayers = FMath::Clamp(RequiredSurroundedLayers, 1, ContainmentLayerCount);
	const bool bInternalContained = Occupancy.FullySurroundedLayerCount >= RequiredLayers;
	const bool bCanFallThroughVerticalShaft = EscapeProbe.bDownwardEscapePathFound
		&& !EscapeProbe.bNonDownwardEscapePathFound
		&& IsValid(Character)
		&& IsValid(Character->GetCharacterMovement())
		&& Character->GetCharacterMovement()->IsFalling();
	const bool bProposedContained =
		!bCanFallThroughVerticalShaft &&
		(bInternalContained || (EscapeProbe.bValid && !EscapeProbe.bNonDownwardEscapePathFound));
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	const bool bStateActive = IsValid(AbilitySystem) && AbilitySystem->HasMatchingGameplayTag(
		DRGameplayTags::State_VoxelContained);
	const FString ResultText = FString::Printf(
		TEXT("INTERNAL: %s | Layers %d/%d |%s\nESCAPE: Side/Up=%s Down=%s | Occupancy %.2f -> %.2f | PROPOSED: %s | State: %s"),
		bInternalContained ? TEXT("PASS") : TEXT("FAIL"),
		Occupancy.FullySurroundedLayerCount,
		RequiredLayers,
		*LayerSummary,
		EscapeProbe.bNonDownwardEscapePathFound ? TEXT("FOUND") : TEXT("BLOCKED"),
		EscapeProbe.bDownwardEscapePathFound ? TEXT("FOUND") : TEXT("BLOCKED"),
		EscapeProbe.StartOccupancyScore,
		EscapeProbe.BestEndOccupancyScore,
		bProposedContained ? TEXT("CONTAINED") : TEXT("NOT CONTAINED"),
		bStateActive ? TEXT("ACTIVE") : TEXT("INACTIVE"));
	DrawDebugString(
		World,
		Center + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + 35.f),
		ResultText,
		nullptr,
		bProposedContained ? FColor::Red : FColor::Yellow,
		Duration,
		true,
		1.15f);
#endif
}

void UDRVoxelContainmentComponent::EnterVoxelContainedMode(AVoxelWorld& VoxelWorld)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	VoxelContainmentWorld = &VoxelWorld;
	ReleaseStartTime = -1.f;

	if (!ContainmentEffectHandle.IsValid())
	{
		FGameplayEffectContextHandle Context =
			AbilitySystem->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
			UDRGE_VoxelContained::StaticClass(),
			1.f,
			Context);
		if (!Spec.IsValid())
		{
			return;
		}

		ContainmentEffectHandle =
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!ContainmentEffectHandle.IsValid())
		{
			return;
		}

		// 매몰 직전에 진행 중이던 행동도 종료한다. 눈 흡수만은 탈출 수단으로 유지한다.
		FGameplayTagContainer AbsorbAbilityTags;
		AbsorbAbilityTags.AddTag(DRGameplayTags::Ability_Snow_Absorb);
		AbilitySystem->CancelAbilities(nullptr, &AbsorbAbilityTags, nullptr);

		StartContainmentTimers();
	}
}

void UDRVoxelContainmentComponent::UpdateVoxelContainedMode()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem) ||
		!AbilitySystem->HasMatchingGameplayTag(
			DRGameplayTags::State_VoxelContained))
	{
		ClearContainmentState();
		return;
	}

	UWorld* World = GetWorld();
	AVoxelWorld* VoxelWorld = VoxelContainmentWorld.Get();
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		ClearContainmentState();
		return;
	}

	const float Time = World->GetTimeSeconds();
	const FVoxelCapsuleOccupancy Occupancy =
		GetVoxelCapsuleOccupancy(*VoxelWorld);
	const bool bInternalContained =
		Occupancy.FullySurroundedLayerCount >=
		FMath::Clamp(RequiredSurroundedLayers, 1, ContainmentLayerCount);
	const FCapsuleEscapeProbeResult EscapeProbe =
		GetCapsuleEscapeProbe(*VoxelWorld);
	if (bInternalContained ||
		(EscapeProbe.bValid && !EscapeProbe.bNonDownwardEscapePathFound))
	{
		ReleaseStartTime = -1.f;
		return;
	}
	if (!EscapeProbe.bValid)
	{
		return;
	}

	if (ReleaseStartTime < 0.f)
	{
		ReleaseStartTime = Time;
		return;
	}
	if (Time - ReleaseStartTime < FMath::Max(0.f, ReleaseDelay))
	{
		return;
	}

	ClearContainmentState();
}

void UDRVoxelContainmentComponent::ApplyFreezeGain()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem) || !FreezeGainEffectClass ||
		!AbilitySystem->HasMatchingGameplayTag(
			DRGameplayTags::State_VoxelContained))
	{
		return;
	}

	const float MaxHealth = AbilitySystem->GetNumericAttribute(
		UDRPlayerAttributeSet::GetMaxHealthAttribute());
	const float Interval = FMath::Max(0.01f, FreezeTickInterval);
	const float Duration = FMath::Max(0.01f, FreezeDeathDuration);
	const float FreezeAmount = MaxHealth / Duration * Interval;
	if (FreezeAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
		FreezeGainEffectClass,
		1.f,
		Context);
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Freeze_Amount,
		FreezeAmount);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UDRVoxelContainmentComponent::StartContainmentTimers()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.SetTimer(
		ContainmentCheckTimerHandle,
		this,
		&ThisClass::UpdateVoxelContainedMode,
		FMath::Max(0.01f, CheckInterval),
		true);
	TimerManager.SetTimer(
		FreezeGainTimerHandle,
		this,
		&ThisClass::ApplyFreezeGain,
		FMath::Max(0.01f, FreezeTickInterval),
		true);
}

void UDRVoxelContainmentComponent::StopContainmentTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ContainmentCheckTimerHandle);
		World->GetTimerManager().ClearTimer(FreezeGainTimerHandle);
	}
}

void UDRVoxelContainmentComponent::ClearContainmentState()
{
	StopContainmentTimers();

	if (ContainmentEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
		{
			AbilitySystem->RemoveActiveGameplayEffect(ContainmentEffectHandle);
		}
		ContainmentEffectHandle.Invalidate();
	}

	VoxelContainmentWorld.Reset();
	ReleaseStartTime = -1.f;
}
