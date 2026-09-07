#include "DRIceWall.h"

#include "DRIceWallSegment.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ADRIceWall::ADRIceWall()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(true);
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ADRIceWall::ConfigureWall(const int32 InOwnerTeamId, const FVector& InWallDimensions,
	const int32 InSegmentCount, const float InRiseDuration,
	const float InWallLifeSpan, const float InSegmentMaxHealth, const TSubclassOf<ADRIceWallSegment> InSegmentClass)
{
	check(HasAuthority());
	OwnerTeamId = InOwnerTeamId;
	WallDimensions = FVector(FMath::Max(1.f, InWallDimensions.X), FMath::Max(1.f, InWallDimensions.Y), FMath::Max(1.f, InWallDimensions.Z));
	SegmentCount = FMath::Max(1, InSegmentCount);
	RiseDuration = FMath::Max(0.01f, InRiseDuration);
	WallLifeSpan = FMath::Max(0.01f, InWallLifeSpan);
	SegmentMaxHealth = FMath::Max(1.f, InSegmentMaxHealth);
	SegmentClass = InSegmentClass;
}

void ADRIceWall::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	if (!SegmentClass || OwnerTeamId == INDEX_NONE)
	{
		Destroy();
		return;
	}

	SpawnSegments();
	if (Segments.IsEmpty())
	{
		Destroy();
		return;
	}

	bIsRising = GetRiseDistance() > KINDA_SMALL_NUMBER;
	SetLifeSpan(WallLifeSpan);
}

void ADRIceWall::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && bIsRising)
	{
		UpdateRise(DeltaSeconds);
	}
}

void ADRIceWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (ADRIceWallSegment* Segment : Segments)
		{
			if (IsValid(Segment))
			{
				Segment->Destroy();
			}
		}
	}

	Segments.Reset();
	SegmentFinalLocations.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADRIceWall::SpawnSegments()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FTransform WallTransform = GetActorTransform();
	const float RiseDistance = GetRiseDistance();
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const FVector FinalLocation = WallTransform.TransformPosition(MakeSegmentLocalLocation(SegmentIndex));
		const FVector InitialLocation = FinalLocation - FVector::UpVector * RiseDistance;
		const FTransform SegmentTransform(WallTransform.GetRotation(), InitialLocation);
		ADRIceWallSegment* Segment = World->SpawnActorDeferred<ADRIceWallSegment>(
			SegmentClass, SegmentTransform, GetOwner(), nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Segment))
		{
			continue;
		}

		Segment->InitializeSegment(OwnerTeamId, SegmentMaxHealth, GetSegmentDimensions());
		Segment->FinishSpawning(SegmentTransform);
		Segments.Add(Segment);
		SegmentFinalLocations.Add(FinalLocation);
	}
}

void ADRIceWall::UpdateRise(const float DeltaSeconds)
{
	const float RiseDistance = GetRiseDistance();
	RiseElapsed = FMath::Min(RiseElapsed + DeltaSeconds, RiseDuration);
	const float Alpha = FMath::InterpEaseOut(0.f, 1.f, RiseElapsed / RiseDuration, 2.f);

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		ADRIceWallSegment* Segment = Segments[Index];
		if (!IsValid(Segment))
		{
			continue;
		}

		const FVector InitialLocation = SegmentFinalLocations[Index] - FVector::UpVector * RiseDistance;
		// 조각은 바닥 내부에서 시작한다. Sweep 이동이면 시작 겹침을 바닥 충돌로 판정해 상승이 멈춘다.
		// 충돌체는 유지한 채 위치만 이동시켜, 올라온 조각을 CharacterMovement가 발판으로 인식하게 한다.
		Segment->SetActorLocation(FMath::Lerp(InitialLocation, SegmentFinalLocations[Index], Alpha), false);
	}

	if (RiseElapsed >= RiseDuration)
	{
		bIsRising = false;
		SetActorTickEnabled(false);
	}
}

FVector ADRIceWall::MakeSegmentLocalLocation(const int32 SegmentIndex) const
{
	const float SegmentCenter = (static_cast<float>(SegmentCount) - 1.f) * 0.5f;
	return FVector((static_cast<float>(SegmentIndex) - SegmentCenter) * GetSegmentDimensions().X, 0.f, 0.f);
}

FVector ADRIceWall::GetSegmentDimensions() const
{
	return FVector(
		WallDimensions.X / static_cast<float>(SegmentCount),
		WallDimensions.Y,
		WallDimensions.Z);
}

float ADRIceWall::GetRiseDistance() const
{
	return GetSegmentDimensions().Z;
}
