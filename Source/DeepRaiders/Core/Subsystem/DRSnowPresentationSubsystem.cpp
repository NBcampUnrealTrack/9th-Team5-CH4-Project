#include "DRSnowPresentationSubsystem.h"

#include "DeepRaiders/Core/Settings/DRSnowPresentationSettings.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"
#include "VoxelRender/VoxelMaterialInterface.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "VoxelWorld.h"

DEFINE_LOG_CATEGORY_STATIC(LogDRSnowPresentation, Log, All);

namespace
{
const FName SnowWPOEnabledParameterName(TEXT("SnowWPO_Enabled"));
const FName SnowPreviousSurfaceParameterName(TEXT("SnowPreviousSurface"));
const FName SnowPreviousSurfaceComponentTag(TEXT("DRSnowPreviousSurface"));

float GetPresentationRadius(const FDRSnowAddOperation& Operation)
{
	if (Operation.EditTool != EDRSnowVoxelEditTool::OrientedBoxTool)
	{
		return Operation.Radius;
	}

	return FMath::Max3(
		static_cast<float>(Operation.BoxExtent.X),
		static_cast<float>(Operation.BoxExtent.Y),
		static_cast<float>(Operation.BoxExtent.Z));
}
}

bool UDRSnowPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

void UDRSnowPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SlotEndTimes.Init(0.0, MaxTransitionSlots);
	const UDRSnowPresentationSettings* Settings = GetDefault<UDRSnowPresentationSettings>();
	if (!IsValid(Settings) || Settings->WPOParameterCollection.IsNull())
	{
		UE_LOG(LogDRSnowPresentation, Verbose, TEXT("Snow WPO parameter collection is not configured."));
		return;
	}

	ParameterCollection = Settings->WPOParameterCollection.LoadSynchronous();
	bParameterContractValid = ValidateParameterContract();
	if (bParameterContractValid)
	{
		ResetPresentation();
	}
}

void UDRSnowPresentationSubsystem::Deinitialize()
{
	ResetPreviousSurfaceSnapshots(true);

	if (bParameterContractValid)
	{
		if (UMaterialParameterCollectionInstance* Instance = GetCollectionInstance())
		{
			Instance->SetScalarParameterValue(SnowWPOEnabledParameterName, 0.f);
		}
	}

	bParameterContractValid = false;
	ParameterCollection = nullptr;
	SlotEndTimes.Reset();
	NextSlotIndex = 0;
	Super::Deinitialize();
}

