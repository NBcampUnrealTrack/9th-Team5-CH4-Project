#include "DRPlacementTargetActor.h"

#include "Abilities/GameplayAbility.h"
#include "DeepRaiders/Combat/Placement/DRPlacementPreviewActor.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 설치형 스킬은 경사면까지는 허용하지만, 벽/천장은 설정값과 무관하게 대상이 될 수 없다.
	constexpr float AbsoluteMaximumFloorSlopeDegrees = 45.f;
}

ADRPlacementTargetActor::ADRPlacementTargetActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicates(false);
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;
}

void ADRPlacementTargetActor::Configure(const FDRPlacementSettings& InSettings,
	TSubclassOf<ADRPlacementPreviewActor> InPreviewActorClass, const FVector& InPreviewDimensions)
{
	Settings = InSettings;
	PreviewActorClass = InPreviewActorClass;
	PreviewDimensions = FVector(
		FMath::Max(1.f, InPreviewDimensions.X),
		FMath::Max(1.f, InPreviewDimensions.Y),
		FMath::Max(1.f, InPreviewDimensions.Z));
}

void ADRPlacementTargetActor::AddRotationInput(const float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	RotationOffsetDegrees = FMath::UnwindDegrees(
		RotationOffsetDegrees + FMath::Sign(Value) * Settings.RotationStepDegrees);
	UpdateTargeting();
}

void ADRPlacementTargetActor::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	IsConfirmationSubmitted = false;
	RotationOffsetDegrees = 0.f;
	const FGameplayAbilityActorInfo* ActorInfo = IsValid(Ability) ? Ability->GetCurrentActorInfo() : nullptr;
	SourceActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (UWorld* World = GetWorld(); IsValid(PrimaryPC) && PrimaryPC->IsLocalController() && PreviewActorClass)
	{
		PreviewActor = World->SpawnActor<ADRPlacementPreviewActor>(PreviewActorClass);
	}
	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(PrimaryPC))
	{
		PlayerController->BeginPlacementInput(this);
	}
	SetActorTickEnabled(true);
	UpdateTargeting();
}

void ADRPlacementTargetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTargeting();
}

bool ADRPlacementTargetActor::UpdateTargeting()
{
	bHasValidAimData = false;
	HidePreview();

	if (!IsValid(PrimaryPC) || !IsValid(SourceActor) || !IsValid(GetWorld()))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PrimaryPC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRPlacementAim), false);
	QueryParams.AddIgnoredActor(SourceActor);

	FHitResult AimHit;
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Settings.MaxDistance;
	if (!GetWorld()->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd, Settings.AimTraceChannel, QueryParams))
	{
		return false;
	}

	if (!IsValidPlacementSurface(AimHit, Settings))
	{
		// 벽이나 천장을 조준한 경우에는 빨간 프리뷰조차 띄우지 않는다.
		return false;
	}

	CachedAimHit = AimHit;
	bHasValidAimData = true;

	const FVector PreviewAimDirection = FQuat(
		FVector::UpVector,
		FMath::DegreesToRadians(RotationOffsetDegrees)).RotateVector(ViewRotation.Vector());
	UpdatePreview(AimHit, PreviewAimDirection.Rotation(), true);
	return true;
}

bool ADRPlacementTargetActor::IsConfirmTargetingAllowed()
{
	return bHasValidAimData;
}

void ADRPlacementTargetActor::ConfirmTargetingAndContinue()
{
	if (IsConfirmationSubmitted || !ShouldProduceTargetData() || !IsConfirmTargetingAllowed())
	{
		return;
	}

	IsConfirmationSubmitted = true;
	SetActorTickEnabled(false);
	HidePreview();

	// VoxelProceduralMeshComponent는 NetGUID 대상이 아니다. 서버는 HitComponent를
	// 신뢰하지 않고 TraceStart/TraceEnd로 재검증하므로, 컴포넌트 참조는 전송하지 않는다.
	FHitResult TargetHit = CachedAimHit;
	TargetHit.Component = nullptr;
	FDRGameplayAbilityTargetData_Placement* TargetData =
		new FDRGameplayAbilityTargetData_Placement(TargetHit, RotationOffsetDegrees);
	TargetDataReadyDelegate.Broadcast(FGameplayAbilityTargetDataHandle(TargetData));
}

void ADRPlacementTargetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(PrimaryPC))
	{
		PlayerController->EndPlacementInput(this);
	}

	if (IsValid(PreviewActor))
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool ADRPlacementTargetActor::IsValidPlacementSurface(const FHitResult& Hit, const FDRPlacementSettings& InSettings)
{
	if (!Hit.IsValidBlockingHit())
	{
		return false;
	}

	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float MaximumAllowedSlope = FMath::Clamp(
		InSettings.MaxFloorSlopeDegrees,
		0.f,
		AbsoluteMaximumFloorSlopeDegrees);
	const float MinimumUpDot = FMath::Cos(FMath::DegreesToRadians(MaximumAllowedSlope));
	return FVector::DotProduct(Normal, FVector::UpVector) >= MinimumUpDot;
}

void ADRPlacementTargetActor::UpdatePreview(const FHitResult& Hit, const FRotator& ViewRotation, bool bCanPlace)
{
	if (IsValid(PreviewActor))
	{
		PreviewActor->UpdatePreview(Hit.ImpactPoint, Hit.ImpactNormal, ViewRotation.Vector(), bCanPlace, PreviewDimensions);
	}
}

void ADRPlacementTargetActor::HidePreview()
{
	if (IsValid(PreviewActor))
	{
		PreviewActor->HidePreview();
	}
}
