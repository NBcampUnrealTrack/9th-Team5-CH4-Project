#include "DRSnowControlZone.h"

#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/UI/HUD/DRPointLocationWidget.h"
#include "EngineUtils.h"
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelData/VoxelDataLock.h"
#include "VoxelIntBox.h"
#include "VoxelMaterial.h"
#include "VoxelUtilities/VoxelMathUtilities.h"
#include "VoxelValue.h"
#include "VoxelWorld.h"

#pragma region Debug

int32 ADRSnowControlZone::GetDominantMaterialIndex(
	const FVoxelMaterial& Material,
	const EVoxelMaterialConfig MaterialConfig)
{
	if (MaterialConfig == EVoxelMaterialConfig::SingleIndex)
	{
		return Material.GetSingleIndex();
		}

	if (MaterialConfig == EVoxelMaterialConfig::MultiIndex)
	{
		const float Blend0 = Material.GetMultiIndex_Blend0_AsFloat();
		const float Blend1 = Material.GetMultiIndex_Blend1_AsFloat();
		const float Blend2 = Material.GetMultiIndex_Blend2_AsFloat();
		const TVoxelStaticArray<float, 4> Strengths =
			FVoxelUtilities::XWayBlend_AlphasToStrengths_Static<4>({ Blend0, Blend1, Blend2 });

		int32 BestChannel = 0;
		float BestStrength = Strengths[0];
		// MultiIndex는 가장 강한 blend channel의 material index를 팀 색으로 해석한다.
		for (int32 Channel = 1; Channel < 4; ++Channel)
		{
			if (Strengths[Channel] > BestStrength)
			{
				BestChannel = Channel;
				BestStrength = Strengths[Channel];
			}
		}

		switch (BestChannel)
		{
		case 0:
			return Material.GetMultiIndex_Index0();
		case 1:
			return Material.GetMultiIndex_Index1();
		case 2:
			return Material.GetMultiIndex_Index2();
		case 3:
			return Material.GetMultiIndex_Index3();
		default:
			return 0;
		}
	}

	return INDEX_NONE;
}

#pragma endregion

ADRSnowControlZone::ADRSnowControlZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	TargetMesh->SetupAttachment(Root);
	TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetMesh->SetGenerateOverlapEvents(false);
	TargetMesh->SetCastShadow(false);
	TargetMesh->SetRenderInMainPass(true);
	TargetMesh->SetRenderInDepthPass(true);
	TargetMesh->SetRenderCustomDepth(false);
	TargetMesh->SetVisibility(false);
	TargetMesh->SetHiddenInGame(true);
	TargetMesh->SetCustomDepthStencilValue(1);
	TargetMesh->bNeverDistanceCull = true;

	CleanupBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CleanupBounds"));
	CleanupBounds->SetupAttachment(Root);
	CleanupBounds->SetBoxExtent(FVector(700.f, 700.f, 400.f));
	CleanupBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CleanupBounds->SetHiddenInGame(true);

	PointLocationWidgetComponent =
		CreateDefaultSubobject<UWidgetComponent>(TEXT("PointLocationWidget"));
	PointLocationWidgetComponent->SetupAttachment(Root);
	PointLocationWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PointLocationWidgetComponent->SetDrawAtDesiredSize(true);
	PointLocationWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRSnowControlZone::BeginPlay()
{
	Super::BeginPlay();
	RefreshControlVisuals();

	RefreshControlRatio();
	SetActorTickEnabled(HasAuthority() && bUpdateControlRatioEveryTick);
	if (HasAuthority() && !bUpdateControlRatioEveryTick)
	{
		GetWorldTimerManager().SetTimer(
			ControlUpdateTimerHandle,
			this,
			&ThisClass::RefreshControlRatio,
			FMath::Max(0.01f, ControlUpdateInterval),
			true);
	}

	InitializeDebug();
}

void ADRSnowControlZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ActivationRevealTimerHandle);
	if (CleanupCancellation.IsValid())
	{
		*CleanupCancellation = true;
	}
	GetWorldTimerManager().ClearTimer(ControlUpdateTimerHandle);
	DeinitializeDebug();
	Super::EndPlay(EndPlayReason);
}

void ADRSnowControlZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshControlRatio();
}

FBox ADRSnowControlZone::GetZoneWorldBounds() const
{
	if (IsValid(TargetMesh) && TargetMesh->GetStaticMesh())
	{
		return TargetMesh->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

FDRSnowControlRatio ADRSnowControlZone::GetControlRatio() const
{
	return CachedControlRatio;
}

void ADRSnowControlZone::RefreshControlRatio()
{
	if (!HasAuthority() || bControlFrozen)
	{
		return;
	}
	if (TargetMesh && TargetMesh->GetStaticMesh())
	{
		if (!bZoneActive)
		{
			return;
		}
		const FDRSnowVoxelMaterialScanResult Scan = ScanVoxelMaterials();
		if (Scan.bTruncated || Scan.ScannedVoxelCount <= 0)
		{
			CachedControlRatio = FDRSnowControlRatio();
			CompletionRatio = 0.f;
			OnRep_ControlState();
			return;
		}
		// 점령 완성도에는 두 플레이어 팀의 눈만 포함한다. 내부 TeamId는 0과 1이다.
		int32 OwnedVoxelCount = 0;
		CachedControlRatio = FDRSnowControlRatio();
		CachedControlRatio.TotalAmount = Scan.FilledVoxelCount;
		CachedControlRatio.NeutralAmount = Scan.NeutralCount + Scan.UnknownCount;
		CachedControlRatio.SampledCellCount = Scan.ScannedVoxelCount;
		for (const FDRSnowVoxelMaterialTeamCount& Team : Scan.Teams)
		{
			if (Team.TeamId == 0 || Team.TeamId == 1)
			{
				OwnedVoxelCount += Team.VoxelCount;
			}
			FDRSnowTeamAmount& Amount = CachedControlRatio.Teams.AddDefaulted_GetRef();
			Amount.TeamId = Team.TeamId;
			Amount.Amount = Team.VoxelCount;
			Amount.Ratio = Scan.FilledVoxelCount > 0
				? float(Team.VoxelCount) / Scan.FilledVoxelCount : 0.f;
		}
		CompletionRatio = float(OwnedVoxelCount) / Scan.ScannedVoxelCount;
		bZoneCompleted |= CompletionRatio >= FMath::Clamp(RequiredCompletionRatio, 0.01f, 1.f);
		OnRep_ControlState();
		if (bZoneCompleted && !bRewardGranted && GetLeadingTeamId() != INDEX_NONE)
		{
			if (ADRMiningGameModeBase* Mode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>())
			{
				Mode->HandleControlZoneCompleted(this);
			}
		}
		return;
	}
	// 메쉬가 없으면 점령 계산을 하지 않고 이전 결과를 초기화한다.
	CachedControlRatio = FDRSnowControlRatio();
	CompletionRatio = 0.f;
	RefreshPointLocationWidget();
}

void ADRSnowControlZone::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADRSnowControlZone, bZoneActive);
	DOREPLIFETIME(ADRSnowControlZone, bZoneCompleted);
	DOREPLIFETIME(ADRSnowControlZone, CompletionRatio);
	DOREPLIFETIME(ADRSnowControlZone, CachedControlRatio);
}

void ADRSnowControlZone::ResetForGame()
{
	GetWorldTimerManager().ClearTimer(ActivationRevealTimerHandle);
	bActivationRevealShown = false;
	bActivationRevealActive = false;
	if (CleanupCancellation.IsValid())
	{
		*CleanupCancellation = true;
		CleanupCancellation.Reset();
	}
	bMaskAttempted = false;
	TargetMask = FDRMeshVoxelMask();
	bControlFrozen = false;
	if (HasAuthority())
	{
		bZoneActive = false;
		bZoneCompleted = false;
		bRewardGranted = false;
		CompletionRatio = 0.f;
		CachedControlRatio = FDRSnowControlRatio();
		OnRep_ControlState();
		ForceNetUpdate();
	}
}

void ADRSnowControlZone::ShowActivationReveal()
{
	if (!HasAuthority() || bActivationRevealShown || ActivationRevealDuration <= 0.f)
	{
		return;
	}
	bActivationRevealShown = true;
	MulticastShowActivationReveal(ActivationRevealDuration);
}