void UDRSnowPresentationSubsystem::PresentSnowAdd(const FDRSnowAddOperation& Operation)
{
	if (!bParameterContractValid || Operation.Amount <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMaterialParameterCollectionInstance* Instance = GetCollectionInstance();
	const UDRSnowPresentationSettings* Settings = GetDefault<UDRSnowPresentationSettings>();
	const float Radius = GetPresentationRadius(Operation);
	if (!IsValid(World) || !IsValid(Instance) || !IsValid(Settings) || Radius <= 0.f)
	{
		return;
	}

	const float FullStrengthRadius = FMath::Max(1.f, Settings->FullStrengthRadius);
	const float RadiusAlpha = FMath::SmoothStep(0.f, FullStrengthRadius, Radius);
	const float Strength = FMath::Lerp(
		FMath::Clamp(Settings->MinimumStrength, 0.f, 1.f),
		1.f,
		RadiusAlpha);
	const float MinimumDuration = FMath::Max(0.01f, Settings->MinimumDuration);
	const float MaximumDuration = FMath::Max(MinimumDuration, Settings->MaximumDuration);
	const float Duration = FMath::Lerp(MinimumDuration, MaximumDuration, RadiusAlpha);
	const float CollapseHeight = FMath::Min(
		Radius * FMath::Max(0.f, Settings->CollapseHeightRatio),
		FMath::Max(0.f, Settings->MaximumCollapseHeight));
	const float EdgeWidth = FMath::Max(
		FMath::Max(0.f, Settings->MinimumEdgeWidth),
		Radius * FMath::Max(0.f, Settings->EdgeWidthRatio));
	const double CurrentTime = World->GetTimeSeconds();
	const float StartTime = static_cast<float>(CurrentTime + FMath::Max(0.f, Settings->StartDelay));
	const int32 SlotIndex = AcquireSlot(CurrentTime);
	const double EndTime = StartTime + Duration;

	if (Settings->bEnablePreviousSurfaceSnapshots)
	{
		AVoxelWorld* VoxelWorld = ResolveVoxelWorld(Operation.VoxelWorldName);
		if (!IsValid(VoxelWorld) || !CapturePreviousSurface(
			*VoxelWorld,
			Operation,
			Radius,
			EdgeWidth,
			EndTime))
		{
			Instance->SetVectorParameterValue(GetCenterRadiusParameterName(SlotIndex), FLinearColor::Transparent);
			Instance->SetVectorParameterValue(GetNormalStartTimeParameterName(SlotIndex), FLinearColor::Transparent);
			Instance->SetVectorParameterValue(GetTimingParameterName(SlotIndex), FLinearColor::Transparent);
			SlotEndTimes[SlotIndex] = 0.0;
			UE_LOG(
				LogDRSnowPresentation,
				Warning,
				TEXT("Snow add presentation was skipped because its previous surface could not be captured completely."));
			return;
		}
	}

	FVector SurfaceNormal(Operation.SurfaceNormal);
	SurfaceNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();

	Instance->SetVectorParameterValue(
		GetCenterRadiusParameterName(SlotIndex),
		FLinearColor(
			static_cast<float>(Operation.WorldLocation.X),
			static_cast<float>(Operation.WorldLocation.Y),
			static_cast<float>(Operation.WorldLocation.Z),
			Radius));
	Instance->SetVectorParameterValue(
		GetNormalStartTimeParameterName(SlotIndex),
		FLinearColor(
			static_cast<float>(SurfaceNormal.X),
			static_cast<float>(SurfaceNormal.Y),
			static_cast<float>(SurfaceNormal.Z),
			StartTime));
	Instance->SetVectorParameterValue(
		GetTimingParameterName(SlotIndex),
		FLinearColor(Duration, CollapseHeight, EdgeWidth, Strength));

	SlotEndTimes[SlotIndex] = EndTime;
}

void UDRSnowPresentationSubsystem::ResetPresentation()
{
	if (!bParameterContractValid)
	{
		ResetPreviousSurfaceSnapshots(false);
		return;
	}

	ResetPreviousSurfaceSnapshots(false);

	UMaterialParameterCollectionInstance* Instance = GetCollectionInstance();
	if (!IsValid(Instance))
	{
		return;
	}

	Instance->SetScalarParameterValue(SnowWPOEnabledParameterName, 0.f);
	for (int32 SlotIndex = 0; SlotIndex < MaxTransitionSlots; ++SlotIndex)
	{
		Instance->SetVectorParameterValue(GetCenterRadiusParameterName(SlotIndex), FLinearColor::Transparent);
		Instance->SetVectorParameterValue(GetNormalStartTimeParameterName(SlotIndex), FLinearColor::Transparent);
		Instance->SetVectorParameterValue(GetTimingParameterName(SlotIndex), FLinearColor::Transparent);
	}
	Instance->SetScalarParameterValue(SnowWPOEnabledParameterName, 1.f);

	SlotEndTimes.Init(0.0, MaxTransitionSlots);
	NextSlotIndex = 0;
}

bool UDRSnowPresentationSubsystem::ValidateParameterContract() const
{
	if (!IsValid(ParameterCollection))
	{
		return false;
	}

	const TArray<FName> ScalarParameterNames = ParameterCollection->GetScalarParameterNames();
	const TArray<FName> VectorParameterNames = ParameterCollection->GetVectorParameterNames();
	bool bValid = ScalarParameterNames.Contains(SnowWPOEnabledParameterName);
	for (int32 SlotIndex = 0; SlotIndex < MaxTransitionSlots; ++SlotIndex)
	{
		bValid &= VectorParameterNames.Contains(GetCenterRadiusParameterName(SlotIndex));
		bValid &= VectorParameterNames.Contains(GetNormalStartTimeParameterName(SlotIndex));
		bValid &= VectorParameterNames.Contains(GetTimingParameterName(SlotIndex));
	}

	if (!bValid)
	{
		UE_LOG(
			LogDRSnowPresentation,
			Error,
			TEXT("Snow WPO parameter collection does not match the required 8-slot parameter contract: %s"),
			*ParameterCollection->GetPathName());
	}
	return bValid;
}

UMaterialParameterCollectionInstance* UDRSnowPresentationSubsystem::GetCollectionInstance() const
{
	UWorld* World = GetWorld();
	return IsValid(World) && IsValid(ParameterCollection)
		? World->GetParameterCollectionInstance(ParameterCollection)
		: nullptr;
}

int32 UDRSnowPresentationSubsystem::AcquireSlot(const double CurrentTime)
{
	if (SlotEndTimes.Num() != MaxTransitionSlots)
	{
		SlotEndTimes.Init(0.0, MaxTransitionSlots);
	}

	for (int32 Offset = 0; Offset < MaxTransitionSlots; ++Offset)
	{
		const int32 SlotIndex = (NextSlotIndex + Offset) % MaxTransitionSlots;
		if (SlotEndTimes[SlotIndex] <= CurrentTime)
		{
			NextSlotIndex = (SlotIndex + 1) % MaxTransitionSlots;
			return SlotIndex;
		}
	}

	int32 OldestSlotIndex = 0;
	for (int32 SlotIndex = 1; SlotIndex < MaxTransitionSlots; ++SlotIndex)
	{
		if (SlotEndTimes[SlotIndex] < SlotEndTimes[OldestSlotIndex])
		{
			OldestSlotIndex = SlotIndex;
		}
	}
	NextSlotIndex = (OldestSlotIndex + 1) % MaxTransitionSlots;
	return OldestSlotIndex;
}

AVoxelWorld* UDRSnowPresentationSubsystem::ResolveVoxelWorld(const FName VoxelWorldName) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> Iterator(World); Iterator; ++Iterator)
	{
		AVoxelWorld* VoxelWorld = *Iterator;
		if (IsValid(VoxelWorld) && VoxelWorld->IsCreated() &&
			(VoxelWorldName.IsNone() || VoxelWorld->GetFName() == VoxelWorldName))
		{
			return VoxelWorld;
		}
	}

	return nullptr;
}

