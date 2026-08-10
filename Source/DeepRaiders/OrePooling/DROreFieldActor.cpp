#include "DROreFieldActor.h"

#include "Components/BoxComponent.h"
#include "DROrePoolActor.h"
#include "DROrePoolSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

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

    BuildSpawnPoints();
    PrewarmPool();
    UpdateActiveSectors();

    if (UDRVoxelTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UDRVoxelTerrainSubsystem>())
    {
        Terrain->OnTerrainDug.AddUObject(this, &ThisClass::ReportTerrainDig);

        // Field 생성 전에 적용된 서버 채굴 이력 반영
        for (const FDRTerrainDigOperation& Operation : Terrain->GetDigHistory())
        {
            ReportTerrainDig(Operation.Location, Operation.Radius);
        }
    }
}

void ADROreFieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

        for (int32 Index = 0; Index < Config->SpawnCount; ++Index)
        {
            const FDROreWeight* Ore = ChooseOre(*Config, Random);
            if (!Ore || !Ore->ItemDefinition || !Ore->OreActorClass)
            {
                continue;
            }

            FDROreSpawnPoint& Point = Runtime.SpawnPoints.AddDefaulted_GetRef();
            Point.Transform = MakeSpawnTransform(*Config, Random);
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

FTransform ADROreFieldActor::MakeSpawnTransform(const FDROreDepthSector& Sector,
                                                FRandomStream& Random) const
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

    if (Definition->PlacementMode == EDROrePlacementMode::SideWalls)
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

const FDROreWeight* ADROreFieldActor::ChooseOre(const FDROreDepthSector& Sector,
    FRandomStream& Random) const
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