void ADRSnowControlZone::MulticastShowActivationReveal_Implementation(float Duration)
{
	bActivationRevealActive = true;
	RefreshControlVisuals();
	GetWorldTimerManager().SetTimer(
		ActivationRevealTimerHandle,
		this,
		&ThisClass::ClearActivationReveal,
		FMath::Max(Duration, 0.01f),
		false);
}

void ADRSnowControlZone::ClearActivationReveal()
{
	bActivationRevealActive = false;
	RefreshControlVisuals();
}

bool ADRSnowControlZone::PrepareForGame() const
{
	if (!EnsureTargetMask())
	{
		UE_LOG(LogTemp, Error, TEXT("Control zone %s needs a valid closed TargetMesh."), *GetName());
		return false;
	}
	return true;
}

void ADRSnowControlZone::ActivateForPhase(int32 PhaseIndex)
{
	if (!HasAuthority() || bZoneActive || bControlFrozen || PhaseIndex < ActivationPhaseIndex)
	{
		return;
	}
	if (!EnsureTargetMask())
	{
		return;
	}
	// 이후 페이즈에서도 미완성 거점과 기존 점유량을 유지한다.
	bZoneActive = true;
	RefreshControlRatio();
	OnRep_ControlState();
	ForceNetUpdate();
}

int32 ADRSnowControlZone::GetActivationCountdownRemaining(
	int32 PhaseIndex, int32 PhaseElapsedSeconds) const
{
	if (!bShowActivationCountdown || !ShouldActivateForPhase(PhaseIndex))
	{
		return 0;
	}
	return FMath::Max(0, ActivationCountdownSeconds - PhaseElapsedSeconds);
}

void ADRSnowControlZone::FreezeForGameEnd()
{
	if (!HasAuthority())
	{
		return;
	}
	// 종료 시 추가 스캔 없이 플레이 중 마지막 계산 결과를 유지한다.
	bControlFrozen = true;
	ForceNetUpdate();
}

bool ADRSnowControlZone::TryClaimCompletionReward()
{
	if (!HasAuthority() || !bZoneActive || !bZoneCompleted || bRewardGranted
		|| GetLeadingTeamId() == INDEX_NONE)
	{
		return false;
	}
	bRewardGranted = true;
	return true;
}

bool ADRSnowControlZone::EnsureTargetMask() const
{
	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated()
		|| !TargetMesh || !TargetMesh->GetStaticMesh())
	{
		return false;
	}
	const FTransform MeshTransform = TargetMesh->GetComponentTransform();
	if (bMaskAttempted && CachedTargetMesh == TargetMesh->GetStaticMesh()
		&& CachedVoxelWorld == VoxelWorld && CachedMeshTransform.Equals(MeshTransform)
		&& CachedVoxelTransform.Equals(VoxelWorld->GetTransform())
		&& CachedWorldOffset == VoxelWorld->GetWorldOffset() && CachedVoxelSize == VoxelWorld->VoxelSize
		&& CachedMaskLimit == MaxVoxelScanCount)
	{
		return !TargetMask.InsideVoxels.IsEmpty();
	}
	bMaskAttempted = true;
	CachedTargetMesh = TargetMesh->GetStaticMesh();
	CachedVoxelWorld = VoxelWorld;
	CachedMeshTransform = MeshTransform;
	CachedVoxelTransform = VoxelWorld->GetTransform();
	CachedWorldOffset = VoxelWorld->GetWorldOffset();
	CachedVoxelSize = VoxelWorld->VoxelSize;
	CachedMaskLimit = MaxVoxelScanCount;
	if (!ADRMeshVoxelCarver::BuildMeshVoxelMask(TargetMesh, VoxelWorld, MaxVoxelScanCount, TargetMask))
	{
		UE_LOG(LogTemp, Error, TEXT("Control zone %s: invalid mesh mask; scoring/cleanup disabled."),
			*GetName());
		return false;
	}
	return true;
}

