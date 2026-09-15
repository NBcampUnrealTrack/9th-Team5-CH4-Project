#include "DROreFieldActor.h"

#include "Components/BoxComponent.h"
#include "DROrePoolActor.h"
#include "DROrePoolSubsystem.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
float RandomNormalRange(FRandomStream& Random, const FVector2D& Range)
{
    const float Minimum = FMath::Min(Range.X, Range.Y);
    const float Maximum = FMath::Max(Range.X, Range.Y);
    const float Mean = (Minimum + Maximum) * 0.5f;
    const float StandardDeviation = (Maximum - Minimum) / 6.f;
    const float U1 = FMath::Max(Random.FRand(), UE_SMALL_NUMBER);
    const float U2 = Random.FRand();
    const float Normal = FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(UE_TWO_PI * U2);
    return FMath::Clamp(Mean + Normal * StandardDeviation, Minimum, Maximum);
}
}

ADROreFieldActor::ADROreFieldActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false; // 필드는 서버에서만 사용

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(BoundSize);

    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
}

void ADROreFieldActor::HandleOreReleased(int32 SpawnPointId, ADROrePoolActor* ReleasedActor)
{
    if (!HasAuthority() || !IsValid(ReleasedActor)) return;

    for (FDROreRuntimeSector& Sector : RuntimeSectors)
    {
        for (FDROreSpawnPoint& Point : Sector.SpawnPoints)
        {
            if (Point.SpawnPointId != SpawnPointId || Point.ActiveActor != ReleasedActor)
            {
                continue;
            }

            Point.ActiveActor = nullptr;
            Point.bDepleted = true;
            return;
        }
    }
}

void ADROreFieldActor::BeginPlay()
{
    Super::BeginPlay();

    // 클라이언트에는 위치 목록을 만들지 않는다.
    if (!HasAuthority() || !Definition) return;

    BuildCaveSamples();
    BuildSpawnPoints();
    PrewarmPool();

    if (UDRVoxelTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UDRVoxelTerrainSubsystem>())
    {
        Terrain->OnTerrainDug.AddUObject(this, &ThisClass::ReportTerrainDig);

        // Field 생성 전에 적용된 서버 채굴 이력 반영
        for (const FDRTerrainDigOperation& Operation : Terrain->GetDigHistory())
        {
            ReportTerrainDig(Operation.Location, Operation.Radius);
        }

        GenerateCave();
    }

    UpdateActiveSectors();
}

void ADROreFieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(CaveGenerationRetryTimer);

    if (UDRVoxelTerrainSubsystem* Terrain =
        GetWorld()->GetSubsystem<UDRVoxelTerrainSubsystem>())
    {
        Terrain->OnTerrainDug.RemoveAll(this);
    }

    if (HasAuthority())
    {
        for (FDROreRuntimeSector& Sector : RuntimeSectors)
        {
            SetSectorActive(Sector, false);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ADROreFieldActor::ReportTerrainDig(const FVector& Location, float Radius)
{
    if (!HasAuthority() || Radius < 0.f) return;

    const FVector LocalLocation = Bounds->GetComponentTransform().InverseTransformPosition(Location);
    const FVector Extent = Bounds->GetUnscaledBoxExtent();
    if (FMath::Abs(LocalLocation.X) > Extent.X + Radius || FMath::Abs(LocalLocation.Y) > Extent.Y + Radius)
    {
        return;
    }

    const float DugDepth = FMath::Clamp(Extent.Z - (LocalLocation.Z - Radius),
        0.f, Extent.Z * 2.f);
    if (DugDepth > DeepestDugDepth)
    {
        DeepestDugDepth = DugDepth;
        UpdateActiveSectors();
    }

    CheckNearbyOreGround(Location, Radius);
}

void ADROreFieldActor::CheckNearbyOreGround(const FVector& Location, float Radius)
{
    // RequestDig에 전달된 구보다 10% 넓게 광물 검색
    const float CheckRadius = Radius * 1.1f;
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams ObjectQuery;
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

    const bool bHasOverlap = GetWorld()->OverlapMultiByObjectType(Overlaps, Location,
        FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(CheckRadius));
    if (!bHasOverlap) return;

    TSet<ADROrePoolActor*> CheckedActors;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        ADROrePoolActor* OreActor = Cast<ADROrePoolActor>(Overlap.GetActor());
        if (!IsValid(OreActor) || CheckedActors.Contains(OreActor)) continue;

        CheckedActors.Add(OreActor);
        OreActor->ScheduleGroundCheck(Location);
    }
}

// 전체 스폰 수량을 풀에 등록하고 분할 생성 시작
void ADROreFieldActor::PrewarmPool()
{
    UDROrePoolSubsystem* Pool = GetWorld()->GetSubsystem<UDROrePoolSubsystem>();
    if (!Pool) return;

    for (const FDROreRuntimeSector& Sector : RuntimeSectors)
    {
        for (const FDROreSpawnPoint& Point : Sector.SpawnPoints)
        {
            Pool->QueuePrewarmOre(Point.ItemDefinition, Point.OreActorClass, 1);
        }
    }

    Pool->StartPrewarm(Definition->InitialPrewarmCount, Definition->PrewarmBatchSize);
}

// Seed로 부드럽게 이어지는 동굴 중심과 반경을 선계산한다.
void ADROreFieldActor::BuildCaveSamples()
{
    CaveSamples.Reset();
    PillarProtections.Reset();
    NextCaveSampleIndex = 0;
    CaveGenerationRetryCount = 0;
    if (!Definition->bGenerateCave || Definition->Caves.IsEmpty())
    {
        return;
    }

    const FVector BoundsExtent = Bounds->GetUnscaledBoxExtent();
    struct FCaveInstance
    {
        const FDROreCaveConfig* Config = nullptr;
        FVector Origin = FVector::ZeroVector;
        FVector Anchor = FVector::ZeroVector;
        int32 Seed = 0;
    };

    TArray<FCaveInstance> Instances;
    FRandomStream LayoutRandom(Definition->RandomSeed);
    int32 TotalInstanceCount = 0;
    for (const FDROreCaveConfig& Config : Definition->Caves)
    {
        TotalInstanceCount += FMath::Max(1, Config.Count);
    }

    const int32 GridSize = FMath::Max(1, FMath::CeilToInt(FMath::Pow(
        static_cast<float>(TotalInstanceCount), 1.f / 3.f)));
    TArray<int32> AvailableCells;
    for (int32 CellIndex = 0; CellIndex < GridSize * GridSize * GridSize; ++CellIndex)
    {
        AvailableCells.Add(CellIndex);
    }
    for (int32 Index = AvailableCells.Num() - 1; Index > 0; --Index)
    {
        AvailableCells.Swap(Index, LayoutRandom.RandRange(0, Index));
    }

    int32 NextCellIndex = 0;
    for (const FDROreCaveConfig& Config : Definition->Caves)
    {
        for (int32 Index = 0; Index < FMath::Max(1, Config.Count); ++Index)
        {
            FCaveInstance& Instance = Instances.AddDefaulted_GetRef();
            Instance.Config = &Config;
            const FVector RandomRatio = Config.CenterRandomRangeRatio.GetAbs();
            const FVector RandomRange = BoundsExtent * RandomRatio;
            const int32 CellIndex = AvailableCells[NextCellIndex++];
            const FIntVector Cell(CellIndex % GridSize, (CellIndex / GridSize) % GridSize,
                CellIndex / (GridSize * GridSize));
            const FVector CellAlpha = (FVector(Cell) + FVector(
                LayoutRandom.FRandRange(0.2f, 0.8f), LayoutRandom.FRandRange(0.2f, 0.8f),
                LayoutRandom.FRandRange(0.2f, 0.8f))) / static_cast<float>(GridSize);
            Instance.Origin = Config.CenterOffset + FVector(
                FMath::Lerp(-RandomRange.X, RandomRange.X, CellAlpha.X),
                FMath::Lerp(-RandomRange.Y, RandomRange.Y, CellAlpha.Y),
                FMath::Lerp(-RandomRange.Z, RandomRange.Z, CellAlpha.Z));
            Instance.Seed = static_cast<int32>(LayoutRandom.GetUnsignedInt());
            Instance.Anchor = Instance.Origin;
        }
    }

    // 기둥 보호 영역을 먼저 등록해 다른 동굴도 기둥을 깎지 못하게 한다.
    for (const FCaveInstance& Instance : Instances)
    {
        if (Instance.Config->CaveType != EDROreCaveType::PillarCave)
        {
            continue;
        }

        FRandomStream ShapeRandom(Instance.Seed);
        GeneratePillarCave(*Instance.Config, Instance.Origin, ShapeRandom);
    }

    for (const FCaveInstance& Instance : Instances)
    {
        if (Instance.Config->CaveType == EDROreCaveType::PillarCave)
        {
            continue;
        }

        FRandomStream ShapeRandom(Instance.Seed);
        switch (Instance.Config->CaveType)
        {
        case EDROreCaveType::LongCave:
            GenerateLongCave(*Instance.Config, Instance.Origin, ShapeRandom);
            break;
        case EDROreCaveType::BigCave:
            GenerateBigCave(*Instance.Config, Instance.Origin, ShapeRandom);
            break;
        case EDROreCaveType::FlatCave:
            GenerateFlatCave(*Instance.Config, Instance.Origin, ShapeRandom);
            break;
        case EDROreCaveType::PillarCave:
            break;
        }
    }

    const auto TryAddConnection = [&](const FCaveInstance& Start, const FCaveInstance& End)
    {
        if (LayoutRandom.FRand() > Start.Config->ConnectionChance)
        {
            return;
        }

        // 후보 통로의 절반은 완전히 연결하고, 나머지는 중간에서 막히게 한다.
        const float Completion = LayoutRandom.FRand() < 0.5f ? 1.f :
            LayoutRandom.FRandRange(0.35f, 0.65f);
        AddConnection(Start.Anchor, FMath::Lerp(Start.Anchor, End.Anchor, Completion),
            Start.Config->ConnectionRadius, Start.Config->ConnectionSpacing,
            Start.Config->ConnectionBottomRadiusScale, LayoutRandom);
    };

    for (int32 Index = 1; Index < Instances.Num(); ++Index)
    {
        const FCaveInstance& Instance = Instances[Index];
        TryAddConnection(Instance, Instances[Index - 1]);
    }

    // Long Cave는 최소 하나의 통로로 가장 가까운 다른 동굴과 연결한다.
    for (int32 Index = 0; Index < Instances.Num(); ++Index)
    {
        const FCaveInstance& Instance = Instances[Index];
        if (Instance.Config->CaveType != EDROreCaveType::LongCave || Instances.Num() < 2)
        {
            continue;
        }

        int32 NearestIndex = INDEX_NONE;
        float NearestDistanceSquared = MAX_flt;
        for (int32 OtherIndex = 0; OtherIndex < Instances.Num(); ++OtherIndex)
        {
            if (OtherIndex == Index)
            {
                continue;
            }
            const float DistanceSquared = FVector::DistSquared(Instance.Anchor,
                Instances[OtherIndex].Anchor);
            if (DistanceSquared < NearestDistanceSquared)
            {
                NearestIndex = OtherIndex;
                NearestDistanceSquared = DistanceSquared;
            }
        }
        TryAddConnection(Instance, Instances[NearestIndex]);
    }
}

void ADROreFieldActor::GenerateLongCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random)
{
    const float Spacing = FMath::Max(10.f, Config.LongSpacing);
    const int32 Count = FMath::Max(2, FMath::CeilToInt(Config.LongLength / Spacing) + 1);
    const float PhaseY = Random.FRandRange(0.f, UE_TWO_PI);
    const float PhaseZ = Random.FRandRange(0.f, UE_TWO_PI);
    const float PhaseRadius = Random.FRandRange(0.f, UE_TWO_PI);

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(Count - 1);
        const float Wave = Alpha * UE_TWO_PI * 1.5f;
        FVector Center = Origin;
        Center.X += FMath::Lerp(-Config.LongLength * 0.5f, Config.LongLength * 0.5f, Alpha);
        Center.Y += (FMath::Sin(Wave + PhaseY) + FMath::Sin(Wave * 2.3f + PhaseZ) * 0.35f) *
            Config.LongPathVariation;
        Center.Z += (FMath::Sin(Wave * 0.7f + PhaseZ) + FMath::Sin(Wave * 1.9f + PhaseY) * 0.25f) *
            Config.LongPathVariation * 0.6f;
        const float Radius = Config.LongRadius *
            (1.f + FMath::Sin(Wave * 1.3f + PhaseRadius) * Config.LongRadiusVariation);
        AddCaveSample(Center, Radius);
    }
}

