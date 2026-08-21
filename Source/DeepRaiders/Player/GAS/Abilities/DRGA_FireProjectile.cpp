// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGA_FireProjectile.h"

#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameplayEffect.h"

#include "DeepRaiders/DeepRaiders.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"

bool ResolveSelectedWeaponInstance(const FGameplayAbilityActorInfo* ActorInfo
	, const UDRProjectileWeaponItemDefinition* ExpectedDefinition
	,UDRInventoryComponent*& OutInventory, const FDRItemInstance*& OutItemInstance)
{
	OutInventory = nullptr;
	OutItemInstance = nullptr;
	
	if (ActorInfo == nullptr || !IsValid(ExpectedDefinition))
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
	
	if (!IsValid(Inventory) || !IsValid(QuickSlot))
	{
		return false;
	}
	
	// 현재 선택된 아이템의 Definition과 ExpectedDefinition이 동일한지 검사	
	const FGuid SelectedInstanceId = QuickSlot->GetSelectedInstanceId();
	const FDRItemInstance* SelectedItem = Inventory->FindItemInstance(SelectedInstanceId);
	
	if (!SelectedItem || SelectedItem->Definition.Get() != ExpectedDefinition)
	{
		return false;
	}
	
	OutInventory = Inventory;
	OutItemInstance = SelectedItem;
	return true;
}

UDRGA_FireProjectile::UDRGA_FireProjectile()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;	
}

void UDRGA_FireProjectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	UDRProjectileWeaponItemDefinition* WeaponDefinition = Cast<UDRProjectileWeaponItemDefinition>(GetSourceObject(Handle, ActorInfo));

	if (!IsValid(WeaponDefinition)
		|| WeaponDefinition->AttackType != EDRRangedWeaponAttackType::Projectile
		|| !WeaponDefinition->ProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());

	UAnimMontage* PrimaryActionMontage = nullptr;

	if (IsValid(WeaponDefinition->ItemAnimationSet))
	{
		PrimaryActionMontage = WeaponDefinition->ItemAnimationSet->PrimaryActionMontage;
	}

	if (IsValid(Character))
	{
		const float AimHoldDuration = WeaponDefinition->BaseFireInterval + 0.15f;

		// Remote owning client 예측 재생
		if (!ActorInfo->IsNetAuthority() && Character->IsLocallyControlled() && IsValid(PrimaryActionMontage))
		{
			Character->PlayWeaponFirePresentationLocal(PrimaryActionMontage);
		}
	}

	// 실제 게임 결과는 서버
	if (ActorInfo->IsNetAuthority())
	{
		if (IsValid(Character) && IsValid(PrimaryActionMontage))
		{
			Character->PlayWeaponFirePresentationFromServer(PrimaryActionMontage);
		}

		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

		TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;

		BuildImpactEffectSpecs(ASC, WeaponDefinition, ImpactEffectSpecs);

		SpawnProjectile(ActorInfo, WeaponDefinition, ImpactEffectSpecs);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDRGA_FireProjectile::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 부모 클래스의 ApplyCooldown 함수를 완전히 대체한다.
	//Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	
	const UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect();
	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = Cast<UDRProjectileWeaponItemDefinition>(
		GetSourceObject(Handle, ActorInfo));
	
	if (!IsValid(CooldownEffect)
		|| !IsValid(WeaponDefinition))
	{
		return;
	}
	
	FGameplayEffectSpecHandle CoolDownSpec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo
		, ActivationInfo, CooldownEffect->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	
	if (!CoolDownSpec.IsValid())
	{
		return;
	}
	
	CoolDownSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Cooldown.Duration"))
		, WeaponDefinition->BaseFireInterval);
	
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CoolDownSpec);
}

bool UDRGA_FireProjectile::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (ActorInfo == nullptr)
	{
		return false;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = Cast<UDRProjectileWeaponItemDefinition>(GetSourceObject(Handle, ActorInfo));

	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	// ProjectileWeapon의 ResourceType에 따른 Cost 처리
	switch (WeaponDefinition->ResourceType)
	{
	case EDRProjectileWeaponResourceType::SnowGauge:
	{
		if (WeaponDefinition->SnowCostPerShot <= 0.f)
		{
			return true;
		}

		if (!WeaponDefinition->SnowCostEffectClass)
		{
			return false;
		}

		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

		if (!IsValid(ASC))
		{
			return false;
		}

		const float CurrentSnow = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetSnowGaugeAttribute());

		return CurrentSnow + KINDA_SMALL_NUMBER >= WeaponDefinition->SnowCostPerShot;
	}
	case EDRProjectileWeaponResourceType::InstanceAmmo:
	{
		UDRInventoryComponent* Inventory = nullptr;
		const FDRItemInstance* ItemInstance = nullptr;
		if (!ResolveSelectedWeaponInstance(ActorInfo, WeaponDefinition, Inventory, ItemInstance))
		{
			return false;
		}

		const FDRProjectileWeaponRuntimeState* WeaponState = ItemInstance->RuntimeState.GetPtr<
			FDRProjectileWeaponRuntimeState>();

		return WeaponState && WeaponState->CurrentAmmo > 0;
	}

	default:
		DR_ERROR(TEXT("[%s] Invalid projectile Weapon resource type"), *GetName());
		return false;
	}
}