bool ADRSnowControlZone::StartEndCleanup(TFunction<void(bool)>&& Completion)
{
	if (!bCleanupOnGameEnd || !TargetMesh || !TargetMesh->GetStaticMesh())
	{
		Completion(true);
		return true;
	}
	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || !CleanupBounds)
	{
		return false;
	}
	if (CleanupCancellation.IsValid() && !*CleanupCancellation)
	{
		return false;
	}
	TArray<TPair<UStaticMeshComponent*, int32>> Meshes;
	Meshes.Emplace(TargetMesh.Get(), MaxVoxelScanCount);
	// 정리 Box가 겹쳐도 다른 거점의 목표 모양은 함께 보존한다.
	for (TActorIterator<ADRSnowControlZone> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->ResolveVoxelWorld() == ResolveVoxelWorld()
			&& It->GetZoneWorldBounds().Intersect(CleanupBounds->Bounds.GetBox()))
		{
			Meshes.Emplace(It->TargetMesh.Get(), It->MaxVoxelScanCount);
		}
	}
	CleanupCancellation = MakeShared<FThreadSafeBool, ESPMode::ThreadSafe>(false);
	const auto Cancellation = CleanupCancellation.ToSharedRef();
	const TWeakObjectPtr<ADRSnowControlZone> WeakThis(this);
	const TWeakObjectPtr<AVoxelWorld> WeakWorld(VoxelWorld);
	const FTransform BoxTransform = CleanupBounds->GetComponentTransform();
	const FVector BoxExtent = CleanupBounds->GetUnscaledBoxExtent();
	// 종료 정리는 점유율 계산용 동기 캐시를 만들지 않고 별도의 비동기 마스크를 사용한다.
	const bool bStarted = ADRMeshVoxelCarver::BuildMeshVoxelMasksAsync(
		VoxelWorld, Meshes, Cancellation,
		[WeakThis, WeakWorld, BoxTransform, BoxExtent, Cancellation,
		Completion = MoveTemp(Completion)](TArray<FDRMeshVoxelMask>&& Masks) mutable
		{
			ADRSnowControlZone* Zone = WeakThis.Get();
			if (!Zone || !WeakWorld.IsValid() || *Cancellation || Masks.IsEmpty())
			{
				*Cancellation = true;
				Completion(false);
				return;
			}
			FDRMeshVoxelMask KeepMask = Masks[0];
			for (int32 Index = 1; Index < Masks.Num(); ++Index)
			{
				KeepMask.InsideVoxels.Append(Masks[Index].InsideVoxels);
			}
			// 시작 실패 시에도 완료 콜백을 한 번 전달할 수 있도록 보관한다.
			const auto Finish = MakeShared<TFunction<void(bool)>>(MoveTemp(Completion));
			if (!ADRMeshVoxelCarver::TrimOutsideMesh(WeakWorld.Get(), BoxTransform, BoxExtent,
				KeepMask, Zone->MaxCleanupVoxelCount, Masks[0], Cancellation,
				[Finish](bool bSucceeded)
				{
					(*Finish)(bSucceeded);
				}))
			{
				*Cancellation = true;
				(*Finish)(false);
			}
		});
	if (!bStarted)
	{
		*CleanupCancellation = true;
	}
	return bStarted;
}

void ADRSnowControlZone::OnRep_ControlState()
{
	if (bZoneCompleted)
	{
		GetWorldTimerManager().ClearTimer(ActivationRevealTimerHandle);
		bActivationRevealActive = false;
	}
	RefreshControlVisuals();
	RefreshPointLocationWidget();
}