void ADROreFieldActor::GenerateBigCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random)
{
    // 배치 범위만 인스턴스별로 바꾸고, 구 반경은 설정한 최소/최대를 지킨다.
    const float CaveScale = RandomNormalRange(Random, FVector2D(0.7f, 1.3f));
    const FVector Extent = Config.BigCaveSize.GetAbs() * 0.5f * CaveScale;
    const float MinimumRadius = FMath::Min(Config.BigMinSphereRadius,
        Config.BigMaxSphereRadius);
    const float MaximumRadius = FMath::Max(Config.BigMinSphereRadius,
        Config.BigMaxSphereRadius);
    AddCaveSample(Origin, Random.FRandRange(MinimumRadius, MaximumRadius));

    for (int32 Index = 1; Index < FMath::Max(1, Config.BigSphereCount); ++Index)
    {
        const FVector Direction = Random.VRand();
        const float DistanceAlpha = FMath::Pow(Random.FRand(), 0.65f);
        FVector Offset(Direction.X * Extent.X, Direction.Y * Extent.Y, Direction.Z * Extent.Z);
        Offset *= DistanceAlpha;
        Offset += Random.VRand() * MaximumRadius * 0.2f;
        const float EdgeAlpha = FMath::Clamp(
            DistanceAlpha + Random.FRandRange(-0.12f, 0.12f), 0.f, 1.f);
        const float Radius = FMath::Clamp(FMath::Lerp(MaximumRadius, MinimumRadius, EdgeAlpha) *
            Random.FRandRange(0.9f, 1.1f), MinimumRadius, MaximumRadius);
        AddCaveSample(Origin + Offset, Radius);
    }

    // 본체 주변에 작은 포켓을 붙여 외곽 실루엣을 끊어준다.
    const int32 PocketCount = Random.RandRange(3, 7);
    for (int32 Index = 0; Index < PocketCount; ++Index)
    {
        const FVector Direction = Random.VRand();
        const FVector Offset(Direction.X * Extent.X, Direction.Y * Extent.Y,
            Direction.Z * Extent.Z);
        AddCaveSample(Origin + Offset * Random.FRandRange(0.75f, 1.1f),
            Random.FRandRange(MinimumRadius * 0.25f, MinimumRadius * 0.55f));
    }
}

