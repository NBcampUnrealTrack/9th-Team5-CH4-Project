
#include "DRGA_ThrowItem.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Combat/Projectile/DRThrowableProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Item/DRThrowableItemDefinition.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Combat/Throw/DRThrowTargetActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UDRGA_ThrowItem::UDRGA_ThrowItem()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	NetExecutionPolicy  = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	FGameplayTagContainer DefaultTags;
	DefaultTags.AddTag(DRGameplayTags::Ability_Attack);
	DefaultTags.AddTag(DRGameplayTags::Ability_Item_Throw);
	SetAssetTags(DefaultTags);
	
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Item_Throw);
	
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	
	TargetActorClass = ADRThrowTargetActor::StaticClass();	
}

bool UDRGA_ThrowItem::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const UDRThrowableItemDefinition* Definition = Cast<UDRThrowableItemDefinition>(GetSourceObject(Handle, ActorInfo));
	
	if (!IsValid(Definition)
		|| !Definition->ProjectileClass
		|| !TargetActorClass)
	{
		return false;
	}
	
	const int32 InputId = ResolveInputId(Handle, ActorInfo);
	
	if (InputId != static_cast<int32>(EDRAbilityInputId::Primary)
		&& InputId != static_cast<int32>(EDRAbilityInputId::Secondary))
	{
		return false;
	}
	
	UDRInventoryComponent* Inventory = nullptr;
	FGuid InstanceId;
	
	return ResolveSelectedThrowable(ActorInfo, Definition, Inventory, InstanceId);
}

void UDRGA_ThrowItem::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	ActiveDefinition = Cast<UDRThrowableItemDefinition>(GetSourceObject(Handle, ActorInfo));
	UDRInventoryComponent* Inventory = nullptr;
	
	if (!IsValid(ActiveDefinition)
		|| !ResolveSelectedThrowable(ActorInfo, ActiveDefinition, Inventory, ActiveInstanceId))
	{
		CancelThrow();
		return;
	}
	
	StartBlockingStateTasks();
	StartTargeting(ResolveInputId(Handle, ActorInfo));	
}

void UDRGA_ThrowItem::StartTargeting(int32 InputId)
{
	const bool bQuickThrow = InputId == static_cast<int32>(EDRAbilityInputId::Primary);
	const EGameplayTargetingConfirmation::Type ConfirmationType = bQuickThrow ?
		EGameplayTargetingConfirmation::Instant : EGameplayTargetingConfirmation::Custom;
	
	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(this, TEXT("ThrowTargetData"),
	 	ConfirmationType, TargetActorClass);
	
	if (!IsValid(TargetDataTask))
	{
		CancelThrow();
		return;
	}
	
	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);
	TargetDataTask->ReadyForActivation();
	
	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	
	if (TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		ADRThrowTargetActor* ThrowTargetActor = Cast<ADRThrowTargetActor>(SpawnedTargetActor);
		
		if (!IsValid(ThrowTargetActor))
		{
			SpawnedTargetActor->Destroy();
			CancelThrow();
			return;
		}
		
		ThrowTargetActor->Configure(ActiveDefinition->ThrowSettings, ActionSettings, !bQuickThrow);
		
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
	
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (!bQuickThrow
		&& ActorInfo != nullptr
		&& ActorInfo->IsLocallyControlled())
	{
		UAbilityTask_WaitInputRelease* ReleaseTask = 
			UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		
		if (!IsValid(ReleaseTask))
		{
			CancelThrow();
			return;
		}
		
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleAimReleased);
		
		ReleaseTask->ReadyForActivation();
	}
	
}

void UDRGA_ThrowItem::HandleAimReleased(float TimeHeld)
{
	if (IsActive()
		&& IsValid(TargetDataTask))
	{
		TargetDataTask->ExternalConfirm(true);
	}
}

void UDRGA_ThrowItem::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive()
		|| TargetData.Num() != 1)
	{
		CancelThrow();
		return;
	}
	
	ConfirmedTargetData = TargetData;
	ExecuteConfirmedThrow();
}

void UDRGA_ThrowItem::ExecuteConfirmedThrow()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ActorInfo== nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}
	
	FVector LaunchLocation;
	FVector LaunchDirection;
	if (!ValidateServerTargetData(LaunchLocation, LaunchDirection))
	{
		CancelThrow();
		return;
	}
	
	UDRInventoryComponent* Inventory = nullptr;
	FGuid InstanceId;
	
	if (!ResolveSelectedThrowable(ActorInfo, ActiveDefinition, Inventory, InstanceId)
		|| InstanceId != ActiveInstanceId)
	{
		CancelThrow();
		return;
	}
	
	if (!SpawnServerProjectile(LaunchLocation, LaunchDirection))
	{
		CancelThrow();
		return;
	}
	
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	
	const bool bConsumed = Inventory->TryRemoveItemInstance(InstanceId, 1);
	
	ensureMsgf(bConsumed, TEXT("[%s] Throwable spawned, but item %s was not consumed."),
		*GetName(), *InstanceId.ToString());
}

