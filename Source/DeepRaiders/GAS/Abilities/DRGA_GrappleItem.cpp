
#include "DRGA_GrappleItem.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Combat/Grapple/DRGrappleTargetActor.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UDRGA_GrappleItem::UDRGA_GrappleItem()
{
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer InitialAbilityTags;
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Action);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_MovementAction);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Item_Grapple);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Input_SecondaryCancel);

	SetAssetTags(InitialAbilityTags);

	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_MovementAction);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Skill);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Attack);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Snow_Absorb);

	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Skill);
	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Snow_Absorb);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_GamePreparing);

	TargetActorClass = ADRGrappleTargetActor::StaticClass();
}

bool UDRGA_GrappleItem::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (ResolveInputId(Handle, ActorInfo) != static_cast<int32>(EDRAbilityInputId::Primary))
	{
		return false;
	}

	const UDRItemDefinition* Definition = Cast<UDRItemDefinition>(GetSourceObject(Handle, ActorInfo));

	if (!IsValid(Definition)
		|| !TargetActorClass)
	{
		return false;
	}

	UDRInventoryComponent* Inventory = nullptr;
	FGuid InstanceId;

	return ResolveSelectedItem(ActorInfo, Definition, Inventory, InstanceId);
}

void UDRGA_GrappleItem::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ActiveItemDefinition = Cast<UDRItemDefinition>(GetSourceObject(Handle, ActorInfo));

	UDRInventoryComponent* Inventory = nullptr;

	if (!IsValid(ActiveItemDefinition)
		|| !ResolveSelectedItem(ActorInfo, ActiveItemDefinition, Inventory, ActiveInstanceId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GrapplePhase = EDRGrapplePhase::Targeting;

	/*
	 * HookFlying에서는 우클릭 취소를 받지 않는다.
	 * StartCancelEventTask는 실제 Grappling 진입에 성공한 뒤 호출한다.
	 */
	StartTargeting();
}

void UDRGA_GrappleItem::StartTargeting()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ActorInfo == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}
	
	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(this,
		TEXT("GrappleTargetData"), EGameplayTargetingConfirmation::Instant, TargetActorClass);

	if (!IsValid(TargetDataTask))
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCanceled);

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	
	// 원격 클라이언트의 경우 BeginSpawningActor()에 항상 실패한다.
	// 실패에도 그냥 넘어가고 서버가 보내주는 TargetDelegate를 기다린다.
	if (TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		ADRGrappleTargetActor* GrappleTargetActor = Cast<ADRGrappleTargetActor>(SpawnedTargetActor);

		if (!IsValid(GrappleTargetActor))
		{
			SpawnedTargetActor->Destroy();

			QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
			return;
		}

		GrappleTargetActor->Configure(GrappleSettings);
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
}

void UDRGA_GrappleItem::StartCancelEventTask()
{
	if (IsValid(CancelEventTask))
	{
		return;
	}
	
	CancelEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,
		DRGameplayTags::Event_MovementAction_Cancel, nullptr, true, true);
	
	if (!IsValid(CancelEventTask))
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	CancelEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleCancelEventReceived);
	CancelEventTask->ReadyForActivation();
}

bool UDRGA_GrappleItem::BeginHookFlight(const FVector& InHookLocation, const FVector& InHookNormal)
{
	if (!IsActive()
		|| GrapplePhase != EDRGrapplePhase::Targeting
		|| InHookLocation.ContainsNaN()
		|| InHookNormal.ContainsNaN())
	{
		return false;
	}

	const FVector SafeHookNormal = InHookNormal.GetSafeNormal();

	if (SafeHookNormal.IsNearlyZero())
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	/*
	 * 훅 위치와 표면 노멀은 비행 시작 시 한 번만 확정한다.
	 * 이후 이동 시작, 평면 이탈 검사, GameplayCue가 모두 같은 값을 사용한다.
	 */
	HookLocation = InHookLocation;
	HookSurfaceNormal = SafeHookNormal;
	HookFlightDuration = CalculateHookFlightDuration(HookLocation);
	GrapplePhase = EDRGrapplePhase::HookFlying;

	// 훅 비행 중에도 아이템 변경과 다른 이동 액션은 막되, 실제 캐릭터 이동은 아직 시작하지 않는다.
	ApplyMovementActionTag();
	StartGrappleGameplayCue(HookLocation, HookSurfaceNormal);

	if (HookFlightDuration <= KINDA_SMALL_NUMBER)
	{
		HandleHookFlightFinished();
		return true;
	}

	World->GetTimerManager().SetTimer(HookFlightTimerHandle,this, &ThisClass::HandleHookFlightFinished,
		HookFlightDuration, false);

	return true;
}

