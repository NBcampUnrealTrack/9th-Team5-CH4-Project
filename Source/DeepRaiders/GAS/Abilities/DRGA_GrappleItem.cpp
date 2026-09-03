
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
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_MovementAction);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Item_Grapple);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Input_SecondaryCancel);

	SetAssetTags(InitialAbilityTags);

	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_MovementAction);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);

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

	StartCancelEventTask();
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
	CancelEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,
		DRGameplayTags::Event_MovementAction_Cancel, nullptr, true, true);
	
	if (!IsValid(CancelEventTask))
	{
		return;
	}

	CancelEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleCancelEventReceived);
	CancelEventTask->ReadyForActivation();
}

void UDRGA_GrappleItem::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive()
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
			if (!StartAuthoritativeMovement(ServerTargetLocation, ServerTargetNormal))
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
		if (!StartPredictedMovement(ClientHit->ImpactPoint, ClientHit->ImpactNormal))
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
	QueueEndGrapple(EDRMovementActionEndReason::Cancelled);
}

bool UDRGA_GrappleItem::StartPredictedMovement(const FVector& InHookLocation, const FVector& InHookNormal)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();
	
	if (ActorInfo == nullptr
		|| ActorInfo->IsNetAuthority()
		|| !ActorInfo->IsLocallyControlled()
		|| !IsValid(World)
		|| InHookLocation.ContainsNaN())
	{
		return false;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());

	if (!IsValid(Character))
	{
		return false;
	}

	UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();

	UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

	if (!IsValid(MovementAction)
		|| !IsValid(Movement))
	{
		return false;
	}

	HookLocation = InHookLocation;

	const FDRMovementActionState State = BuildMovementActionState(HookLocation);

	if (!MovementAction->StartPredictedMovementAction(State))
	{
		return false;
	}

	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);

	MovementAction->OnMovementActionEnded.AddUObject(this, &ThisClass::HandleMovementActionEnded);
	MovementAction->OnMovementActionSimulated.AddUObject(this, &ThisClass::HandleMovementActionSimulated);

	MovementStartTimeSeconds = World->GetTimeSeconds();
	bMovementStarted = true;
	ApplyMovementActionTag();
	StartGrappleGameplayCue(HookLocation, InHookNormal);

	return true;
}

bool UDRGA_GrappleItem::StartAuthoritativeMovement(const FVector& InHookLocation, const FVector& InHookNormal)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();
	
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(World)
		|| InHookLocation.ContainsNaN())
	{
		return false;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());

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
		|| !ResolveSelectedItem(ActorInfo, ActiveItemDefinition, Inventory, InstanceId)
		|| InstanceId != ActiveInstanceId)
	{
		return false;
	}

	const FDRMovementActionState State = BuildMovementActionState(InHookLocation);

	if (!MovementAction->StartAuthoritativeMovementAction(State))
	{
		return false;
	}

	if (!Inventory->TryRemoveItemInstance(ActiveInstanceId, 1))
	{
		MovementAction->EndMovementAction(EDRMovementActionEndReason::Invalidated);

		return false;
	}

	HookLocation = InHookLocation;

	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);

	MovementAction->OnMovementActionEnded.AddUObject(this, &ThisClass::HandleMovementActionEnded);
	MovementAction->OnMovementActionSimulated.AddUObject(this, &ThisClass::HandleMovementActionSimulated);

	MovementStartTimeSeconds = World->GetTimeSeconds();
	bMovementStarted = true;
	ApplyMovementActionTag();
	StartGrappleGameplayCue(HookLocation, InHookNormal);

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
	if (!bMovementStarted
		|| !IsValid(ActiveItemDefinition))
	{
		return;
	}

	const float ElapsedTime = GetMovementElapsedTime();
	
	if (GrappleSettings.MaxDuration > 0.f && ElapsedTime >= GrappleSettings.MaxDuration)
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
	
	// 설정 시간만큼은 반드시 보장된다.
	if (ElapsedTime < GrappleSettings.ViewDetachProtectionDuration)
	{
		return;
	}
	
	const FVector ToHookDirection = ToHook.GetSafeNormal();
	const FVector ViewDirection = ResolveViewDirection();
	
	if (ToHookDirection.IsNearlyZero()
		|| ViewDirection.IsNearlyZero())
	{
		return;
	}
	
	// 훅이 시선의 90도 뒤쪽 반구로 넘어가면 플레이어가 훅을 지나친 것으로 판단한다.
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

		if (IsValid(MovementAction)
			&& MovementAction->IsMovementActionActive())
		{
			MovementAction->EndMovementAction(EndReason);
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

FVector UDRGA_GrappleItem::ResolveViewDirection() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ADRPlayerCharacter* Character = ActorInfo != nullptr ? Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}
	
	const FVector ViewDirection = Character->GetBaseAimRotation().Vector().GetSafeNormal();
	
	return !ViewDirection.IsNearlyZero() ? ViewDirection : Character->GetActorForwardVector().GetSafeNormal();
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
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = ActiveItemDefinition;
	
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

void UDRGA_GrappleItem::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndGrappleTimerHandle);
	}

	EndGrappleTimerHandle.Invalidate();
	
	const EDRMovementActionEndReason EndReason = bEndQueued ?
		PendingEndReason : bWasCancelled ? EDRMovementActionEndReason::Cancelled :	EDRMovementActionEndReason::Completed;
	
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
	HookLocation = FVector::ZeroVector;
	MovementStartTimeSeconds = -1.f;

	Super::EndAbility(Handle,ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
