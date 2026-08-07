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

	// 서버 RPC를 사용할 예정이므로 컴포넌트 자체도 기본 Replicate 대상으로 둔다.
	SetIsReplicatedByDefault(true);

	// 실제 채굴 클릭 피드백은 기본적으로 표시한다.
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

	// 입력은 외부에서 들어오지만, 채굴 가능 여부는 컴포넌트가 최종 방어선으로 검사한다.
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

	// 현재는 로컬 테스트용 채굴이므로 성공한 시점에 바로 쿨타임을 갱신한다.
	LastMineTime = GetWorld()->GetTimeSeconds();
}

void UDRMiningComponent::PreviewMineTarget()
{
	CacheOwnerCharacter();

	// 미리보기는 화면을 보는 로컬 플레이어에게만 필요하다.
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

	// 실제 채굴 위치와 동일한 계산식을 사용해야 미리보기와 결과가 어긋나지 않는다.
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
	// TODO: 서버 권한 채굴로 전환할 때 TraceStart/TraceEnd를 검증하고 Voxel 편집을 처리한다.
	// 지금은 로컬 단독 동작 확인 단계라 요청 데이터만 명시적으로 소비한다.
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
		// 월드가 없으면 시간 계산을 할 수 없으므로 안전하게 채굴을 막는다.
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

	// 카메라/마우스 조준 방향으로 MineTraceDistance만큼 채굴 후보를 찾는다.
	const FVector TraceEnd =
		TraceStart + (TraceRotation.Vector() * MineTraceDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MineTrace), false);
	// Voxel 지형은 복잡 충돌을 통해 표면을 맞는 경우가 있어 TraceComplex를 켠다.
	QueryParams.bTraceComplex = true;
	QueryParams.AddIgnoredActor(OwnerCharacter.Get());

	bool bHit = false;

	switch (TraceMode)
	{
	case EDRMiningTraceMode::LineTrace:
		// 조준선이 직접 닿은 표면만 채굴 대상으로 인정한다.
		bHit = World->LineTraceSingleByChannel(
			OutHitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);
		break;

	case EDRMiningTraceMode::SphereSweep:
		{
			// 구 형태로 쓸어가며 판정하므로 커서가 약간 빗나가도 주변 지형을 잡을 수 있다.
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

	// 실제 Voxel 지형을 깎는 진입점이다.
	// 멀티플레이에서는 서버에서 이 호출이 일어나야 다른 플레이어와 결과를 맞출 수 있다.
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
	// Hit Actor가 곧 VoxelWorld인 단순한 경우를 먼저 처리한다.
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	// VoxelWorld 하위 충돌 컴포넌트를 맞은 경우에는 컴포넌트의 Owner를 확인한다.
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

	// 표면 법선의 반대 방향으로 중심을 밀어 넣어 구가 지형 내부를 파고들게 한다.
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

	// 컴포넌트 Owner가 플레이어 캐릭터일 때만 채굴 기능을 활성화한다.
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

	// DrawTime이 0이면 매 프레임 그려도 잔상이 남지 않아 미리보기에 적합하다.
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
		// 플레이어가 조준하는 카메라 시점을 우선 사용한다.
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	// 컨트롤러가 아직 없으면 캐릭터 눈 위치를 대체 시점으로 사용한다.
	OwnerCharacter->GetActorEyesViewPoint(OutLocation, OutRotation);
}