void ADROreFieldActor::GenerateFlatCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random)
{
    const float Radius = FMath::Max(10.f, Config.FlatSize.Z * 0.5f);
    const float Spacing = FMath::Min(FMath::Max(10.f, Config.FlatSpacing), Radius * 1.5f);
    const int32 XCount = FMath::Max(1, FMath::CeilToInt(FMath::Max(0.f,
        Config.FlatSize.X - Radius * 2.f) / Spacing) + 1);
    const int32 YCount = FMath::Max(1, FMath::CeilToInt(FMath::Max(0.f,
        Config.FlatSize.Y - Radius * 2.f) / Spacing) + 1);
    const float SlopeX = RandomNormalRange(Random, FVector2D(-0.12f, 0.12f));
    const float SlopeY = RandomNormalRange(Random, FVector2D(-0.12f, 0.12f));
    const float BendX = RandomNormalRange(Random,
        FVector2D(-Config.FlatNoiseStrength, Config.FlatNoiseStrength));
    const float BendY = RandomNormalRange(Random,
        FVector2D(-Config.FlatNoiseStrength, Config.FlatNoiseStrength));

    for (int32 X = 0; X < XCount; ++X)
    {
        for (int32 Y = 0; Y < YCount; ++Y)
        {
            const float XAlpha = XCount == 1 ? 0.5f : static_cast<float>(X) / (XCount - 1);
            const float YAlpha = YCount == 1 ? 0.5f : static_cast<float>(Y) / (YCount - 1);
            const float NoiseX = XAlpha * UE_TWO_PI * Config.FlatNoiseFrequency;
            const float NoiseY = YAlpha * UE_TWO_PI * Config.FlatNoiseFrequency;
            const float HeightNoise = (FMath::Sin(NoiseX + NoiseY * 0.7f) * 0.65f +
                FMath::Sin(NoiseX * 1.9f - NoiseY * 1.3f) * 0.35f) * Config.FlatNoiseStrength;
            const float LocalX = FMath::Lerp(-Config.FlatSize.X * 0.5f,
                Config.FlatSize.X * 0.5f, XAlpha);
            const float LocalY = FMath::Lerp(-Config.FlatSize.Y * 0.5f,
                Config.FlatSize.Y * 0.5f, YAlpha);
            const float Bend = (FMath::Square(XAlpha * 2.f - 1.f) - 0.5f) * BendX +
                (FMath::Square(YAlpha * 2.f - 1.f) - 0.5f) * BendY;
            FVector Center = Origin + FVector(
                FMath::Lerp(-Config.FlatSize.X * 0.5f + Radius,
                    Config.FlatSize.X * 0.5f - Radius, XAlpha),
                FMath::Lerp(-Config.FlatSize.Y * 0.5f + Radius,
                    Config.FlatSize.Y * 0.5f - Radius, YAlpha),
                HeightNoise + LocalX * SlopeX + LocalY * SlopeY + Bend);
            Center += Random.VRand() * Config.FlatNoiseStrength * 0.15f;
            const float RadiusNoise = Random.FRandRange(0.85f, 1.15f);
            AddCaveSample(Center, Radius * RadiusNoise);
        }
    }
}

