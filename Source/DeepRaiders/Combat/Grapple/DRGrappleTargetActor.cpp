#include "DRGrappleTargetActor.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"

ADRGrappleTargetActor::ADRGrappleTargetActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetReplicates(false);

	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;
}

void ADRGrappleTargetActor::Configure(
	const FDRGrappleAbilitySettings& InSettings)
{
	Settings = InSettings;
}

void ADRGrappleTargetActor::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);

	const FGameplayAbilityActorInfo* ActorInfo = IsValid(Ability) ? Ability->GetCurrentActorInfo() : nullptr;

	SourceActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	SetActorTickEnabled(true);
	UpdateTargeting();
}

void ADRGrappleTargetActor::Tick(
	float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateTargeting();
}

bool ADRGrappleTargetActor::UpdateTargeting()
{
	bHasAimData = false;

	if (!IsValid(PrimaryPC)
		|| !IsValid(SourceActor))
	{
		DestroyAimMarker();
		return false;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		DestroyAimMarker();
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PrimaryPC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Settings.MaxDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRGrappleAim), false);
	QueryParams.AddIgnoredActor(SourceActor);

	FHitResult AimHit;

	const bool bBlockingHit = World->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd, 
		Settings.AimTraceChannel, QueryParams);

	if (bBlockingHit)
	{
		// 다른 액터와의 충돌도 실패 연출 위치로 전달
		CachedAimHit = AimHit;
	}
	else
	{
		// 허공을 조준한 경우에도 최대 사거리 지점을 포함한 TargetData 생성
		CachedAimHit = FHitResult();
		CachedAimHit.TraceStart = ViewLocation;
		CachedAimHit.TraceEnd = TraceEnd;
		CachedAimHit.Location = TraceEnd;
		CachedAimHit.ImpactPoint = TraceEnd;
		CachedAimHit.Distance = Settings.MaxDistance;
	}
	
	bHasAimData = true;

	if (bBlockingHit && IsValidGrappleSurface(AimHit))
	{
		UpdateAimMarker(AimHit);
	}
	else
	{
		DestroyAimMarker();
	}
	
	return true;
}

void ADRGrappleTargetActor::ConfirmTargetingAndContinue()
{
	if (!ShouldProduceTargetData())
	{
		return;
	}
	
	// 클릭 시점의 조준 정보를 다시 계산한다.
	if (!UpdateTargeting() || !bHasAimData)
	{
		CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
		return;
	}

	FGameplayAbilityTargetData_SingleTargetHit* TargetData = new FGameplayAbilityTargetData_SingleTargetHit(CachedAimHit);

	TargetDataReadyDelegate.Broadcast(FGameplayAbilityTargetDataHandle(TargetData));
}

bool ADRGrappleTargetActor::IsValidGrappleSurface(const FHitResult& Hit)
{
	if (!Hit.IsValidBlockingHit())
	{
		return false;
	}

	const UPrimitiveComponent* HitComponent = Hit.GetComponent();

	if (!IsValid(HitComponent))
	{
		return false;
	}

	if (HitComponent->IsA<UVoxelProceduralMeshComponent>())
	{
		return true;
	}

	return HitComponent->GetCollisionObjectType() == ECC_WorldStatic;
}

void ADRGrappleTargetActor::UpdateAimMarker(const FHitResult& Hit)
{
	if (!IsValid(Settings.AimMarkerSystem))
	{
		return;
	}

	if (!IsValid(AimMarkerComponent))
	{
		AimMarkerComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,
				Settings.AimMarkerSystem,Hit.ImpactPoint,	Hit.ImpactNormal.Rotation(),
				FVector::OneVector,false);
	}

	if (!IsValid(AimMarkerComponent))
	{
		return;
	}
	
	AimMarkerComponent->SetWorldLocation(Hit.ImpactPoint);
	AimMarkerComponent->SetWorldRotation(Hit.ImpactNormal.Rotation());
	AimMarkerComponent->SetVisibility(true);
}

void ADRGrappleTargetActor::DestroyAimMarker()
{
	if (!IsValid(AimMarkerComponent))
	{
		return;
	}

	AimMarkerComponent->DeactivateImmediate();
	AimMarkerComponent->DestroyComponent();
	AimMarkerComponent = nullptr;
}

void ADRGrappleTargetActor::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	DestroyAimMarker();

	Super::EndPlay(EndPlayReason);
}