float UDRGA_GrappleItem::CalculateHookFlightDuration(const FVector& InHookLocation) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	const float MinimumDuration = FMath::Max(GrappleSettings.MinimumHookFlightDuration, 0.f);

	if (!IsValid(AvatarActor) 
		|| InHookLocation.ContainsNaN())
	{
		return MinimumDuration;
	}

	const float TravelSpeed = FMath::Max(GrappleSettings.HookTravelSpeed, 0.f);

	if (TravelSpeed <= KINDA_SMALL_NUMBER)
	{
		return MinimumDuration;
	}

	const float Distance = FVector::Distance(AvatarActor->GetActorLocation(), InHookLocation);
	return FMath::Max(Distance / TravelSpeed, MinimumDuration);
}

void UDRGA_GrappleItem::HandleHookFlightFinished()
{
	HookFlightTimerHandle.Invalidate();

	if (!IsActive() || GrapplePhase != EDRGrapplePhase::HookFlying)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	bool bMovementStartedSuccessfully = false;

	if (ActorInfo->IsNetAuthority())
	{
		bMovementStartedSuccessfully = StartAuthoritativeMovement();
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		bMovementStartedSuccessfully = StartPredictedMovement();
	}

	if (!bMovementStartedSuccessfully)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
	}
}

void UDRGA_GrappleItem::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive()
		|| GrapplePhase != EDRGrapplePhase::Targeting
		|| TargetData.Num() != 1)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);

		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		FVector ServerTargetLocation;
		FVector ServerTargetNormal;
		
		const EDRGrappleTargetValidationResult ValidationResult = 
			ValidateServerTargetData(TargetData, ServerTargetLocation, ServerTargetNormal);
		
		if (ValidationResult == EDRGrappleTargetValidationResult::Succeeded)
		{
			if (!BeginHookFlight(ServerTargetLocation, ServerTargetNormal))
			{
				QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
			}
			
			return;
		}

		if (ValidationResult == EDRGrappleTargetValidationResult::Failed)
		{
			PlayFailedGrappleGameplayCue(ServerTargetLocation);
		}
		
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	if (!ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientHit = Data != nullptr ? Data->GetHitResult() : nullptr;

	if (ClientHit == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}
	
	if (ADRGrappleTargetActor::IsValidGrappleSurface(*ClientHit))
	{
		if (!BeginHookFlight(ClientHit->ImpactPoint, ClientHit->ImpactNormal))
		{
			QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		}
		
		return;
	}
	
	const FVector FailedLocation = ClientHit->IsValidBlockingHit() ? ClientHit->ImpactPoint : ClientHit->TraceEnd;
	
	PlayFailedGrappleGameplayCue(FailedLocation);
	QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
}

void UDRGA_GrappleItem::HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	QueueEndGrapple(EDRMovementActionEndReason::Cancelled);
}