void ADROreFieldActor::GeneratePillarCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random)
{
    const float PillarRadius = RandomNormalRange(Random, Config.PillarRadiusRange);
    const float PillarHeight = RandomNormalRange(Random, Config.PillarHeightRange);
    const float CaveRadius = RandomNormalRange(Random, Config.PillarCaveRadiusRange);
    const float Spacing = RandomNormalRange(Random, Config.PillarSpacingRange);
    const float RingRadius = PillarRadius + CaveRadius + Spacing + Config.PillarPositionNoise;
    const int32 RingCount = FMath::Max(6, FMath::CeilToInt(UE_TWO_PI * RingRadius / Spacing));
    const int32 LayerCount = FMath::Max(1, FMath::CeilToInt(PillarHeight / Spacing) + 1);

    FDROrePillarProtection& Protection = PillarProtections.AddDefaulted_GetRef();
    Protection.LocalCenter = Origin;
    Protection.Radius = PillarRadius;
    Protection.HalfHeight = PillarHeight * 0.5f;

    for (int32 Layer = 0; Layer < LayerCount; ++Layer)
    {
        const float ZAlpha = LayerCount == 1 ? 0.5f : static_cast<float>(Layer) / (LayerCount - 1);
        const float Z = FMath::Lerp(-PillarHeight * 0.5f, PillarHeight * 0.5f, ZAlpha);
        for (int32 Index = 0; Index < RingCount; ++Index)
        {
            const float Angle = UE_TWO_PI * static_cast<float>(Index) / RingCount;
            const float SurfaceWave = FMath::Sin(Angle * 3.f + ZAlpha * UE_TWO_PI * 2.f) * 0.65f +
                FMath::Sin(Angle * 7.f - ZAlpha * UE_TWO_PI * 3.f) * 0.35f;
            const float NoisyRingRadius = RingRadius + SurfaceWave * Config.PillarPositionNoise;
            FVector Center = Origin + FVector(FMath::Cos(Angle) * NoisyRingRadius,
                FMath::Sin(Angle) * NoisyRingRadius,
                Z + Random.FRandRange(-Config.PillarPositionNoise, Config.PillarPositionNoise));
            Center += Random.VRand() * Config.PillarPositionNoise * 0.35f;
            AddCaveSample(Center, CaveRadius * Random.FRandRange(0.65f, 1.35f));
        }
    }

    // 기둥 외곽에 1~3개의 작은 Big Cave 군집을 붙인다.
    FDROreCaveConfig AttachedCave;
    AttachedCave.BigSphereCount = 4;
    AttachedCave.BigMinSphereRadius = CaveRadius * 0.8f;
    AttachedCave.BigMaxSphereRadius = CaveRadius * 1.8f;
    AttachedCave.BigCaveSize = FVector(CaveRadius * 4.f, CaveRadius * 4.f,
        CaveRadius * 2.5f);
    const int32 AttachedCount = Random.RandRange(1, 3);
    for (int32 Index = 0; Index < AttachedCount; ++Index)
    {
        const float Angle = Random.FRandRange(0.f, UE_TWO_PI);
        const float Distance = RingRadius + CaveRadius * Random.FRandRange(1.5f, 2.5f);
        const FVector AttachedOrigin = Origin + FVector(FMath::Cos(Angle) * Distance,
            FMath::Sin(Angle) * Distance,
            Random.FRandRange(-PillarHeight * 0.35f, PillarHeight * 0.35f));
        GenerateBigCave(AttachedCave, AttachedOrigin, Random);
    }
}

