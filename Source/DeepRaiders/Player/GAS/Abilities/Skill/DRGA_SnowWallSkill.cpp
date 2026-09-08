#include "DRGA_SnowWallSkill.h"

#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTargetActor.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/SnowWall/DRSnowWall.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "Components/CapsuleComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "VoxelWorld.h"

UDRGA_SnowWallSkill::UDRGA_SnowWallSkill()
{
	TargetActorClass = ADRPlacementTargetActor::StaticClass();
	SnowWallClass = ADRSnowWall::StaticClass();
}

void UDRGA_SnowWallSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsValid(GetPlayerCharacter(ActorInfo)) || !TargetActorClass || !SnowWallClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartTargeting();
}

void UDRGA_SnowWallSkill::StartTargeting()
{
	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(this, TEXT("SnowWallTargetData"),
		EGameplayTargetingConfirmation::UserConfirmed, TargetActorClass);
	if (!IsValid(TargetDataTask))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCanceled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (!TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		// Grapple Targeting과 같이 서버에서는 TargetActor 생성이 불가능할 수 있다.
		// 이때 서버는 Task를 유지해 클라이언트가 복제한 TargetData를 계속 대기한다.
		return;
	}

	ADRPlacementTargetActor* PlacementTargetActor = Cast<ADRPlacementTargetActor>(SpawnedTargetActor);
	if (!IsValid(PlacementTargetActor))
	{
		if (IsValid(SpawnedTargetActor))
		{
			SpawnedTargetActor->Destroy();
		}
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	PlacementTargetActor->Configure(PlacementSettings, PreviewActorClass, WallDimensions);
	TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
}

void UDRGA_SnowWallSkill::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive() || TargetData.Num() != 1)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}
	if (ActorInfo->IsNetAuthority())
	{
		FTransform WallTransform;
		FHitResult SurfaceHit;
		if (!ValidateServerTargetData(TargetData, WallTransform, SurfaceHit))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		// 서버 검증을 통과했을 때만 쿨다운을 적용한다. 유효하지 않은 클라이언트 TargetData는 비용이 들지 않는다.
		if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		UWorld* World = GetWorld();
		LiftCharactersOntoWall(World, WallTransform, SurfaceHit);
		ADRSnowWall* SnowWall = IsValid(World)
			? World->SpawnActorDeferred<ADRSnowWall>(SnowWallClass, WallTransform, ActorInfo->AvatarActor.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()), ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn)
			: nullptr;
		if (!IsValid(SnowWall))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		SnowWall->ConfigureWall(WallDimensions);
		SnowWall->FinishSpawning(WallTransform);

		AVoxelWorld* VoxelWorld = ResolveVoxelWorld(SurfaceHit);
		UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
		if (!IsValid(VoxelWorld) || !IsValid(SnowSubsystem))
		{
			SnowWall->Destroy();
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		FDRSnowSurfaceAddRequest SnowRequest;
		SnowRequest.WorldLocation = WallTransform.GetLocation();
		SnowRequest.SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
		SnowRequest.ImpactDirection = WallTransform.GetUnitAxis(EAxis::Z);
		SnowRequest.TargetVoxelWorld = VoxelWorld;
		SnowRequest.Amount = 1.f;
		SnowRequest.EditTool = EDRSnowVoxelEditTool::OrientedBoxTool;
		SnowRequest.BoxExtent = WallDimensions * 0.5f;
		SnowRequest.BoxRotation = WallTransform.Rotator();
		SnowRequest.Context.SourceActor = ActorInfo->AvatarActor.Get();
		SnowRequest.Context.InstigatorPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());

		const FDRSnowAddResult SnowResult = SnowSubsystem->AddSnow(SnowRequest);
		if (SnowResult.AddedAmount <= 0.f)
		{
			SnowWall->Destroy();
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}
		if (ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			FDRSnowAddOperation SnowOperation;
			SnowOperation.WorldLocation = SnowRequest.WorldLocation;
			SnowOperation.SurfaceNormal = SnowRequest.SurfaceNormal;
			SnowOperation.ImpactDirection = SnowRequest.ImpactDirection;
			SnowOperation.Amount = SnowRequest.Amount;
			SnowOperation.BoxExtent = SnowRequest.BoxExtent;
			SnowOperation.BoxRotation = SnowRequest.BoxRotation;
			SnowOperation.EditTool = SnowRequest.EditTool;
			SnowOperation.TeamId = SnowRequest.Context.TeamId;
			SnowOperation.VoxelWorldName = VoxelWorld->GetFName();
			GameState->RegisterSnowAdd(SnowOperation);
		}

		// AddSnow가 복셀 값을 기록하고 갱신을 요청한 직후, 임시 충돌을 제거한다.
		SnowWall->Destroy();
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		}

		// TargetData RPC와 EndAbility RPC가 같은 ASC 채널에서 순서 경쟁을 일으킨다.
		// 여기서 종료하면 서버가 TargetData를 처리하기 전에 Ability가 닫힌다.
		// 서버 검증/설치가 끝낸 EndAbility를 클라이언트가 수신해 종료하도록 둔다.
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
}

void UDRGA_SnowWallSkill::HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}