void ADRSnowControlZone::RefreshControlVisuals()
{
	if (TargetMesh)
	{
		const bool bShowActiveMesh = bZoneActive && !bZoneCompleted;
		const bool bShowTargetMesh = !bZoneCompleted
			&& (bShowActiveMesh || bActivationRevealActive);
		TargetMesh->SetRenderCustomDepth(bShowTargetMesh);
		TargetMesh->SetCustomDepthStencilValue(OutlineStencilValue);
		// 활성화 전 색적 중에는 외곽선만 렌더링한다.
		TargetMesh->SetRenderInMainPass(bShowActiveMesh);
		TargetMesh->SetRenderInDepthPass(bShowActiveMesh);
		TargetMesh->SetVisibility(bShowTargetMesh);
		TargetMesh->SetHiddenInGame(!bShowTargetMesh);
		TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (PointLocationWidgetComponent)
	{
		PointLocationWidgetComponent->SetVisibility(bZoneActive);
	}
}

int32 ADRSnowControlZone::GetLeadingTeamId() const
{
	float TeamAmounts[2] = {0.f, 0.f};
	for (const FDRSnowTeamAmount& Team : CachedControlRatio.Teams)
	{
		if (Team.TeamId == 0 || Team.TeamId == 1)
		{
			TeamAmounts[Team.TeamId] += Team.Amount;
		}
	}

	if (FMath::IsNearlyEqual(TeamAmounts[0], TeamAmounts[1]))
	{
		return INDEX_NONE;
	}

	return TeamAmounts[0] > TeamAmounts[1] ? 0 : 1;
}

void ADRSnowControlZone::RefreshPointLocationWidget()
{
	if (!IsValid(PointLocationWidgetComponent))
	{
		return;
	}

	UDRPointLocationWidget* PointLocationWidget = Cast<UDRPointLocationWidget>(
		PointLocationWidgetComponent->GetUserWidgetObject());
	if (!IsValid(PointLocationWidget))
	{
		return;
	}

	PointLocationWidget->SetDisplayName(DisplayName);

	const int32 LeadingTeamId = GetLeadingTeamId();
	if (LeadingTeamId == INDEX_NONE)
	{
		PointLocationWidget->SetIndicatorColor(NeutralColor);
		return;
	}

	PointLocationWidget->SetIndicatorColor(LeadingTeamId == 0 ? Team0Color : Team1Color);
}

#pragma region Debug

void ADRSnowControlZone::InitializeDebug()
{
	if (bCreateDebugWidget && DebugWidgetClass)
	{
		APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (IsValid(PlayerController))
		{
			DebugWidget = CreateWidget<UUserWidget>(PlayerController, DebugWidgetClass);
			if (IsValid(DebugWidget))
			{
				DebugWidget->AddToViewport();
				DebugTextBlock = Cast<UTextBlock>(DebugWidget->GetWidgetFromName(DebugTextBlockName));
			}
		}
	}

	UpdateDebugWidget();
	// if (IsValid(DebugTextBlock))
	// {
	// 	GetWorldTimerManager().SetTimer(
	// 		DebugUpdateTimerHandle,
	// 		this,
	// 		&ADRSnowControlZone::UpdateDebugWidget,
	// 		FMath::Max(0.01f, DebugUpdateInterval),
	// 		true);
	// }
}

void ADRSnowControlZone::DeinitializeDebug()
{
	// GetWorldTimerManager().ClearTimer(DebugUpdateTimerHandle);
	if (IsValid(DebugWidget))
	{
		DebugWidget->RemoveFromParent();
	}
	DebugWidget = nullptr;
	DebugTextBlock = nullptr;
}

FDRSnowVoxelMaterialScanResult ADRSnowControlZone::ScanVoxelMaterials() const
{
	FDRSnowVoxelMaterialScanResult Result;
	// 실제 메쉬 내부 마스크만 검사한다.
	if (!EnsureTargetMask())
	{
		Result.bTruncated = true;
		return Result;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	if (!IsValid(VoxelWorld) ||
		!VoxelWorld->IsCreated())
	{
		return Result;
	}

	const FVoxelIntBox VoxelBounds = TargetMask.Bounds;
	if (!VoxelBounds.IsValid())
	{
		return Result;
	}

	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelReadScopeLock Lock(Data, VoxelBounds, FUNCTION_FNAME);
		for (int32 Z = VoxelBounds.Min.Z; Z < VoxelBounds.Max.Z; ++Z)
		{
			for (int32 Y = VoxelBounds.Min.Y; Y < VoxelBounds.Max.Y; ++Y)
			{
				for (int32 X = VoxelBounds.Min.X; X < VoxelBounds.Max.X; ++X)
				{
					const FIntVector VoxelPosition(X, Y, Z);
					if (!TargetMask.InsideVoxels.Contains(VoxelPosition))
					{
						continue;
					}

					++Result.ScannedVoxelCount;

					const FVoxelValue Value = Data.GetValue(VoxelPosition, 0);
					if (Value.IsEmpty())
					{
						continue;
					}

					++Result.FilledVoxelCount;

					const FVoxelMaterial Material = Data.GetMaterial(VoxelPosition, 0);
					const int32 MaterialIndex = GetDominantMaterialIndex(Material, VoxelWorld->MaterialConfig);
					if (MaterialIndex == INDEX_NONE)
					{
						++Result.UnknownCount;
						continue;
					}

					if (MaterialIndex == 0)
					{
						++Result.NeutralCount;
						continue;
					}

					++Result.MaterialVoxelCount;
					const int32 TeamId = DRSnowMaterialMapping::MaterialIndexToTeamId(
						static_cast<uint8>(MaterialIndex));
					FDRSnowVoxelMaterialTeamCount* Team = Result.Teams.FindByPredicate(
						[TeamId](const FDRSnowVoxelMaterialTeamCount& Entry)
						{
							return Entry.TeamId == TeamId;
						});
					if (!Team)
					{
						Team = &Result.Teams.AddDefaulted_GetRef();
						Team->TeamId = TeamId;
					}
					++Team->VoxelCount;
				}

				if (Result.bTruncated)
				{
					break;
				}
			}

			if (Result.bTruncated)
			{
				break;
			}
		}
	}

	if (Result.MaterialVoxelCount > 0)
	{
		for (FDRSnowVoxelMaterialTeamCount& Team : Result.Teams)
		{
			Team.Ratio = static_cast<float>(Team.VoxelCount) / static_cast<float>(Result.MaterialVoxelCount);
		}
	}

	if (Result.FilledVoxelCount > 0)
	{
		Result.Coverage = static_cast<float>(Result.MaterialVoxelCount) / static_cast<float>(Result.FilledVoxelCount);
	}

	return Result;
}

FString ADRSnowControlZone::BuildSnowCountDebugText() const
{
	return BuildSnowCountDebugTextFromScan(ScanVoxelMaterials());
}

FString ADRSnowControlZone::BuildSnowCountDebugTextFromScan(
	const FDRSnowVoxelMaterialScanResult& MaterialScan) const
{
	FString TeamText;
	for (const FDRSnowVoxelMaterialTeamCount& Team : MaterialScan.Teams)
	{
		TeamText += FString::Printf(TEXT("Team %d: %d (%.1f%%)\n"), Team.TeamId, Team.VoxelCount, Team.Ratio * 100.f);
	}
	return FString::Printf(
		TEXT("[Snow Zone Debug]%s\n\n")
		TEXT("Voxel Material Scan\n")
		TEXT("%sNeutral: %d  Unknown: %d\n")
		TEXT("Filled Voxels: %d\n")
		TEXT("Material Voxels: %d\n")
		TEXT("Coverage: %.1f%%\n")
		TEXT("Scanned: %d\n\n"),
		MaterialScan.bTruncated ? TEXT(" [Truncated]") : TEXT(""),
		*TeamText,
		MaterialScan.NeutralCount,
		MaterialScan.UnknownCount,
		MaterialScan.FilledVoxelCount,
		MaterialScan.MaterialVoxelCount,
		MaterialScan.Coverage * 100.f,
		MaterialScan.ScannedVoxelCount);
}

AVoxelWorld* ADRSnowControlZone::ResolveVoxelWorld() const
{
	if (IsValid(TargetVoxelWorld.Get()))
	{
		return TargetVoxelWorld.Get();
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}

void ADRSnowControlZone::UpdateDebugWidget()
{
	if (!IsValid(DebugTextBlock) && IsValid(DebugWidget))
	{
		DebugTextBlock = Cast<UTextBlock>(DebugWidget->GetWidgetFromName(DebugTextBlockName));
	}

	if (!IsValid(DebugTextBlock))
	{
		return;
	}

	DebugTextBlock->SetText(FText::FromString(BuildSnowCountDebugText()));
}

#pragma endregion