void ADROreFieldActor::AddConnection(const FVector& Start, const FVector& End, float Radius,
    float Spacing, float BottomRadiusScale, FRandomStream& Random)
{
    const float Distance = FVector::Distance(Start, End);
    const int32 Count = FMath::Max(2, FMath::CeilToInt(Distance / FMath::Max(10.f, Spacing)) + 1);
    const float ZExtent = FMath::Max(Bounds->GetUnscaledBoxExtent().Z, 1.f);
    const FVector Direction = (End - Start).GetSafeNormal();
    const FVector Side = FVector::CrossProduct(Direction,
        FMath::Abs(Direction.Z) < 0.9f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
    const FVector Up = FVector::CrossProduct(Direction, Side).GetSafeNormal();
    const float CurveStrength = FMath::Min(Distance * 0.22f, FMath::Max(Radius * 2.5f, Spacing));
    const float SidePhase = Random.FRandRange(0.f, UE_TWO_PI);
    const float UpPhase = Random.FRandRange(0.f, UE_TWO_PI);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(Count - 1);
        const float EndpointFade = FMath::Sin(Alpha * UE_PI);
        const float SideNoise = FMath::Sin(Alpha * UE_TWO_PI * 1.7f + SidePhase) * 0.7f +
            FMath::Sin(Alpha * UE_TWO_PI * 4.3f - UpPhase) * 0.3f;
        const float UpNoise = FMath::Sin(Alpha * UE_TWO_PI * 1.3f + UpPhase) * 0.65f +
            FMath::Sin(Alpha * UE_TWO_PI * 3.7f + SidePhase) * 0.35f;
        const FVector Center = FMath::Lerp(Start, End, Alpha) +
            (Side * SideNoise + Up * UpNoise) * CurveStrength * EndpointFade;
        const float HeightAlpha = FMath::Clamp((Center.Z + ZExtent) / (ZExtent * 2.f), 0.f, 1.f);
        const float RadiusScale = FMath::Lerp(BottomRadiusScale, 1.f, HeightAlpha);
        const float RadiusNoise = Random.FRandRange(0.8f, 1.2f);
        AddCaveSample(Center, Radius * RadiusScale * RadiusNoise);
    }
}

void ADROreFieldActor::AddCaveSample(const FVector& Center, float Radius)
{
    if (Radius <= 0.f || IntersectsProtectedPillar(Center, Radius))
    {
        return;
    }

    FDROreCaveSample& Sample = CaveSamples.AddDefaulted_GetRef();
    Sample.LocalCenter = Center;
    Sample.Radius = Radius;
}

bool ADROreFieldActor::IntersectsProtectedPillar(const FVector& Center, float Radius) const
{
    for (const FDROrePillarProtection& Protection : PillarProtections)
    {
        const FVector2D Offset(Center.X - Protection.LocalCenter.X,
            Center.Y - Protection.LocalCenter.Y);
        if (Offset.Size() < Protection.Radius + Radius &&
            FMath::Abs(Center.Z - Protection.LocalCenter.Z) < Protection.HalfHeight + Radius)
        {
            return true;
        }
    }
    return false;
}

void ADROreFieldActor::GenerateCave()
{
    if (!HasAuthority() || CaveSamples.IsEmpty())
    {
        return;
    }

    UWorld* World = GetWorld();
    UDRVoxelTerrainSubsystem* Terrain = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
    if (!Terrain)
    {
        return;
    }

    ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>();
    const FTransform BoundsTransform = Bounds->GetComponentTransform();
    const FVector Scale = BoundsTransform.GetScale3D().GetAbs();
    const float RadiusScale = FMath::Min3(Scale.X, Scale.Y, Scale.Z);

    for (; NextCaveSampleIndex < CaveSamples.Num(); ++NextCaveSampleIndex)
    {
        const FDROreCaveSample& Sample = CaveSamples[NextCaveSampleIndex];
        FDRTerrainDigOperation Operation;
        const FVector WorldCenter = BoundsTransform.TransformPosition(Sample.LocalCenter);
        if (!Terrain->RequestDigAtLocation(WorldCenter, Sample.Radius * RadiusScale, &Operation))
        {
            if (CaveGenerationRetryCount++ < 50)
            {
                GetWorldTimerManager().SetTimer(CaveGenerationRetryTimer, this,
                    &ThisClass::GenerateCave, 0.2f, false);
            }
            return;
        }

        if (MiningGameState)
        {
            MiningGameState->RegisterTerrainDig(Operation);
        }
    }

    GetWorldTimerManager().ClearTimer(CaveGenerationRetryTimer);
}