bool UDRSnowPresentationSubsystem::CapturePreviousSurface(
	AVoxelWorld& VoxelWorld,
	const FDRSnowAddOperation& Operation,
	const float Radius,
	const float EdgeWidth,
	const double EndTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnowPresentation_CapturePreviousSurface);
	CleanupExpiredSnapshots();

	const UDRSnowPresentationSettings* Settings = GetDefault<UDRSnowPresentationSettings>();
	if (!IsValid(Settings))
	{
		return false;
	}

	const int32 FirstCapturedSnapshotIndex = ActivePreviousSurfaceSnapshots.Num();
	const auto RollbackCapture = [this, FirstCapturedSnapshotIndex]()
	{
		while (ActivePreviousSurfaceSnapshots.Num() > FirstCapturedSnapshotIndex)
		{
			ReleaseSnapshot(ActivePreviousSurfaceSnapshots.Num() - 1);
		}
	};

	const float BoundsPadding = EdgeWidth + VoxelWorld.VoxelSize * 2.f;
	FBox CaptureBounds;
	if (Operation.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool)
	{
		const FBox LocalBounds(-FVector(Operation.BoxExtent), FVector(Operation.BoxExtent));
		CaptureBounds = LocalBounds.TransformBy(FTransform(Operation.BoxRotation, Operation.WorldLocation));
		CaptureBounds = CaptureBounds.ExpandBy(BoundsPadding);
	}
	else
	{
		CaptureBounds = FBox::BuildAABB(Operation.WorldLocation, FVector(Radius + BoundsPadding));
	}

	TInlineComponentArray<UVoxelProceduralMeshComponent*> SourceComponents(&VoxelWorld);
	int32 CapturedComponentCount = 0;
	for (UVoxelProceduralMeshComponent* SourceComponent : SourceComponents)
	{
		if (!IsValid(SourceComponent) ||
			SourceComponent->ComponentHasTag(SnowPreviousSurfaceComponentTag) ||
			!SourceComponent->IsRegistered() ||
			!SourceComponent->IsVisible() ||
			!SourceComponent->Bounds.GetBox().Intersect(CaptureBounds))
		{
			continue;
		}

		UVoxelProceduralMeshComponent* SnapshotComponent = AcquireSnapshotComponent(
			VoxelWorld,
			*SourceComponent,
			FMath::Max(1, Settings->MaximumSnapshotComponents));
		if (!IsValid(SnapshotComponent))
		{
			UE_LOG(
				LogDRSnowPresentation,
				Warning,
				TEXT("Previous-surface snapshot capture exceeded the component limit (%d) and was aborted."),
				Settings->MaximumSnapshotComponents);
			RollbackCapture();
			return false;
		}

		SnapshotComponent->SetWorldTransform(SourceComponent->GetComponentTransform());
		if (!SnapshotComponent->CopyRenderSectionsFrom(
			*SourceComponent,
			EVoxelProcMeshSectionUpdate::DelayUpdate))
		{
			RecycleSnapshotComponent(SnapshotComponent);
			continue;
		}
		if (!ConfigureSnapshotMaterials(*SnapshotComponent))
		{
			RecycleSnapshotComponent(SnapshotComponent);
			RollbackCapture();
			return false;
		}
		SnapshotComponent->FinishSectionsUpdates();
		SnapshotComponent->SetVisibility(true, true);

		FDRSnowPreviousSurfaceSnapshot& Snapshot = ActivePreviousSurfaceSnapshots.Emplace_GetRef();
		Snapshot.Component = SnapshotComponent;
		Snapshot.EndTime = EndTime + FMath::Max(0.f, Settings->SnapshotLifetimePadding);
		CapturedComponentCount++;
	}

	if (CapturedComponentCount > 0)
	{
		UE_LOG(
			LogDRSnowPresentation,
			VeryVerbose,
			TEXT("Captured %d previous-surface voxel components for snow add presentation."),
			CapturedComponentCount);
		ScheduleSnapshotCleanup();
	}
	return CapturedComponentCount > 0;
}

