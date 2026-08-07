#include "DROreFieldActor.h"

#include "Components/BoxComponent.h"
#include "DROrePoolActor.h"
#include "DROrePoolSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ADROreFieldActor::ADROreFieldActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetBoxExtent(FVector(2000.f, 2000.f, 2000.f));
}

void ADROreFieldActor::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority() || !Definition)
    {
        return;
    }

    // 클라이언트에는 위치 목록을 만들지 않는다.
    BuildSpawnPoints();
    UpdateActiveSectors();
    GetWorldTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateActiveSectors,
                                    Definition->UpdateInterval, true);
}

void ADROreFieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(UpdateTimer);

    if (HasAuthority())
    {
        for (FDROreRuntimeSector& Sector : RuntimeSectors)
        {
            SetSectorActive(Sector, false);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ADROreFieldActor::BuildSpawnPoints()
{
    RuntimeSectors.Reset();
    NextSpawnPointId = 0;

    if (!Definition->SectorTable)
    {
        return;
    }

    FRandomStream Random(Definition->RandomSeed);
    TArray<FName> RowNames = Definition->SectorTable->GetRowNames();
    RowNames.Sort([](const FName& A, const FName& B)
    {
        return A.ToString() < B.ToString();
    });

    for (const FName& RowName : RowNames)
    {
        const FDROreDepthSector* Config = Definition->SectorTable->FindRow<
            FDROreDepthSector>(RowName, TEXT("BuildSpawnPoints"));
        if (!Config || Config->SpawnCount <= 0 || Config->EndDepth <= Config->StartDepth)
        {
            continue;
        }

        FDROreRuntimeSector& Runtime = RuntimeSectors.AddDefaulted_GetRef();
        Runtime.StartDepth = Config->StartDepth;
        Runtime.EndDepth = Config->EndDepth;
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

void ADROreFieldActor::UpdateActiveSectors()
{
    if (!HasAuthority() || !Definition)
    {
        return;
    }

    // 다시 올라가도 이미 도달한 깊이는 유지한다.
    DeepestReachedDepth = FMath::Max(DeepestReachedDepth, FindDeepestPlayerDepth());

    const float MaximumDepth = DeepestReachedDepth + Definition->LoadAheadDistance;

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
            Point.ActiveActor =
                Pool->AcquireOre(Point.ItemDefinition, Point.OreActorClass,
                                 Point.Transform, Point.SpawnPointId);
        }
        else if (Point.ActiveActor)
        {
            Pool->ReleaseOre(Point.ActiveActor);
            Point.ActiveActor = nullptr;
        }
    }
}

float ADROreFieldActor::FindDeepestPlayerDepth() const
{
    const float TopZ = Bounds->GetComponentLocation().Z + Bounds->GetScaledBoxExtent().Z;
    float DeepestDepth = 0.f;

    for (TActorIterator<APawn> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        const APawn* Pawn = *Iterator;
        if (!Pawn || !Pawn->IsPlayerControlled() ||
            !IsPlayerInsideField(Pawn->GetActorLocation()))
        {
            continue;
        }

        DeepestDepth = FMath::Max(DeepestDepth, TopZ - Pawn->GetActorLocation().Z);
    }

    return DeepestDepth;
}

bool ADROreFieldActor::IsPlayerInsideField(const FVector& WorldLocation) const
{
    const FVector LocalLocation =
        Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
    const FVector Extent = Bounds->GetUnscaledBoxExtent();

    return FMath::Abs(LocalLocation.X) <= Extent.X &&
           FMath::Abs(LocalLocation.Y) <= Extent.Y &&
           FMath::Abs(LocalLocation.Z) <= Extent.Z;
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
            LocalLocation =
                FVector(Sign * XExtent, Random.FRandRange(-YExtent, YExtent), LocalZ);
            LocalRotation = FRotator(0.f, Sign < 0.f ? 0.f : 180.f, 0.f);
        }
        else
        {
            const float Sign = Side == 2 ? -1.f : 1.f;
            LocalLocation =
                FVector(Random.FRandRange(-XExtent, XExtent), Sign * YExtent, LocalZ);
            LocalRotation = FRotator(0.f, Sign < 0.f ? 90.f : -90.f, 0.f);
        }
    }
    else
    {
        LocalLocation = FVector(Random.FRandRange(-XExtent, XExtent),
                                Random.FRandRange(-YExtent, YExtent), LocalZ);
        LocalRotation =
            FRotator(Random.FRandRange(-180.f, 180.f), Random.FRandRange(-180.f, 180.f),
                     Random.FRandRange(-180.f, 180.f));
    }

    const FTransform BoundsTransform = Bounds->GetComponentTransform();
    return FTransform(BoundsTransform.TransformRotation(LocalRotation.Quaternion()),
                      BoundsTransform.TransformPosition(LocalLocation));
}

const FDROreWeight* ADROreFieldActor::ChooseOre(const FDROreDepthSector& Sector,
                                                FRandomStream& Random) const
{
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