void UDRGA_FireProjectile::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	if (ActorInfo == nullptr)
	{
		return;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = Cast<UDRProjectileWeaponItemDefinition>(GetSourceObject(Handle, ActorInfo));

	if (!IsValid(WeaponDefinition))
	{
		return;
	}

	switch (WeaponDefinition->ResourceType)
	{
	case EDRProjectileWeaponResourceType::SnowGauge:
	{
		if (WeaponDefinition->SnowCostPerShot <= 0.f
			|| !WeaponDefinition->SnowCostEffectClass)
		{
			return;
		}

		FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo
		                                                                    , WeaponDefinition->SnowCostEffectClass,
		                                                                    GetAbilityLevel(Handle, ActorInfo));

		if (!CostSpec.IsValid())
		{
			return;
		}

		CostSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, -WeaponDefinition->SnowCostPerShot);

		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);
		
		return;
	}

	case EDRProjectileWeaponResourceType::InstanceAmmo:
	{
		if (!ActorInfo->IsNetAuthority())
		{
			return;
		}
		
		UDRInventoryComponent* Inventory = nullptr;
		const FDRItemInstance* ItemInstance = nullptr;
		
		if (!ResolveSelectedWeaponInstance(ActorInfo, WeaponDefinition, Inventory, ItemInstance))
		{
			return;
		}
		
		const FGuid SelectedInstanceId = ItemInstance->InstanceId;
		
		const bool bConsumed = Inventory->ModifyItemInstance(SelectedInstanceId, 
			[](FDRItemInstance& Candidate)
			{
				FDRProjectileWeaponRuntimeState* WeaponState = Candidate.RuntimeState.GetMutablePtr<FDRProjectileWeaponRuntimeState>();
				
				if (!WeaponState
					|| WeaponState->CurrentAmmo <= 0)
				{
					return false;
				}
				
				--WeaponState->CurrentAmmo;
				return true;
			});

		ensureMsgf(bConsumed, TEXT("Failed to consume ammo from item instance %s"), *SelectedInstanceId.ToString());
		
		return;		
	}

	default:
		DR_ERROR(TEXT("[%s] Invalid projectile Weapon resource type"), *GetName());
		return;
	}
}

void UDRGA_FireProjectile::BuildImpactEffectSpecs(UAbilitySystemComponent* AbilitySystemComponent,
                                                  UDRProjectileWeaponItemDefinition* WeaponDefinition, TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();
	
	if (!IsValid(AbilitySystemComponent)
		|| !IsValid(WeaponDefinition))
	{
		return;
	}
	
	for (const FDRProjectileImpactEffect& EffectData : WeaponDefinition->ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}
		
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(WeaponDefinition);
		
		FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(
			EffectData.EffectClass,
			EffectData.EffectLevel,
			EffectContext);
		
		if (!EffectSpec.IsValid())
		{
			continue;
		}
		
		for (const TPair<FGameplayTag, float>& Pair : EffectData.SetByCallerMagnitudes)
		{
			if (Pair.Key.IsValid())
			{
				EffectSpec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
			}
		}
		
		OutEffectSpecs.Add(EffectSpec);
	}
}

bool UDRGA_FireProjectile::SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo,
	UDRProjectileWeaponItemDefinition* WeaponDefinition,
	const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const
{
	if (ActorInfo == nullptr 
		|| !IsValid(WeaponDefinition))
	{
		return false;
	}
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
	
	if (!IsValid(AvatarActor)
		|| !IsValid(AbilitySystemComponent))
	{
		return false;
	}
	
	UWorld* World = AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		return false;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;
	
	if (AController* Controller = ActorInfo->PlayerController.Get())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{	
		AvatarActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}
	
	const FVector SpawnLocation =AvatarActor->GetActorLocation() + FVector::UpVector * WeaponDefinition->SpawnHeightOffset 
		+ ViewRotation.Vector() * WeaponDefinition->SpawnForwardOffset;
	
	const FTransform SpawnTransform(ViewRotation, SpawnLocation);
	
	APawn* InstigatorPawn = Cast<APawn>(AvatarActor);
	
	ADRProjectile* Projectile = World->SpawnActorDeferred<ADRProjectile>(WeaponDefinition->ProjectileClass, 
		SpawnTransform, AvatarActor, InstigatorPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	
	if (!IsValid(Projectile))
	{
		return false;
	}
	
	int32 SourceTeamId = INDEX_NONE;
	
	if (const ADRPlayerState* DRPlayerState = Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()))
	{
		SourceTeamId = DRPlayerState->GetTeamId();
	}
	
	Projectile->InitializeProjectile(AbilitySystemComponent, ImpactEffectSpecs, WeaponDefinition->WorldImpactData, SourceTeamId);
	
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	
	return true;	
}