bool UDRGA_SnowWallSkill::ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
	FTransform& OutWallTransform, FHitResult& OutSurfaceHit) const
{
	OutWallTransform = FTransform::Identity;
	OutSurfaceHit = FHitResult();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FDRGameplayAbilityTargetData_Placement* PlacementData = Data != nullptr
		&& Data->GetScriptStruct() == FDRGameplayAbilityTargetData_Placement::StaticStruct()
		? static_cast<const FDRGameplayAbilityTargetData_Placement*>(Data)
		: nullptr;
	const FHitResult* ClientHit = PlacementData != nullptr ? PlacementData->GetHitResult() : nullptr;
	APlayerController* PlayerController = ActorInfo != nullptr ? ActorInfo->PlayerController.Get() : nullptr;
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority() || ClientHit == nullptr || !IsValid(PlayerController)
		|| !IsValid(AvatarActor) || !IsValid(World))
	{
		return false;
	}
	if (!FMath::IsFinite(PlacementData->RotationOffsetDegrees)
		|| FMath::Abs(PlacementData->RotationOffsetDegrees) > 180.f)
	{
		return false;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	PlayerController->GetPlayerViewPoint(ServerViewLocation, ServerViewRotation);
	if (FVector::Dist(ServerViewLocation, ClientHit->TraceStart) > PlacementSettings.ServerViewOriginTolerance)
	{
		return false;
	}

	const FVector ClientAimDirection = (ClientHit->TraceEnd - ClientHit->TraceStart).GetSafeNormal();
	if (ClientAimDirection.IsNearlyZero())
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(PlacementSettings.ServerAimAngleTolerance, 0.f, 90.f)));
	if (FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumDot)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRSnowWallServerAim), false);
	QueryParams.AddIgnoredActor(AvatarActor);
	FHitResult ServerHit;
	const FVector TraceEnd = ServerViewLocation + ClientAimDirection * PlacementSettings.MaxDistance;
	if (!World->LineTraceSingleByChannel(ServerHit, ServerViewLocation, TraceEnd, PlacementSettings.AimTraceChannel, QueryParams)
		|| !ADRPlacementTargetActor::IsValidPlacementSurface(ServerHit, PlacementSettings))
	{
		return false;
	}

	// 설치 위치 검증에 사용한 클라이언트 조준선으로 회전도 계산해야
	// 고개를 거의 수직으로 숙였을 때 프리뷰와 실제 벽의 가로축이 달라지지 않는다.
	OutWallTransform = MakeWallTransform(
		ServerHit.ImpactPoint,
		ClientAimDirection,
		PlacementData->RotationOffsetDegrees);
	OutSurfaceHit = ServerHit;
	return true;
}

AVoxelWorld* UDRGA_SnowWallSkill::ResolveVoxelWorld(const FHitResult& SurfaceHit) const
{
	if (AVoxelWorld* HitVoxelWorld = Cast<AVoxelWorld>(SurfaceHit.GetActor()))
	{
		return HitVoxelWorld;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It) && (*It)->IsCreated())
		{
			return *It;
		}
	}

	return nullptr;
}

FTransform UDRGA_SnowWallSkill::MakeWallTransform(const FVector& ImpactPoint, const FVector& AimDirection,
	const float RotationOffsetDegrees) const
{
	FVector HorizontalAimDirection = AimDirection.GetSafeNormal2D();
	if (HorizontalAimDirection.IsNearlyZero())
	{
		HorizontalAimDirection = FVector::ForwardVector;
	}
	HorizontalAimDirection = FQuat(
		FVector::UpVector,
		FMath::DegreesToRadians(RotationOffsetDegrees)).RotateVector(HorizontalAimDirection);
	const FVector WallLengthDirection = FVector::CrossProduct(FVector::UpVector, HorizontalAimDirection).GetSafeNormal();
	const FQuat WallRotation = FRotationMatrix::MakeFromXZ(WallLengthDirection, FVector::UpVector).ToQuat();
	return FTransform(WallRotation, ImpactPoint + FVector::UpVector * (WallDimensions.Z * 0.5f));
}

void UDRGA_SnowWallSkill::LiftCharactersOntoWall(
	UWorld* World, const FTransform& WallTransform, const FHitResult& SurfaceHit) const
{
	if (!IsValid(World))
	{
		return;
	}

	const FVector WallExtent = WallDimensions * 0.5f;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		const UCapsuleComponent* Capsule =
			IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
		if (!IsValid(Capsule))
		{
			continue;
		}

		const FVector BoundsOrigin = Capsule->Bounds.Origin;
		const FVector BoundsExtent = Capsule->Bounds.BoxExtent;
		if (BoundsExtent.IsNearlyZero())
		{
			continue;
		}

		const FVector LocalBoundsOrigin = WallTransform.InverseTransformPosition(BoundsOrigin);
		const float HorizontalRadius = FMath::Max(BoundsExtent.X, BoundsExtent.Y);
		const bool bOverlapsWallFootprint =
			FMath::Abs(LocalBoundsOrigin.X) <= WallExtent.X + HorizontalRadius
			&& FMath::Abs(LocalBoundsOrigin.Y) <= WallExtent.Y + HorizontalRadius;
		const float CharacterBaseHeight = BoundsOrigin.Z - Capsule->GetScaledCapsuleHalfHeight();
		const bool bStandingOnPlacementSurface =
			FMath::Abs(CharacterBaseHeight - SurfaceHit.ImpactPoint.Z) <= 20.f;
		if (!bOverlapsWallFootprint || !bStandingOnPlacementSurface)
		{
			continue;
		}

		// Voxel 편집 직전에 Character를 눈벽 윗면으로 옮긴다. Sweep을 켜면
		// InvisibleWall을 포함해 Pawn을 막는 모든 월드 충돌이 상승 경로를 막는다.
		// 따라서 경계 벽에 막힌 Character는 벽을 관통하거나 월드 밖으로 나갈 수 없다.
		FHitResult LiftHit;
		Character->SetActorLocation(
			Character->GetActorLocation() + FVector::UpVector * WallDimensions.Z,
			true,
			&LiftHit,
			ETeleportType::TeleportPhysics);
	}
}

void UDRGA_SnowWallSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