// 시드와 섹터 설정으로 모든 스폰 위치 선계산
void ADROreFieldActor::BuildSpawnPoints()
{
    RuntimeSectors.Reset();
    NextSpawnPointId = 0;

    if (!Definition->SectorTable) return;

    FRandomStream Random(Definition->RandomSeed);
    TArray<FName> RowNames = Definition->SectorTable->GetRowNames();
    RowNames.Sort([](const FName& A, const FName& B)
    {
        return A.ToString() < B.ToString();
    });

    for (const FName& RowName : RowNames)
    {
        const FDROreDepthSector* Config = Definition->SectorTable->FindRow<FDROreDepthSector>(
            RowName, TEXT("BuildSpawnPoints"));
        if (!Config || Config->SpawnCount <= 0 || Config->EndDepth <= Config->StartDepth)
        {
            continue;
        }

        FDROreRuntimeSector& Runtime = RuntimeSectors.AddDefaulted_GetRef();
        Runtime.StartDepth = Config->StartDepth;
        Runtime.SpawnPoints.Reserve(Config->SpawnCount);

        TArray<FTransform> ClusterCenters;
        if (Definition->bUseOreClusters)
        {
            const int32 MinimumCount = FMath::Min(Definition->OreClusterCountRange.X,
                Definition->OreClusterCountRange.Y);
            const int32 MaximumCount = FMath::Max(Definition->OreClusterCountRange.X,
                Definition->OreClusterCountRange.Y);
            const int32 ClusterCount = Random.RandRange(FMath::Max(0, MinimumCount),
                FMath::Max(0, MaximumCount));
            for (int32 Index = 0; Index < ClusterCount; ++Index)
            {
                ClusterCenters.Add(MakeSpawnTransform(*Config, Random));
            }
        }

        for (int32 Index = 0; Index < Config->SpawnCount; ++Index)
        {
            const FDROreWeight* Ore = ChooseOre(*Config, Random);
            if (!Ore || !Ore->ItemDefinition || !Ore->OreActorClass)
            {
                continue;
            }

            FDROreSpawnPoint& Point = Runtime.SpawnPoints.AddDefaulted_GetRef();
            if (!ClusterCenters.IsEmpty() && Random.FRand() < Definition->OreClusterChance)
            {
                const FTransform& Center = ClusterCenters[Random.RandRange(
                    0, ClusterCenters.Num() - 1)];
                const float Angle = Random.FRandRange(0.f, UE_TWO_PI);
                const float Distance = RandomNormalRange(Random,
                    FVector2D(0.f, Definition->OreClusterRadius));
                const FVector Offset =
                    Center.GetUnitAxis(EAxis::Y) * FMath::Cos(Angle) * Distance +
                    Center.GetUnitAxis(EAxis::Z) * FMath::Sin(Angle) * Distance;
                Point.Transform = Center;
                Point.Transform.AddToTranslation(Offset);
            }
            else
            {
                Point.Transform = MakeSpawnTransform(*Config, Random);
            }
            Point.SpawnPointId = NextSpawnPointId++;
            Point.ItemDefinition = Ore->ItemDefinition;
            Point.OreActorClass = Ore->OreActorClass;
        }
    }
}

// 실제로 파인 최심도에 도달한 섹터 활성화
void ADROreFieldActor::UpdateActiveSectors()
{
    if (!HasAuthority() || !Definition) return;

    const float MaximumDepth = DeepestDugDepth + Definition->LoadAheadDistance;

    for (FDROreRuntimeSector& Sector : RuntimeSectors)
    {
        if (!Sector.bActive && Sector.StartDepth <= MaximumDepth)
        {
            SetSectorActive(Sector, true);
        }
    }
}

void ADROreFieldActor::SetSectorActive(FDROreRuntimeSector& Sector, bool bNewActive)
{
    if (Sector.bActive == bNewActive)
    {
        return;
    }

    UDROrePoolSubsystem* Pool = GetWorld()->GetSubsystem<UDROrePoolSubsystem>();
    if (!Pool)
    {
        return;
    }

    Sector.bActive = bNewActive;

    for (FDROreSpawnPoint& Point : Sector.SpawnPoints)
    {
        if (bNewActive)
        {
            if (Point.bDepleted)
            {
                continue;
            }

            Point.ActiveActor = Pool->AcquireOre(Point.ItemDefinition, Point.OreActorClass,
                Point.Transform, this, Point.SpawnPointId);
        }
        else if (Point.ActiveActor)
        {
            Pool->ReleaseOre(Point.ActiveActor);
            Point.ActiveActor = nullptr;
        }
    }
}

