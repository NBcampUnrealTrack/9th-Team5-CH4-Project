#include "DRVoxelTerrainAreaSyncActor.h"

#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

ADRVoxelTerrainAreaSyncActor::ADRVoxelTerrainAreaSyncActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
}

void ADRVoxelTerrainAreaSyncActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	LastAppliedTerrainRevision = TerrainEditRevision;

	GetWorldTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&ThisClass::ScanVoxelArea,
		ScanInterval,
		true,
		0.f);

	GetWorldTimerManager().SetTimer(
		DepositTimerHandle,
		this,
		&ThisClass::RequestDepositArea,
		DepositInterval,
		true,
		0.f);
}

void ADRVoxelTerrainAreaSyncActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawDebugBox)
	{
		DrawScanDebugBox();
	}

	if (bDrawDepositGridPoints)
	{
		DrawDepositGridPoints();
	}

	if (HasAuthority())
	{
		ProcessServerDepositRequests();
	}
	else
	{
		ApplyPendingDeltaRecords();
	}
}

void ADRVoxelTerrainAreaSyncActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, TerrainEditRevision);
	DOREPLIFETIME(ADRVoxelTerrainAreaSyncActor, DepositDeltaRecords);
}

void ADRVoxelTerrainAreaSyncActor::OnRep_DepositDeltaRecords()
{
	ApplyPendingDeltaRecords();
}

void ADRVoxelTerrainAreaSyncActor::ScanVoxelArea()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
	}

	TMap<uint8, int32> MaterialCounts;
	int32 TotalCount = 0;

	const bool bSuccess = UDRVoxelTerrainQueryLibrary::GetMaterialCountsInBox(
		VoxelWorld,
		GetActorLocation(),
		BoxExtent,
		DepositSettings.SampleStep,
		TeamMaterialIndices,
		MaterialCounts,
		TotalCount);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to scan voxel area."));
		return;
	}

	if (TotalCount <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("No matching solid voxels found in area."));
		return;
	}

	for (const TPair<uint8, int32>& Pair : MaterialCounts)
	{
		const uint8 MaterialIndex = Pair.Key;
		const int32 Count = Pair.Value;
		const float Percent =
			static_cast<float>(Count) /
			static_cast<float>(TotalCount) *
			100.f;

		UE_LOG(
			LogTemp,
			Log,
			TEXT("Material %d: Count=%d Percent=%.2f%%"),
			MaterialIndex,
			Count,
			Percent);
	}
}

void ADRVoxelTerrainAreaSyncActor::RequestDepositArea()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bEnableDepositAccumulation)
	{
		DepositRequests.Reset();
		return;
	}

	if (DepositRequests.Num() > 0)
	{
		return;
	}

	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		DepositRequests.Reset();
		return;
	}

	FDRVoxelDepositInBoxSettings RequestSettings = DepositSettings;
	RequestSettings.RandomSeed = FMath::Rand();

	FDRVoxelDepositInBoxRequest Request;
	const bool bRequestCreated = UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
		VoxelWorld,
		GetActorLocation(),
		BoxExtent,
		RequestSettings,
		Request);

	if (!bRequestCreated)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Failed to create deposit request."));
		return;
	}

	DepositRequests.Add(MoveTemp(Request));
}

void ADRVoxelTerrainAreaSyncActor::ProcessServerDepositRequests()
{
	if (!HasAuthority() || DepositRequests.Num() == 0)
	{
		return;
	}

	int32 ModifiedVoxelCount = 0;
	int32 ScannedColumnCount = 0;
	int32 RemainingRequestCount = 0;
	FDRVoxelDepositDeltaRecord DeltaRecord;

	UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
		DepositRequests,
		MaxDepositScanColumnsPerTick,
		MaxDepositVoxelsPerTick,
		ModifiedVoxelCount,
		ScannedColumnCount,
		DeltaRecord,
		RemainingRequestCount);

	if (ModifiedVoxelCount <= 0 || DeltaRecord.Deltas.Num() == 0)
	{
		return;
	}

	TerrainEditRevision++;
	DeltaRecord.Revision = TerrainEditRevision;
	DepositDeltaRecords.Add(MoveTemp(DeltaRecord));

	TrimReplicatedDeltaRecords();
	ForceNetUpdate();

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Deposit replicated. Revision=%d ModifiedVoxelCount=%d ScannedColumnCount=%d RemainingRequestCount=%d"),
		TerrainEditRevision,
		ModifiedVoxelCount,
		ScannedColumnCount,
		RemainingRequestCount);
}

void ADRVoxelTerrainAreaSyncActor::ApplyPendingDeltaRecords()
{
	if (HasAuthority() || !IsValid(VoxelWorld))
	{
		return;
	}

	for (const FDRVoxelDepositDeltaRecord& DeltaRecord : DepositDeltaRecords)
	{
		if (DeltaRecord.Revision <= LastAppliedTerrainRevision)
		{
			continue;
		}

		int32 AppliedVoxelCount = 0;
		if (UDRVoxelTerrainQueryLibrary::ApplyDepositDeltaRecord(
			VoxelWorld,
			DeltaRecord,
			AppliedVoxelCount))
		{
			LastAppliedTerrainRevision = DeltaRecord.Revision;
		}
	}
}

void ADRVoxelTerrainAreaSyncActor::TrimReplicatedDeltaRecords()
{
	if (MaxReplicatedDeltaRecords <= 0)
	{
		return;
	}

	const int32 ExcessCount = DepositDeltaRecords.Num() - MaxReplicatedDeltaRecords;
	if (ExcessCount > 0)
	{
		DepositDeltaRecords.RemoveAt(0, ExcessCount, EAllowShrinking::No);
	}
}

void ADRVoxelTerrainAreaSyncActor::DrawScanDebugBox() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	DrawDebugBox(
		World,
		GetActorLocation(),
		BoxExtent,
		FColor::Cyan,
		false,
		0.f,
		0,
		3.f);
}

void ADRVoxelTerrainAreaSyncActor::DrawDepositGridPoints() const
{
	UWorld* World = GetWorld();
	const float GridStep = DepositSettings.SampleStep;
	if (!IsValid(World) || GridStep <= 0.f || MaxDebugDepositGridPoints <= 0)
	{
		return;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.IsNearlyZero())
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const FVector Min = Center - AbsExtent;
	const FVector Max = Center + AbsExtent;

	int32 DrawnPointCount = 0;

	for (float X = Min.X; X <= Max.X && DrawnPointCount < MaxDebugDepositGridPoints; X += GridStep)
	{
		for (float Y = Min.Y; Y <= Max.Y && DrawnPointCount < MaxDebugDepositGridPoints; Y += GridStep)
		{
			for (float Z = Min.Z; Z <= Max.Z && DrawnPointCount < MaxDebugDepositGridPoints; Z += GridStep)
			{
				DrawDebugPoint(
					World,
					FVector(X, Y, Z),
					DepositGridPointSize,
					DepositGridPointColor,
					false,
					0.f);

				DrawnPointCount++;
			}
		}
	}
}