UVoxelProceduralMeshComponent* UDRSnowPresentationSubsystem::AcquireSnapshotComponent(
	AVoxelWorld& VoxelWorld,
	const UVoxelProceduralMeshComponent& SourceComponent,
	const int32 MaximumSnapshotComponents)
{
	for (int32 Index = AvailablePreviousSurfaceComponents.Num() - 1; Index >= 0; --Index)
	{
		UVoxelProceduralMeshComponent* Component = AvailablePreviousSurfaceComponents[Index];
		if (!IsValid(Component))
		{
			AvailablePreviousSurfaceComponents.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (Component->GetOwner() == &VoxelWorld && Component->GetClass() == SourceComponent.GetClass())
		{
			AvailablePreviousSurfaceComponents.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			return Component;
		}
	}

	if (ActivePreviousSurfaceSnapshots.Num() + AvailablePreviousSurfaceComponents.Num() >= MaximumSnapshotComponents)
	{
		return nullptr;
	}

	UVoxelProceduralMeshComponent* Component = NewObject<UVoxelProceduralMeshComponent>(
		&VoxelWorld,
		SourceComponent.GetClass(),
		NAME_None,
		RF_Transient);
	if (!IsValid(Component))
	{
		return nullptr;
	}

	Component->ComponentTags.Add(SnowPreviousSurfaceComponentTag);
	Component->SetupAttachment(VoxelWorld.GetRootComponent());
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->CastShadow = false;
	Component->bUseAsOccluder = false;
	Component->RegisterComponent();
	return Component;
}

bool UDRSnowPresentationSubsystem::ConfigureSnapshotMaterials(
	UVoxelProceduralMeshComponent& SnapshotComponent)
{
	bool bMaterialsValid = true;
	bool bMissingPreviousSurfaceParameter = false;
	SnapshotComponent.IterateSectionsSettings(
		[this, &SnapshotComponent, &bMaterialsValid, &bMissingPreviousSurfaceParameter](
			FVoxelProcMeshSectionSettings& SectionSettings)
	{
		if (!bMaterialsValid)
		{
			return;
		}

		UMaterialInterface* SourceMaterial = SectionSettings.Material.IsValid()
			? SectionSettings.Material->GetMaterial()
			: nullptr;
		float PreviousSurfaceValue = 0.f;
		if (!IsValid(SourceMaterial) || !SourceMaterial->GetScalarParameterValue(
			FMaterialParameterInfo(SnowPreviousSurfaceParameterName), PreviousSurfaceValue))
		{
			bMaterialsValid = false;
			bMissingPreviousSurfaceParameter = true;
			return;
		}

		UMaterialInterface* ParentMaterial = SourceMaterial;
		UMaterialInstanceDynamic* SourceDynamicMaterial = Cast<UMaterialInstanceDynamic>(SourceMaterial);
		if (IsValid(SourceDynamicMaterial) && IsValid(SourceDynamicMaterial->Parent))
		{
			ParentMaterial = SourceDynamicMaterial->Parent;
		}

		UMaterialInstanceDynamic* SnapshotMaterial = UMaterialInstanceDynamic::Create(
			ParentMaterial,
			&SnapshotComponent);
		if (!IsValid(SnapshotMaterial))
		{
			bMaterialsValid = false;
			return;
		}
		if (IsValid(SourceDynamicMaterial))
		{
			SnapshotMaterial->CopyParameterOverrides(SourceDynamicMaterial);
		}
		SnapshotMaterial->SetScalarParameterValue(SnowPreviousSurfaceParameterName, 1.f);
		SectionSettings.Material = FVoxelMaterialInterfaceManager::Get().CreateMaterial(SnapshotMaterial);
	});

	if (!bMaterialsValid)
	{
		if (bMissingPreviousSurfaceParameter && !bLoggedMissingPreviousSurfaceParameter)
		{
			UE_LOG(
				LogDRSnowPresentation,
				Warning,
				TEXT("Previous-surface snapshots require scalar parameter '%s' in every voxel material section."),
				*SnowPreviousSurfaceParameterName.ToString());
			bLoggedMissingPreviousSurfaceParameter = true;
		}
		else if (!bMissingPreviousSurfaceParameter)
		{
			UE_LOG(LogDRSnowPresentation, Warning, TEXT("Failed to create a previous-surface snapshot material."));
		}
	}
	return bMaterialsValid;
}

void UDRSnowPresentationSubsystem::RecycleSnapshotComponent(
	UVoxelProceduralMeshComponent* SnapshotComponent)
{
	if (!IsValid(SnapshotComponent))
	{
		return;
	}

	SnapshotComponent->SetVisibility(false, true);
	SnapshotComponent->ClearSections(EVoxelProcMeshSectionUpdate::UpdateNow);
	AvailablePreviousSurfaceComponents.AddUnique(SnapshotComponent);
}

void UDRSnowPresentationSubsystem::CleanupExpiredSnapshots()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	for (int32 Index = ActivePreviousSurfaceSnapshots.Num() - 1; Index >= 0; --Index)
	{
		if (ActivePreviousSurfaceSnapshots[Index].EndTime <= CurrentTime)
		{
			ReleaseSnapshot(Index);
		}
	}
	ScheduleSnapshotCleanup();
}