FTransform ADROreFieldActor::MakeSpawnTransform(const FDROreDepthSector& Sector, FRandomStream& Random) const
{
    const FVector Extent = Bounds->GetUnscaledBoxExtent();
    const float Padding = FMath::Max(0.f, Definition->BoundsPadding);
    const float XExtent = FMath::Max(0.f, Extent.X - Padding);
    const float YExtent = FMath::Max(0.f, Extent.Y - Padding);
    const float MaximumDepth = Extent.Z * 2.f;
    const float StartDepth = FMath::Clamp(Sector.StartDepth, 0.f, MaximumDepth);
    const float EndDepth = FMath::Clamp(Sector.EndDepth, StartDepth, MaximumDepth);
    const float LocalZ = Extent.Z - Random.FRandRange(StartDepth, EndDepth);

    FVector LocalLocation;
    FRotator LocalRotation;

    if (Definition->PlacementMode == EDROrePlacementMode::CaveSurface && !CaveSamples.IsEmpty())
    {
        const FDROreCaveSample& Sample = CaveSamples[Random.RandRange(0, CaveSamples.Num() - 1)];
        const float SurfaceAngle = Random.FRandRange(0.f, UE_TWO_PI);
        const FVector SurfaceDirection = FVector(Random.FRandRange(-0.15f, 0.15f),
            FMath::Cos(SurfaceAngle), FMath::Sin(SurfaceAngle)).GetSafeNormal();
        LocalLocation = Sample.LocalCenter + SurfaceDirection * Sample.Radius;
        LocalRotation = (-SurfaceDirection).Rotation();
    }
    else if (Definition->PlacementMode == EDROrePlacementMode::SideWalls)
    {
        // Box의 네 측면 중 하나를 균등하게 고른다.
        const int32 Side = Random.RandRange(0, 3);
        if (Side < 2)
        {
            const float Sign = Side == 0 ? -1.f : 1.f;
            LocalLocation = FVector(Sign * XExtent, Random.FRandRange(-YExtent, YExtent), LocalZ);
            LocalRotation = FRotator(0.f, Sign < 0.f ? 0.f : 180.f, 0.f);
        }
        else
        {
            const float Sign = Side == 2 ? -1.f : 1.f;
            LocalLocation = FVector(Random.FRandRange(-XExtent, XExtent), Sign * YExtent, LocalZ);
            LocalRotation = FRotator(0.f, Sign < 0.f ? 90.f : -90.f, 0.f);
        }
    }
    else
    {
        LocalLocation = FVector(Random.FRandRange(-XExtent, XExtent),
                                Random.FRandRange(-YExtent, YExtent), LocalZ);
        LocalRotation = FRotator(Random.FRandRange(-180.f, 180.f),
            Random.FRandRange(-180.f, 180.f),
            Random.FRandRange(-180.f, 180.f));
    }

    const FTransform BoundsTransform = Bounds->GetComponentTransform();
    return FTransform(BoundsTransform.TransformRotation(LocalRotation.Quaternion()),
        BoundsTransform.TransformPosition(LocalLocation));
}

const FDROreWeight* ADROreFieldActor::ChooseOre(const FDROreDepthSector& Sector, FRandomStream& Random) const
{
    // 가중치 세팅
    float TotalWeight = 0.f;
    for (const FDROreWeight& Ore : Sector.Ores)
    {
        if (Ore.ItemDefinition && Ore.OreActorClass)
        {
            TotalWeight += FMath::Max(0.f, Ore.Weight);
        }
    }

    if (TotalWeight <= 0.f)
    {
        return nullptr;
    }

    // 누적 가중치에서 선택값이 포함된 항목을 찾는다.
    float Selection = Random.FRandRange(0.f, TotalWeight);
    for (const FDROreWeight& Ore : Sector.Ores)
    {
        if (!Ore.ItemDefinition || !Ore.OreActorClass)
        {
            continue;
        }

        Selection -= FMath::Max(0.f, Ore.Weight);
        if (Selection <= 0.f)
        {
            return &Ore;
        }
    }

    return nullptr;
}
