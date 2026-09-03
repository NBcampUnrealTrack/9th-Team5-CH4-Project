#include "DRVoxelContainmentComponent.h"

#include "DeepRaiders/Player/GAS/Effects/DRGE_VoxelContained.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelWorld.h"

UDRVoxelContainmentComponent::UDRVoxelContainmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UGameplayEffect> FreezeGainEffectFinder(
		TEXT("/Game/DeepRaiders/GAS/Effects/BP_GE_FreezeGain"));
	if (FreezeGainEffectFinder.Succeeded())
	{
		FreezeGainEffectClass = FreezeGainEffectFinder.Class;
	}
}

void UDRVoxelContainmentComponent::EvaluateVoxelContainment(AVoxelWorld* VoxelWorld)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!IsValid(GetAbilitySystemComponent()))
	{
		return;
	}

	const FVoxelCapsuleOccupancy Occupancy =
		GetVoxelCapsuleOccupancy(*VoxelWorld);
	if (Occupancy.FullySurroundedLayerCount >=
		FMath::Clamp(RequiredSurroundedLayers, 1, 3))
	{
		EnterVoxelContainedMode(*VoxelWorld);
	}
}

void UDRVoxelContainmentComponent::BindAbilitySystem(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	BoundAbilitySystemComponent = AbilitySystemComponent;
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
	const FVector HorizontalOffsets[] =
	{
		FVector::ZeroVector,
		Forward * Radius * 0.55f,
		-Forward * Radius * 0.55f,
		Right * Radius * 0.55f,
		-Right * Radius * 0.55f
	};
	const float VerticalOffsets[] =
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
			const FIntVector VoxelPosition = VoxelWorld.GlobalToLocal(SampleLocation);
			SamplePositions[VerticalIndex][HorizontalIndex] = VoxelPosition;
			if (WorldBounds.Contains(VoxelPosition))
			{
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

		// 높이별 결과를 합치지 않는다. 경사진 한쪽 벽이 서로 다른 높이에서
		// 반대편 표본까지 채운 것처럼 보이는 오탐을 막는다.
		if ((SolidLayerMask & FullySurroundedMask) == FullySurroundedMask)
		{
			++Result.FullySurroundedLayerCount;
		}
	}

	return Result;
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
	if (Occupancy.FullySurroundedLayerCount >=
		FMath::Clamp(RequiredSurroundedLayers, 1, 3))
	{
		ReleaseStartTime = -1.f;
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