void UDRGA_GrappleItem::HandleCancelEventReceived(FGameplayEventData Payload)
{
	// HookFlying에서 발생했던 취소 이벤트가 이동 시작 후까지 영향을 주지 않도록 실제 이동 단계만 허용한다.
	if (GrapplePhase != EDRGrapplePhase::Grappling 
		|| !bMovementStarted)
	{
		return;
	}
	
	QueueEndGrapple(EDRMovementActionEndReason::Cancelled);
}
bool UDRGA_GrappleItem::StartPredictedMovement()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();

	if (ActorInfo == nullptr
		|| ActorInfo->IsNetAuthority()
		|| !ActorInfo->IsLocallyControlled()
		|| !IsValid(World)		
		|| GrapplePhase != EDRGrapplePhase::HookFlying
		|| HookLocation.ContainsNaN()
		|| HookSurfaceNormal.IsNearlyZero())
	{
		return false;
	}

	const FVector SafeHookNormal = HookSurfaceNormal.GetSafeNormal();
	if (SafeHookNormal.IsNearlyZero())
	{
		return false;
	}
	
	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());

	if (!IsValid(Character))
	{
		return false;
	}

	UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();
	UDRCharacterMovementComponent* Movement =
		Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

	if (!IsValid(MovementAction) || !IsValid(Movement))
	{
		return false;
	}

	const FDRMovementActionState State = BuildMovementActionState(HookLocation);

	if (!MovementAction->StartPredictedMovementAction(State))
	{
		return false;
	}


	MovementAction->OnMovementActionEnded.RemoveAll(this);
	MovementAction->OnMovementActionSimulated.RemoveAll(this);
	MovementAction->OnMovementActionEnded.AddUObject(this, &ThisClass::HandleMovementActionEnded);
	MovementAction->OnMovementActionSimulated.AddUObject(this, &ThisClass::HandleMovementActionSimulated);

	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);
	
	MovementStartTimeSeconds = World->GetTimeSeconds();
	bMovementStarted = true;
	GrapplePhase = EDRGrapplePhase::Grappling;
	
	// 우클릭 취소는 훅이 도착하고 실제 이동이 시작된 이후에만 활성화
	StartCancelEventTask();

	return true;
}
bool UDRGA_GrappleItem::StartAuthoritativeMovement()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(World)
		|| GrapplePhase != EDRGrapplePhase::HookFlying
		|| HookLocation.ContainsNaN()
		|| HookSurfaceNormal.IsNearlyZero())
	{
		return false;
	}

	ADRPlayerCharacter* Character =	Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());

	if (!IsValid(Character))
	{
		return false;
	}

	UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();
	UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

	UDRInventoryComponent* Inventory = nullptr;
	FGuid InstanceId;

	if (!IsValid(MovementAction)
		|| !IsValid(Movement)
		|| !ResolveSelectedItem(ActorInfo, ActiveItemDefinition, Inventory,InstanceId)
		|| InstanceId != ActiveInstanceId)
	{
		return false;
	}

	const FDRMovementActionState State = BuildMovementActionState(HookLocation);

	if (!MovementAction->StartAuthoritativeMovementAction(State))
	{
		return false;
	}

	if (!Inventory->TryRemoveItemInstance(ActiveInstanceId, 1))
	{
		MovementAction->EndMovementAction(
			EDRMovementActionEndReason::Invalidated);

		return false;
	}

	
	MovementAction->OnMovementActionEnded.RemoveAll(this);
	MovementAction->OnMovementActionSimulated.RemoveAll(this);
	MovementAction->OnMovementActionEnded.AddUObject(this, &ThisClass::HandleMovementActionEnded);
	MovementAction->OnMovementActionSimulated.AddUObject(this, &ThisClass::HandleMovementActionSimulated);
	
	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);
	
	MovementStartTimeSeconds = World->GetTimeSeconds();
	bMovementStarted = true;
	GrapplePhase = EDRGrapplePhase::Grappling;
	
	StartCancelEventTask();
	
	return true;
}

FDRMovementActionState UDRGA_GrappleItem::BuildMovementActionState(const FVector& InHookLocation) const
{
	FDRMovementActionState State;
	State.bActive = true;
	State.ActionType = EDRMovementActionType::Grapple;
	State.SessionId = ResolveSessionId();
	State.ReferenceLocation = InHookLocation;
	State.ActionAcceleration = GrappleSettings.PullAcceleration;
	State.MaxSpeed = GrappleSettings.MaxSpeed;
	State.ControlScale = GrappleSettings.ControlScale;
	State.ViewDirectionWeight = GrappleSettings.ViewPullWeight;
	State.MinimumReferenceClosingSpeed = FMath::Max(GrappleSettings.MinimumClosingSpeed, 0.f);

	return State;
}

