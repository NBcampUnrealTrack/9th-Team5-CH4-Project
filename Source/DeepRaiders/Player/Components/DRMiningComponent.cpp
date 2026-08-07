#include "DRMiningComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelWorld.h"

UDRMiningComponent::UDRMiningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	bDrawMineAreaOnMine = true;
}

void UDRMiningComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheOwnerCharacter();
}

void UDRMiningComponent::TryMine()
{
	CacheOwnerCharacter();

	if (!CanMine())
	{
		return;
	}

	FHitResult HitResult;
	if (!PerformMiningTrace(HitResult))
	{
		return;
	}

	if (!MineLocal(HitResult))
	{
		return;
	}

	LastMineTime = GetWorld()->GetTimeSeconds();
}

void UDRMiningComponent::PreviewMineTarget()
{
	CacheOwnerCharacter();

	if (!IsValid(OwnerCharacter.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mining] Preview blocked: OwnerCharacter invalid"));
		return;
	}

	if (!OwnerCharacter->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mining] Preview blocked: not locally controlled"));
		return;
	}

	FHitResult HitResult;
	if (!PerformMiningTrace(HitResult))
	{
		return;
	}

	if (!IsValid(GetVoxelWorldFromHit(HitResult)))
	{
		return;
	}

	FVector MinePosition;
	if (GetMinePositionFromHit(HitResult, MinePosition))
	{
		DrawMineArea(MinePosition, FColor::Green, PreviewDebugDrawTime);
	}
}

void UDRMiningComponent::Server_RequestMine_Implementation(
	FVector_NetQuantize TraceStart,
	FVector_NetQuantize TraceEnd)
{
	// TODO: 서버 권한 채굴로 전환할 때 검증 및 Voxel 편집 처리를 구현한다.
	(void)TraceStart;
	(void)TraceEnd;
}

bool UDRMiningComponent::CanMine() const
{
	if (!IsValid(OwnerCharacter.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mining] CanMine false: OwnerCharacter invalid"));
		return false;
	}

	if (!OwnerCharacter->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mining] CanMine false: not locally controlled"));
		return false;
	}

	return !IsMineOnCooldown();
}

bool UDRMiningComponent::IsMineOnCooldown() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return true;
	}

	const float CurrentTime = World->GetTimeSeconds();
	return CurrentTime - LastMineTime < MineCooldown;
}

bool UDRMiningComponent::PerformMiningTrace(FHitResult& OutHitResult) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mining] Trace failed: World invalid"));
		return false;
	}

	if (!IsValid(OwnerCharacter.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mining] Trace failed: OwnerCharacter invalid"));
		return false;
	}

	FVector TraceStart;
	FRotator TraceRotation;
	GetTraceViewPoint(TraceStart, TraceRotation);

	const FVector TraceEnd =
		TraceStart + (TraceRotation.Vector() * MineTraceDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MineTrace), false);
	QueryParams.bTraceComplex = true;
	QueryParams.AddIgnoredActor(OwnerCharacter.Get());

	bool bHit = false;

	switch (TraceMode)
	{
	case EDRMiningTraceMode::LineTrace:
		bHit = World->LineTraceSingleByChannel(
			OutHitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);
		break;

	case EDRMiningTraceMode::SphereSweep:
		{
			const FCollisionShape MineShape =
				FCollisionShape::MakeSphere(MineRadius);

			bHit = World->SweepSingleByChannel(
				OutHitResult,
				TraceStart,
				TraceEnd,
				FQuat::Identity,
				ECC_Visibility,
				MineShape,
				QueryParams);
		}
		break;

	default:
		break;
	}

	return bHit;
}

bool UDRMiningComponent::MineLocal(const FHitResult& HitResult) const
{
	AVoxelWorld* VoxelWorld = GetVoxelWorldFromHit(HitResult);
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Mining] Hit actor is not a VoxelWorld. Actor=%s Component=%s"),
			*GetNameSafe(HitResult.GetActor()),
			*GetNameSafe(HitResult.GetComponent()));

		return false;
	}

	FVector MinePosition;
	if (!GetMinePositionFromHit(HitResult, MinePosition))
	{
		return false;
	}

	if (bDrawMineAreaOnMine)
	{
		DrawMineArea(MinePosition, FColor::Blue, MineDebugDrawTime);
	}

	UVoxelSphereTools::RemoveSphere(
		VoxelWorld,
		MinePosition,
		MineRadius,
		nullptr,
		nullptr,
		true,
		true,
		true);

	return true;
}

AVoxelWorld* UDRMiningComponent::GetVoxelWorldFromHit(const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	if (!IsValid(HitComponent))
	{
		return nullptr;
	}

	return Cast<AVoxelWorld>(HitComponent->GetOwner());
}

bool UDRMiningComponent::GetMinePositionFromHit(const FHitResult& HitResult, FVector& OutMinePosition) const
{
	if (!HitResult.bBlockingHit)
	{
		return false;
	}

	const FVector SurfaceNormal =
		HitResult.ImpactNormal.IsNearlyZero()
			? FVector::UpVector
			: HitResult.ImpactNormal.GetSafeNormal();

	OutMinePosition =
		HitResult.ImpactPoint -
		(SurfaceNormal * MineRadius * MineSurfaceDepthRatio);

	return true;
}

void UDRMiningComponent::CacheOwnerCharacter()
{
	if (IsValid(OwnerCharacter.Get()))
	{
		return;
	}

	OwnerCharacter = Cast<ADRPlayerCharacter>(GetOwner());
}

void UDRMiningComponent::DrawMineArea(
	const FVector& MinePosition,
	const FColor& Color,
	float DrawTime) const
{
	if (!bDrawDebugTrace)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	DrawDebugSphere(
		World,
		MinePosition,
		MineRadius,
		24,
		Color,
		false,
		DrawTime,
		0,
		2.f);

	DrawDebugPoint(
		World,
		MinePosition,
		10.f,
		Color,
		false,
		DrawTime);
}

void UDRMiningComponent::GetTraceViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!IsValid(OwnerCharacter.Get()))
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}

	if (AController* Controller = OwnerCharacter->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	OwnerCharacter->GetActorEyesViewPoint(OutLocation, OutRotation);
}
