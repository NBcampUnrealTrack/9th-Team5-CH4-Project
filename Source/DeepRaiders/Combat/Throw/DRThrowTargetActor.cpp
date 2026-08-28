
#include "DRThrowTargetActor.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

ADRThrowTargetActor::ADRThrowTargetActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	SetReplicates(false);
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;
}

void ADRThrowTargetActor::Configure(const FDRThrowableItemSettings& InItemSettings,
	const FDRThrowActionSettings& InActionSettings, bool bInShowTrajectory)
{
	ItemSettings = InItemSettings;
	ActionSettings = InActionSettings;
	bShowTrajectory = bInShowTrajectory;	
}

void ADRThrowTargetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UpdateTargeting();
}

void ADRThrowTargetActor::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	
	const FGameplayAbilityActorInfo* Actorinfo = IsValid(Ability) ? Ability->GetCurrentActorInfo() : nullptr;
	
	SourceActor = Actorinfo != nullptr ? Actorinfo->AvatarActor.Get() : nullptr;
	
	SetActorTickEnabled(true);
	UpdateTargeting();
}

bool ADRThrowTargetActor::UpdateTargeting()
{
	if (!IsValid(PrimaryPC)
		|| !IsValid(SourceActor))
	{
		bHasValidAimData = false;
		return false;
	}
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		bHasValidAimData = false;
		return false;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;
	PrimaryPC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + ViewDirection * ItemSettings.MaxAimDistance;
	
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRThrowAim), false);
	QueryParams.AddIgnoredActor(SourceActor);
	
	FHitResult AimHit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd,
		ActionSettings.AimTraceChannel1, QueryParams);
	
	if (!bBlockingHit)
	{
		AimHit.TraceStart = ViewLocation;
		AimHit.TraceEnd = TraceEnd;
		AimHit.Location = TraceEnd;
		AimHit.ImpactPoint = TraceEnd;
	}
	
	CachedAimHit = AimHit;
	bHasValidAimData = true;
	
	// 이하부터 투척물 예상 경로
	if (!bShowTrajectory
		|| !IsValid(ActionSettings.TrajectoryPreviewSystem))
	{
		return true;
	}
	
	const FVector AimPoint = bBlockingHit ? AimHit.ImpactPoint : TraceEnd;
	const FVector LaunchLocation = DRThrow::ResolveLaunchLocation(SourceActor, ActionSettings, ViewDirection);
	
	FVector LaunchDirection = (AimPoint - LaunchLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = ViewDirection;
	}
	
	FPredictProjectilePathParams PredictParams(ActionSettings.PreviewProjectileRadius,
		LaunchLocation,	LaunchDirection * ItemSettings.InitialSpeed,
		ActionSettings.MaxSimulationTime, ActionSettings.AimTraceChannel1,SourceActor);
	
	PredictParams.SimFrequency = ActionSettings.SimulationFrequency;
	PredictParams.OverrideGravityZ = World->GetGravityZ() * ItemSettings.GravityScale;
	
	FPredictProjectilePathResult PredictResult;
	UGameplayStatics::PredictProjectilePath(this, PredictParams, PredictResult);
	
	TArray<FVector> PathPoints;
	PathPoints.Reserve(PredictResult.PathData.Num());
	
	for (const FPredictProjectilePathPointData& PointData : PredictResult.PathData)
	{
		PathPoints.Add(PointData.Location);
	}
	
	UpdateTrajectoryVFX(PathPoints);
	
	return true;
}

bool ADRThrowTargetActor::IsConfirmTargetingAllowed()
{
	return bHasValidAimData;
}

void ADRThrowTargetActor::ConfirmTargetingAndContinue()
{
	if (!ShouldProduceTargetData()
		|| !IsConfirmTargetingAllowed())
	{
		return;
	}
	
	FGameplayAbilityTargetData_SingleTargetHit* TargetData = new FGameplayAbilityTargetData_SingleTargetHit(CachedAimHit);
	
	TargetDataReadyDelegate.Broadcast(FGameplayAbilityTargetDataHandle(TargetData));	
}

void ADRThrowTargetActor::UpdateTrajectoryVFX(const TArray<FVector>& PathPoints)
{
	if (!IsValid(TrajectoryComponent))
	{
		TrajectoryComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, ActionSettings.TrajectoryPreviewSystem,
			FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, false);
	}
	
	if (!IsValid(TrajectoryComponent))
	{
		return;
	}
	
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(TrajectoryComponent,
		ActionSettings.TrajectoryPointsParameter, PathPoints);
}

void ADRThrowTargetActor::DestroyTrajectoryVFX()
{
	if (IsValid(TrajectoryComponent))
	{
		TrajectoryComponent->DeactivateImmediate();
		TrajectoryComponent->DestroyComponent();
		TrajectoryComponent = nullptr;
	}
}

void ADRThrowTargetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyTrajectoryVFX();
	
	Super::EndPlay(EndPlayReason);
}