void UDRGA_GrappleItem::ApplyMovementActionTag()
{
	if (bMovementActionTagApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (!IsValid(ASC)
		|| ActorInfo == nullptr)
	{
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		ASC->AddLooseGameplayTag(DRGameplayTags::State_MovementAction_Active, 1, 
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		ASC->AddLooseGameplayTag(DRGameplayTags::State_MovementAction_Active);
	}

	bMovementActionTagApplied = true;
}

void UDRGA_GrappleItem::RemoveMovementActionTag()
{
	if (!bMovementActionTagApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (!IsValid(ASC)
		|| ActorInfo == nullptr)
	{
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		ASC->RemoveLooseGameplayTag(DRGameplayTags::State_MovementAction_Active, 1, 
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		ASC->RemoveLooseGameplayTag(DRGameplayTags::State_MovementAction_Active);
	}

	bMovementActionTagApplied = false;
}

UDRGA_GrappleItem::EDRGrappleTargetValidationResult UDRGA_GrappleItem::ValidateServerTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutTargetLocation, FVector& OutTargetNormal) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutTargetNormal = FVector::ZeroVector;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| TargetData.Num() != 1
		|| !IsValid(ActiveItemDefinition))
	{
		return EDRGrappleTargetValidationResult::InvalidRequest;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientHit = Data != nullptr ? Data->GetHitResult() : nullptr;
	APlayerController* PlayerController = ActorInfo->PlayerController.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();

	if (ClientHit == nullptr
		|| !IsValid(PlayerController)
		|| !IsValid(AvatarActor)
		|| !IsValid(World))
	{
		return EDRGrappleTargetValidationResult::InvalidRequest;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	PlayerController->GetPlayerViewPoint(ServerViewLocation, ServerViewRotation);

	if (FVector::Dist(ServerViewLocation, ClientHit->TraceStart) > GrappleSettings.ServerViewOriginTolerance)
	{
		return EDRGrappleTargetValidationResult::InvalidRequest;
	}

	const FVector ClientAimDirection = (ClientHit->TraceEnd - ClientHit->TraceStart).GetSafeNormal();

	if (ClientAimDirection.IsNearlyZero())
	{
		return EDRGrappleTargetValidationResult::InvalidRequest;
	}

	const float MaxAngle = FMath::Clamp(GrappleSettings.ServerAimAngleTolerance, 0.f, 90.f);
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(MaxAngle));

	if (FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumDot)
	{
		return EDRGrappleTargetValidationResult::InvalidRequest;
	}

	const FVector TraceEnd = ServerViewLocation + ClientAimDirection * GrappleSettings.MaxDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRGrappleServerAim), false);

	QueryParams.AddIgnoredActor(AvatarActor);

	FHitResult ServerHit;

	const bool bBlockingHit = World->LineTraceSingleByChannel(ServerHit,ServerViewLocation,
			TraceEnd, GrappleSettings.AimTraceChannel, QueryParams);

	if (!bBlockingHit
		|| !ADRGrappleTargetActor::IsValidGrappleSurface(ServerHit))
	{
		OutTargetLocation = bBlockingHit ? ServerHit.ImpactPoint : TraceEnd;
		OutTargetNormal = bBlockingHit ? ServerHit.ImpactNormal.GetSafeNormal() : FVector::ZeroVector;
		return EDRGrappleTargetValidationResult::Failed;
	}

	OutTargetLocation = ServerHit.ImpactPoint;
	OutTargetNormal = ServerHit.ImpactNormal.GetSafeNormal();

	return EDRGrappleTargetValidationResult::Succeeded;
}

int32 UDRGA_GrappleItem::ResolveSessionId() const
{
	const int32 PredictionKey =	static_cast<int32>(GetCurrentActivationInfo().GetActivationPredictionKey().Current);

	return FMath::Max(PredictionKey, 1);
}

FVector UDRGA_GrappleItem::ResolveViewDirection() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ADRPlayerCharacter* Character = ActorInfo != nullptr ?
		Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}
	
	const FVector ViewDirection = Character->GetBaseAimRotation().Vector().GetSafeNormal();
	
	return !ViewDirection.IsNearlyZero() ? ViewDirection : Character->GetActorForwardVector().GetSafeNormal();
}

int32 UDRGA_GrappleItem::ResolveInputId(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (ActorInfo == nullptr
		|| !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return INDEX_NONE;
	}

	const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);

	return Spec != nullptr ? Spec->InputID : INDEX_NONE;
}