bool UDRGA_ThrowItem::ValidateServerTargetData(FVector& OutLaunchLocation, FVector& OutLaunchDirection) const
{
	OutLaunchLocation = FVector::ZeroVector;
	OutLaunchDirection = FVector::ZeroVector;
	
	if (!IsValid(ActiveDefinition)
		|| ConfirmedTargetData.Num() != 1)
	{
		return false;
	}
	
	const FHitResult* ClientAimHit = ConfirmedTargetData.Get(0)->GetHitResult();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ClientAimHit == nullptr
		|| ActorInfo == nullptr)
	{
		return false;
	}
	
	APlayerController* PlayerController = ActorInfo->PlayerController.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	
	if (!IsValid(PlayerController)
		|| !IsValid(AvatarActor)
		|| !IsValid(World))
	{
		return false;
	}
	
	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	
	PlayerController->GetPlayerViewPoint(ServerViewLocation,  ServerViewRotation);
	
	if (FVector::Dist(ServerViewLocation, ClientAimHit->TraceStart) > ActionSettings.ServerViewOriginTolerance)
	{
		return false;
	}
	
	const FVector ClientAimDirection = (ClientAimHit->TraceEnd - ClientAimHit->TraceStart).GetSafeNormal();
	if (ClientAimDirection.IsNearlyZero())
	{
		return false;
	}
	
	const float MinimumAimDot = FMath::Cos(FMath::DegreesToRadians(ActionSettings.ServerAimAngleTolerance));
	if (FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumAimDot)
	{
		return false;
	}
	
	const FVector ServerTraceEnd = ServerViewLocation + ClientAimDirection * ActiveDefinition->ThrowSettings.MaxAimDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRServerThrowAim), false);
	QueryParams.AddIgnoredActor(AvatarActor);
	
	FHitResult ServerAimHit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(ServerAimHit, ServerViewLocation, ServerTraceEnd,
		ActionSettings.AimTraceChannel, QueryParams);
	
	const FVector AimPoint = bBlockingHit ? ServerAimHit.ImpactPoint : ServerTraceEnd;
	
	OutLaunchLocation = DRThrow::ResolveLaunchLocation(AvatarActor, ActionSettings, ClientAimDirection);
	OutLaunchDirection = (AimPoint - OutLaunchLocation).GetSafeNormal();
	
	return !OutLaunchDirection.IsNearlyZero();
}

bool UDRGA_ThrowItem::SpawnServerProjectile(const FVector& LaunchLocation, const FVector& LaunchDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(ActiveDefinition)
		|| !ActiveDefinition->ProjectileClass)
	{
		return false;
	}

	UWorld* World = GetWorld();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (!IsValid(World)
		|| !IsValid(AvatarActor)
		|| !IsValid(ASC))
	{
		return false;
	}

	const FTransform SpawnTransform(LaunchDirection.Rotation(), LaunchLocation);

	ADRThrowableProjectile* Projectile = World->SpawnActorDeferred<ADRThrowableProjectile>(
			ActiveDefinition->ProjectileClass, SpawnTransform, AvatarActor, AvatarPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(Projectile))
	{
		return false;
	}

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		Projectile->Destroy();
		return false;
	}
	
	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);

	Projectile->InitializeThrowable(ASC, ImpactEffectSpecs, ActiveDefinition->ThrowSettings,
		ActionSettings, GetSourceTeamId(), ActiveDefinition);

	Projectile->FinishSpawning(SpawnTransform);
	
	return true;
}

void UDRGA_ThrowItem::BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();

	if (!IsValid(ActiveDefinition))
	{
		return;
	}

	for (const FDRGameplayEffectData& EffectData : ActiveDefinition->ThrowSettings.ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(GetCurrentAbilitySpecHandle(),
				GetCurrentActorInfo(), GetCurrentActivationInfo(), EffectData.EffectClass, EffectData.EffectLevel);

		if (!Spec.IsValid())
		{
			continue;
		}

		for (const TPair<FGameplayTag, float>& Magnitude : EffectData.SetByCallerMagnitudes)
		{
			if (Magnitude.Key.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
			}
		}

		OutEffectSpecs.Add(Spec);
	}	
}

bool UDRGA_ThrowItem::ResolveSelectedThrowable(const FGameplayAbilityActorInfo* ActorInfo,
	const UDRThrowableItemDefinition* ExpectedDefinition, UDRInventoryComponent*& OutInventory,
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

int32 UDRGA_ThrowItem::ResolveInputId(const FGameplayAbilitySpecHandle Handle,
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


int32 UDRGA_ThrowItem::GetSourceTeamId() const
{
	const FGameplayAbilityActorInfo* ActorInfo =
	GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return INDEX_NONE;
	}

	const int32 OwnerTeamId = DRCombatTeam::GetActorTeamId(ActorInfo->OwnerActor.Get());

	return OwnerTeamId != INDEX_NONE ? OwnerTeamId : DRCombatTeam::GetActorTeamId(ActorInfo->AvatarActor.Get());
}

void UDRGA_ThrowItem::StartBlockingStateTasks()
{
	UAbilityTask_WaitGameplayTagAdded* DeadTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this, DRGameplayTags::State_Dead, nullptr, true);
	
	DeadTask->Added.AddDynamic(this, &ThisClass::HandleBlockingStateAdded);
	DeadTask->ReadyForActivation();
	
	UAbilityTask_WaitGameplayTagAdded* FrozenTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this, DRGameplayTags::State_Frozen, nullptr, true);
	
	FrozenTask->Added.AddDynamic(this, &ThisClass::HandleBlockingStateAdded);
	FrozenTask->ReadyForActivation();
}

void UDRGA_ThrowItem::HandleBlockingStateAdded()
{
	CancelThrow();
}

void UDRGA_ThrowItem::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	CancelThrow();
}

void UDRGA_ThrowItem::CancelThrow()
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		true, true);
}

void UDRGA_ThrowItem::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}

	ConfirmedTargetData.Clear();
	ActiveDefinition = nullptr;
	ActiveInstanceId.Invalidate();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