void UDRSnowPresentationSubsystem::ScheduleSnapshotCleanup()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PreviousSurfaceCleanupTimer);
	if (ActivePreviousSurfaceSnapshots.IsEmpty())
	{
		return;
	}

	double EarliestEndTime = ActivePreviousSurfaceSnapshots[0].EndTime;
	for (const FDRSnowPreviousSurfaceSnapshot& Snapshot : ActivePreviousSurfaceSnapshots)
	{
		EarliestEndTime = FMath::Min(EarliestEndTime, Snapshot.EndTime);
	}
	const float Delay = FMath::Max(0.001f, static_cast<float>(EarliestEndTime - World->GetTimeSeconds()));
	World->GetTimerManager().SetTimer(
		PreviousSurfaceCleanupTimer,
		this,
		&ThisClass::CleanupExpiredSnapshots,
		Delay,
		false);
}

void UDRSnowPresentationSubsystem::ReleaseSnapshot(const int32 SnapshotIndex)
{
	if (!ActivePreviousSurfaceSnapshots.IsValidIndex(SnapshotIndex))
	{
		return;
	}

	UVoxelProceduralMeshComponent* Component = ActivePreviousSurfaceSnapshots[SnapshotIndex].Component;
	RecycleSnapshotComponent(Component);
	ActivePreviousSurfaceSnapshots.RemoveAtSwap(SnapshotIndex, 1, EAllowShrinking::No);
}

void UDRSnowPresentationSubsystem::ResetPreviousSurfaceSnapshots(const bool bDestroyComponents)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreviousSurfaceCleanupTimer);
	}

	for (const FDRSnowPreviousSurfaceSnapshot& Snapshot : ActivePreviousSurfaceSnapshots)
	{
		RecycleSnapshotComponent(Snapshot.Component);
	}
	ActivePreviousSurfaceSnapshots.Reset();

	if (bDestroyComponents)
	{
		for (UVoxelProceduralMeshComponent* Component : AvailablePreviousSurfaceComponents)
		{
			if (IsValid(Component))
			{
				Component->DestroyComponent();
			}
		}
		AvailablePreviousSurfaceComponents.Reset();
	}
}

FName UDRSnowPresentationSubsystem::GetCenterRadiusParameterName(const int32 SlotIndex)
{
	return *FString::Printf(TEXT("SnowWPO_CenterRadius_%d"), SlotIndex);
}

FName UDRSnowPresentationSubsystem::GetNormalStartTimeParameterName(const int32 SlotIndex)
{
	return *FString::Printf(TEXT("SnowWPO_NormalStartTime_%d"), SlotIndex);
}

FName UDRSnowPresentationSubsystem::GetTimingParameterName(const int32 SlotIndex)
{
	return *FString::Printf(TEXT("SnowWPO_Timing_%d"), SlotIndex);
}