bool UDRGA_GrappleItem::ResolveSelectedItem(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UDRItemDefinition* ExpectedDefinition,
	UDRInventoryComponent*& OutInventory,
	FGuid& OutInstanceId) const
{
	OutInventory = nullptr;
	OutInstanceId.Invalidate();

	if (ActorInfo == nullptr
		|| !IsValid(ExpectedDefinition))
	{
		return false;
	}

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());

	if (!IsValid(PlayerController))
	{
		return false;
	}

	UDRInventoryComponent* Inventory = PlayerController->GetInventoryComponent();

	UDRQuickSlotComponent* QuickSlot = PlayerController->GetQuickSlotComponent();

	if (!IsValid(Inventory)
		|| !IsValid(QuickSlot))
	{
		return false;
	}

	const FGuid InstanceId = QuickSlot->GetSelectedInstanceId();

	const FDRItemInstance* ItemInstance = Inventory->FindItemInstance(InstanceId);

	if (ItemInstance == nullptr
		|| ItemInstance->Definition.Get() != ExpectedDefinition
		|| ItemInstance->Quantity <= 0)
	{
		return false;
	}

	OutInventory = Inventory;
	OutInstanceId = InstanceId;

	return true;
}
void UDRGA_GrappleItem::HandleMovementActionSimulated(
	const FDRMovementActionSimulationResult& Result)
{
	if (GrapplePhase != EDRGrapplePhase::Grappling
		|| !bMovementStarted
		|| !IsValid(ActiveItemDefinition))
	{
		return;
	}

	const float ElapsedTime = GetMovementElapsedTime();

	if (GrappleSettings.MaxDuration > 0.f
		&& ElapsedTime >= GrappleSettings.MaxDuration)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
		return;
	}

	const FVector ToHook = HookLocation - Result.Location;
	const float ArrivalDistance = FMath::Max(GrappleSettings.ArrivalDistance, 0.f);

	if (ToHook.SizeSquared() <= FMath::Square(ArrivalDistance))
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
		return;
	}

	// 그래플 시작 직후에는 훅 평면을 넘어가더라도 설정 시간까지 연결을 보장한다.
	if (ElapsedTime < GrappleSettings.ViewDetachProtectionDuration)
	{
		return;
	}

	if (HookSurfaceNormal.IsNearlyZero())
	{
		/*
		 * 정상적인 시작 경로에서는 발생하지 않는다.
		 * 유효한 평면을 구성할 수 없다면 연결을 계속 유지하지 않는다.
		 */
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	const FVector HookToCharacterDirection = (Result.Location - HookLocation).GetSafeNormal();

	if (HookToCharacterDirection.IsNearlyZero())
	{
		return;
	}

	const float SurfaceSideDot = FVector::DotProduct(HookSurfaceNormal, HookToCharacterDirection);

	/*
	 * 양수: 캐릭터가 훅이 부착된 표면의 바깥쪽에 있다.
	 * 0 이하: 캐릭터가 훅 평면과 나란하거나 반대쪽으로 넘어갔다.
	 */
	if (SurfaceSideDot <= 0.f)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
		return;
	}
	
	const FVector ToHookDirection = ToHook.GetSafeNormal();
	const FVector ViewDirection = ResolveViewDirection();
	
	if (ToHookDirection.IsNearlyZero()
		|| ViewDirection.IsNearlyZero())
	{
		return;
	}
	
	if (FVector::DotProduct(ViewDirection, ToHookDirection) <= 0.f)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
	}
}

void UDRGA_GrappleItem::HandleMovementActionEnded(
	EDRMovementActionEndReason EndReason)
{
	if (bEndingGrapple)
	{
		return;
	}

	QueueEndGrapple(EndReason);
}

void UDRGA_GrappleItem::QueueEndGrapple(
	EDRMovementActionEndReason EndReason)
{
	if (bEndQueued
		|| bEndingGrapple)
	{
		return;
	}

	bEndQueued = true;
	PendingEndReason = EndReason;

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		ApplyQueuedGrappleEnd();
		return;
	}

	// 다음 프레임에 그래플 종료를 예약한다.
	// 캐릭터에게 부여된 어빌리티를 정상 회수한 후에 어빌리티가 종료될 수 있도록
	EndGrappleTimerHandle =
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ApplyQueuedGrappleEnd));
}

void UDRGA_GrappleItem::ApplyQueuedGrappleEnd()
{
	EndGrappleTimerHandle.Invalidate();
	

	if (!IsActive()
		|| bEndingGrapple)
	{
		bEndQueued = false;
		PendingEndReason = EDRMovementActionEndReason::Invalidated;
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true,
		PendingEndReason != EDRMovementActionEndReason::Completed);
}

void UDRGA_GrappleItem::StopMovementAction(
	EDRMovementActionEndReason EndReason)
{
	bEndingGrapple = true;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	ADRPlayerCharacter* Character = ActorInfo != nullptr ?
		Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (IsValid(Character))
	{
		UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();
		UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

		const bool bShouldPreserveMomentum = bMovementStarted 
			&& (EndReason == EDRMovementActionEndReason::Completed || EndReason == EDRMovementActionEndReason::Cancelled);
		
		/*
		 * Custom Movement에서 벗어나기 전에 현재 횡방향 속도를 저장한다.
		 * ExitCustomMovementMode 이후 저장하면 Falling의 일반 속도 제한이 먼저 적용될 수 있다.
		 */
		if (IsValid(Movement)
			&& bShouldPreserveMomentum)
		{
			Movement->BeginAirborneMomentumPreservation();
		}
		
		if (IsValid(MovementAction)
			&& MovementAction->IsMovementActionActive())
		{
			MovementAction->EndMovementAction(EndReason);
		}
		
		if (IsValid(MovementAction))
		{
			MovementAction->OnMovementActionEnded.RemoveAll(this);
			MovementAction->OnMovementActionSimulated.RemoveAll(this);
		}

		if (IsValid(Movement)
			&& Movement->IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
		{
			Movement->ExitCustomMovementMode();
		}
	}

	bMovementStarted = false;
	bEndingGrapple = false;
}

float UDRGA_GrappleItem::GetMovementElapsedTime() const
{
	const UWorld* World = GetWorld();
	
	if (!bMovementStarted
		|| !IsValid(World)
		|| MovementStartTimeSeconds < 0.f)
	{
		return 0.f;
	}
	
	return FMath::Max(World->GetTimeSeconds() - MovementStartTimeSeconds, 0.f);
}

void UDRGA_GrappleItem::StartGrappleGameplayCue(const FVector& InHookLocation, const FVector& InHookNormal)
{
	if (bGrappleGameplayCueActive)
	{
		return;
	}
	
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	
	if (!IsValid(ASC)
		|| !IsValid(AvatarActor)
		|| !IsValid(ActiveItemDefinition))
	{
		return;
	}
	
	FGameplayCueParameters Parameters;
	Parameters.Location = InHookLocation;
	Parameters.Normal = InHookNormal.ContainsNaN() ? FVector::ZeroVector : InHookNormal.GetSafeNormal();
	Parameters.RawMagnitude = HookFlightDuration;
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = ActiveItemDefinition;
	
	/*
 	* RawMagnitude로 GA가 계산한 훅 비행 시간을 전달한다.
 	* Cue와 실제 이동 시작 타이머가 같은 시간을 사용하므로 부착 연출과 이동 시작이 맞춰진다.
 	*/
	ASC->AddGameplayCue(DRGameplayTags::GameplayCue_MovementAction_Grapple_Active, Parameters);
	
	bGrappleGameplayCueActive = true;	
}

void UDRGA_GrappleItem::PlayFailedGrappleGameplayCue(const FVector& FailedLocation)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	if (!IsValid(ASC) || !IsValid(AvatarActor) || FailedLocation.ContainsNaN())
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = FailedLocation;
	Parameters.RawMagnitude = CalculateHookFlightDuration(FailedLocation);
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = ActiveItemDefinition;

	// 실패 연출은 어빌리티 수명과 무관하게 스스로 재생을 끝내는 일회성 Cue다.
	ASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_MovementAction_Grapple_Failed, Parameters);
}

void UDRGA_GrappleItem::StopGrappleGameplayCue()
{
	if (!bGrappleGameplayCueActive)
	{
		return;
	}
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveGameplayCue(DRGameplayTags::GameplayCue_MovementAction_Grapple_Active);
	}
	
	bGrappleGameplayCueActive = false;
}
void UDRGA_GrappleItem::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	GrapplePhase = EDRGrapplePhase::Ending;
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HookFlightTimerHandle);
		World->GetTimerManager().ClearTimer(EndGrappleTimerHandle);
	}

	HookFlightTimerHandle.Invalidate();
	EndGrappleTimerHandle.Invalidate();

	const EDRMovementActionEndReason EndReason = bEndQueued ? PendingEndReason : 
		bWasCancelled ? EDRMovementActionEndReason::Cancelled : EDRMovementActionEndReason::Completed;

	bEndQueued = false;
	PendingEndReason = EDRMovementActionEndReason::Invalidated;

	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}

	if (IsValid(CancelEventTask))
	{
		CancelEventTask->EndTask();
		CancelEventTask = nullptr;
	}

	StopMovementAction(EndReason);
	RemoveMovementActionTag();
	StopGrappleGameplayCue();

	ActiveItemDefinition = nullptr;
	ActiveInstanceId.Invalidate();

	// InstancedPerActor GA가 다음 활성화에서 이전 훅과 타이밍을 재사용하지 않도록 모두 초기화
	HookLocation = FVector::ZeroVector;
	HookSurfaceNormal = FVector::ZeroVector;
	HookFlightDuration = 0.f;
	MovementStartTimeSeconds = -1.f;
	GrapplePhase = EDRGrapplePhase::Inactive;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
