#include "DRPlayerLifecycleComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRJetpackComponent.h"
#include "DeepRaiders/Player/Components/DRPlayerCameraComponent.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/LootBox/Component/DRLootDropComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "Camera/CameraShakeBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRPlayerLifecycleComponent::UDRPlayerLifecycleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	/*
	 * Fall Feedback용 Client RPC를 사용하므로
	 * Component도 복제한다.
	 */
	SetIsReplicatedByDefault(true);
}

void UDRPlayerLifecycleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

ADRPlayerCharacter* UDRPlayerLifecycleComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(GetOwner());
}

void UDRPlayerLifecycleComponent::HandleLanded(float LandingSpeed)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return;
	}

	// const float CalculatedFallDamage = CalculateFallDamage(LandingSpeed);
	//
	// ApplyFallDamage(LandingSpeed);
	//
	// const bool bTookFallDamage = CalculatedFallDamage > KINDA_SMALL_NUMBER;
	// const bool bDied = Character->IsDead();
	//
	// ExecuteFallSoundCueFromServer(bTookFallDamage, bDied);
}

float UDRPlayerLifecycleComponent::CalculateFallDamage(float LandingSpeed) const
{
	const ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return 0.f;
	}

	const float CharacterMaxHealth = Character->GetMaxHealth();

	if (LandingSpeed <= MinFallDamageSpeed || CharacterMaxHealth <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	if (MaxFallDamageSpeed <= MinFallDamageSpeed + KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float NormalizedSpeed = FMath::Clamp((LandingSpeed - MinFallDamageSpeed) / (MaxFallDamageSpeed - MinFallDamageSpeed), 0.f, 1.f);
	const float DamageAlpha = FMath::Pow(NormalizedSpeed, FMath::Max(FallDamageExponent, 0.01f));
	const float MaximumFallDamage = CharacterMaxHealth * FMath::Clamp(MaxFallDamageRatio, 0.f, 1.f);
	return MaximumFallDamage * DamageAlpha;
}

void UDRPlayerLifecycleComponent::ApplyFallDamage(float LandingSpeed)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority() || Character->IsDead())
	{
		return;
	}

	const float CalculatedDamage = CalculateFallDamage(LandingSpeed);

	UE_LOG(LogTemp, Log, TEXT( "[FallDamage] " "Character=%s " "LandingSpeed=%.1f " "CalculatedDamage=%.1f"), 
		*GetNameSafe(Character), LandingSpeed, CalculatedDamage);

	if (CalculatedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float HealthBeforeDamage = Character->GetCurrentHealth();

	const float AppliedDamage = UGameplayStatics::ApplyDamage(Character, CalculatedDamage, Character->GetController(), Character, UDamageType::StaticClass());

	UE_LOG(LogTemp, Warning, TEXT( "[FallDamage] " "Applied Character=%s " "LandingSpeed=%.1f " "Damage=%.1f " "Health=%.1f->%.1f"), 
		*GetNameSafe(Character), LandingSpeed, AppliedDamage, HealthBeforeDamage, Character->GetCurrentHealth());
}

void UDRPlayerLifecycleComponent::PlayLocalCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !ShakeClass)
	{
		return;
	}

	UDRPlayerCameraComponent* PlayerCameraComponent =
		Character->GetPlayerCameraComponent();

	if (!IsValid(PlayerCameraComponent))
	{
		return;
	}

	PlayerCameraComponent->PlayCameraShake(ShakeClass, Scale);
}

void UDRPlayerLifecycleComponent::HandleDeathFromServer()
{
	if (bDeathRagdollApplied)
	{
		return;
	}
	
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority() || !Character->IsDead())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Character, Character);

		FGameplayCueParameters Parameters(Context);
		Parameters.Location = Character->GetActorLocation();
		ASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Sound_Player_Death, Parameters);

		FGameplayTagContainer AbilitiesToCancel;

		AbilitiesToCancel.AddTag(DRGameplayTags::Ability_Attack);
		AbilitiesToCancel.AddTag(DRGameplayTags::Ability_Snow_Absorb);

		ASC->CancelAbilities(&AbilitiesToCancel, nullptr, nullptr);
	}

	if (UDRJetpackComponent* Jetpack = Character->GetJetpackComponent())
	{
		Jetpack->StopFromServer();
	}

	ApplyDeathRagdoll();
	DropDeathItemsFromServer();

	Character->GetWorldTimerManager().SetTimer(
		RespawnTimerHandle, this, &ThisClass::RespawnAtPlayerStart, RespawnDelay, false);

	/*
	 * 기존 외부 참조를 깨지 않기 위해
	 * Character Delegate는 그대로 유지.
	 */
	Character->OnPlayerCharacterDeathDelegate.Broadcast();

	Character->ForceNetUpdate();

	UE_LOG(LogTemp, Warning, TEXT( "[Death] " "Character=%s " "RespawnDelay=%.1f"), *GetNameSafe(Character), RespawnDelay);
}

void UDRPlayerLifecycleComponent::DropDeathItemsFromServer()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();
	UDRLootDropComponent* LootDrop = IsValid(Character) ? Character->GetDeathLootDropComponent() : nullptr;

	if (!IsValid(Character) || !Character->HasAuthority() || !IsValid(LootDrop))
	{
		return;
	}

	TArray<FDRItemInstance> DroppedItems;
	ADRPlayerController* Controller = Cast<ADRPlayerController>(Character->GetController());
	UDRInventoryComponent* Inventory = IsValid(Controller) ? Controller->GetInventoryComponent() : nullptr;

	if (IsValid(Inventory))
	{
		TArray<FDRItemInstance> InventoryDroppedItems;
		TArray<FGuid> DroppedInstanceIds;

		for (const FDRItemInstance& ItemInstance : Inventory->GetItemInstances())
		{
			if (!ItemInstance.IsValid()
				|| !IsValid(ItemInstance.Definition)
				|| !ItemInstance.Definition->bDropOnDeath)
			{
				continue;
			}

			InventoryDroppedItems.Add(ItemInstance);
			DroppedInstanceIds.Add(ItemInstance.InstanceId);
		}

		if (!DroppedInstanceIds.IsEmpty())
		{
			if (Inventory->TryRemoveItemInstanceArray(DroppedInstanceIds))
			{
				DroppedItems.Append(MoveTemp(InventoryDroppedItems));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[DeathDrop] Failed to remove inventory items. Character=%s Count=%d"),
					*GetNameSafe(Character), DroppedInstanceIds.Num());
			}
		}
	}

	AppendCurrencyDeathDropsFromServer(Character->GetPlayerState<ADRPlayerState>(), DroppedItems);

	if (DroppedItems.IsEmpty())
	{
		return;
	}

	LootDrop->SetSpawnMode(EDRLootSpawnMode::AllAtOnce);
	const int32 SpawnedItemCount = LootDrop->SpawnItemInstances(DroppedItems, Character->GetActorTransform());
	if (SpawnedItemCount != DroppedItems.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeathDrop] Some items failed to spawn. Character=%s Requested=%d Spawned=%d"),
			*GetNameSafe(Character), DroppedItems.Num(), SpawnedItemCount);
	}
}

void UDRPlayerLifecycleComponent::AppendCurrencyDeathDropsFromServer(
	ADRPlayerState* PlayerState, TArray<FDRItemInstance>& OutDroppedItems) const
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return;
	}

	constexpr int32 MaxCurrencyDropEntryCount = 2;
	TArray<const FDRDeathCurrencyDropEntry*> ValidEntries;

	for (int32 EntryIndex = 0; EntryIndex < CurrencyDropEntries.Num() && ValidEntries.Num() < MaxCurrencyDropEntryCount; ++EntryIndex)
	{
		const FDRDeathCurrencyDropEntry& Entry = CurrencyDropEntries[EntryIndex];
		if (!IsValid(Entry.ItemDefinition) 
			|| Entry.SnowGaugeValue <= 0)
		{
			continue;
		}

		ValidEntries.Add(&Entry);
	}

	if (CurrencyDropEntries.Num() > MaxCurrencyDropEntryCount)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DeathCurrencyDrop] More than two entries are configured. Only two valid entries are used. Character=%s"),
			*GetNameSafe(GetOwnerCharacter()));
	}

	if (ValidEntries.IsEmpty())
	{
		return;
	}

	const FDRDeathCurrencyDropEntry* FixedRewardEntry = ValidEntries[0];
	for (const FDRDeathCurrencyDropEntry* Entry : ValidEntries)
	{
		if (Entry->SnowGaugeValue < FixedRewardEntry->SnowGaugeValue)
		{
			FixedRewardEntry = Entry;
		}
	}

	int32 FixedRewardItemCount = 0;
	for (int32 ItemIndex = 0; ItemIndex < FMath::Max(0, MinimumCurrencyDropCount); ++ItemIndex)
	{
		FDRItemInstance ItemInstance = DRItemInstanceFactory::Create(FixedRewardEntry->ItemDefinition, 1);
		if (ItemInstance.IsValid())
		{
			OutDroppedItems.Add(MoveTemp(ItemInstance));
			++FixedRewardItemCount;
		}
	}

	const float CurrentSnowGauge = FMath::Max(0.f, PlayerState->GetSnowGauge());
	const float ClampedLossRatio = FMath::Clamp(SnowGaugeLossRatio, 0.f, 1.f);
	const int32 TargetLoss = FMath::FloorToInt(CurrentSnowGauge * ClampedLossRatio);
	const int32 MaxLossDropCount = FMath::Max(0, MaximumLossCurrencyDropCount);

	if (TargetLoss <= 0 || MaxLossDropCount <= 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[DeathCurrencyDrop] Character=%s Current=%.0f TargetLoss=%d ActualLoss=0 FixedItems=%d LossItems=0"),
			*GetNameSafe(GetOwnerCharacter()), CurrentSnowGauge, TargetLoss, FixedRewardItemCount);
		return;
	}

	if (ValidEntries.Num() == 2 && ValidEntries[0]->SnowGaugeValue < ValidEntries[1]->SnowGaugeValue)
	{
		ValidEntries.Swap(0, 1);
	}

	TArray<int32> PlannedCounts;
	PlannedCounts.Init(0, ValidEntries.Num());
	int32 RemainingLoss = TargetLoss;

	for (int32 EntryIndex = 0; EntryIndex < ValidEntries.Num() && RemainingLoss > 0; ++EntryIndex)
	{
		const bool bLastDenomination = EntryIndex == ValidEntries.Num() - 1;
		const int32 DenominationValue = ValidEntries[EntryIndex]->SnowGaugeValue;
		const int32 ItemCount = bLastDenomination
			? FMath::DivideAndRoundUp(RemainingLoss, DenominationValue)
			: RemainingLoss / DenominationValue;

		PlannedCounts[EntryIndex] = ItemCount;
		const int64 RepresentedValue = static_cast<int64>(ItemCount) * DenominationValue;
		RemainingLoss = static_cast<int32>(FMath::Max<int64>(0, RemainingLoss - RepresentedValue));
	}

	int32 PlannedItemCount = 0;
	for (const int32 ItemCount : PlannedCounts)
	{
		PlannedItemCount += ItemCount;
	}

	if (PlannedItemCount > MaxLossDropCount)
	{
		PlannedCounts.Init(0, ValidEntries.Num());
		PlannedCounts[0] = FMath::Min(
			MaxLossDropCount,
			FMath::DivideAndRoundUp(TargetLoss, ValidEntries[0]->SnowGaugeValue));
	}

	int32 LossItemCount = 0;
	int64 LossDropNominalValue = 0;
	for (int32 EntryIndex = 0; EntryIndex < ValidEntries.Num(); ++EntryIndex)
	{
		const FDRDeathCurrencyDropEntry& Entry = *ValidEntries[EntryIndex];
		for (int32 ItemIndex = 0; ItemIndex < PlannedCounts[EntryIndex]; ++ItemIndex)
		{
			FDRItemInstance ItemInstance = DRItemInstanceFactory::Create(Entry.ItemDefinition, 1);
			if (!ItemInstance.IsValid())
			{
				continue;
			}

			OutDroppedItems.Add(MoveTemp(ItemInstance));
			LossDropNominalValue += Entry.SnowGaugeValue;
			++LossItemCount;
		}
	}

	const int32 ActualLoss = static_cast<int32>(FMath::Min<int64>(TargetLoss, LossDropNominalValue));
	if (ActualLoss > 0)
	{
		PlayerState->AddSnowGauge(-static_cast<float>(ActualLoss));
	}

	UE_LOG(LogTemp, Log,
		TEXT("[DeathCurrencyDrop] Character=%s Current=%.0f TargetLoss=%d ActualLoss=%d FixedItems=%d LossItems=%d"),
		*GetNameSafe(GetOwnerCharacter()), CurrentSnowGauge, TargetLoss, ActualLoss, FixedRewardItemCount, LossItemCount);
}

void UDRPlayerLifecycleComponent::ApplyDeathRagdoll()
{
	if (bDeathRagdollApplied)
	{
		return;
	}

	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	bDeathRagdollApplied = true;

	/*
	 * 공격 등 현재 재생 중인 Montage 종료.
	 */
	Character->StopAnimMontage();

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	if (IsValid(Movement))
	{
		Movement->StopMovementImmediately();

		Movement->DisableMovement();
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();

	if (IsValid(Capsule))
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();

	if (IsValid(CharacterMesh))
	{
		CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CharacterMesh->SetAllBodiesSimulatePhysics(true);
		CharacterMesh->SetSimulatePhysics(true);
		CharacterMesh->WakeAllRigidBodies();
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(true);
		Controller->SetIgnoreLookInput(true);
	}

	if (Character->IsLocallyControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, RespawnDelay, FColor::Red, TEXT("YOU DIED"));
	}
}

void UDRPlayerLifecycleComponent::ApplyRagdollKnockback(const FVector& Origin, float Distance)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character)
		|| !Character->HasAuthority()
		|| !Character->IsDead()
		|| Distance <= KINDA_SMALL_NUMBER
		|| Origin.ContainsNaN())
	{
		return;
	}

	FVector Direction = Character->GetActorLocation() - Origin;
	if (!Direction.Normalize())
	{
		Direction = Character->GetActorForwardVector().GetSafeNormal();
	}

	const float VelocityChange = Distance * FMath::Max(RagdollKnockbackVelocityScale, 0.f);
	if (Direction.IsNearlyZero() || VelocityChange <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	MulticastApplyRagdollKnockback(Direction, VelocityChange);
}

void UDRPlayerLifecycleComponent::MulticastApplyRagdollKnockback_Implementation(
	FVector_NetQuantizeNormal Direction,
	float VelocityChange)
{
	ApplyDeathRagdoll();

	ADRPlayerCharacter* Character = GetOwnerCharacter();
	USkeletalMeshComponent* CharacterMesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	if (!IsValid(CharacterMesh)
		|| !CharacterMesh->IsSimulatingPhysics()
		|| VelocityChange <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	CharacterMesh->AddImpulseToAllBodiesBelow(
		FVector(Direction).GetSafeNormal() * VelocityChange,
		RespawnRagdollBoneName,
		true,
		true);
}

void UDRPlayerLifecycleComponent::ClearDeathRagdollPresentation()
{
	if (!bDeathRagdollApplied)
	{
		return;
	}

	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();

	if (IsValid(CharacterMesh))
	{
		/*
		 * 이전 Pawn은 곧 Destroy될 것이므로
		 * 다시 Capsule에 붙이거나 Walking으로
		 * 복구하지 않는다.
		 *
		 * 남아있는 Ragdoll 표현만 제거한다.
		 */
		CharacterMesh->SetAllBodiesSimulatePhysics(false);
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharacterMesh->SetVisibility(false, true);
	}

	bDeathRagdollApplied = false;
}

void UDRPlayerLifecycleComponent::RespawnAtPlayerStart()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	AController* RespawnController = Character->GetController();
	AGameModeBase* GameMode = World->GetAuthGameMode();

	if (!IsValid(RespawnController) || !IsValid(GameMode))
	{
		UE_LOG(LogTemp, Error, TEXT("[Respawn] Invalid Controller or GameMode. Character=%s Controller=%s GameMode=%s"),
			*GetNameSafe(Character), *GetNameSafe(RespawnController), *GetNameSafe(GameMode));

		return;
	}

	/*
	 * 이전 Pawn의 래그돌 표현을 정리한다.
	 * ASC 초기화는 새 Pawn의 PossessedBy()에서 수행한다.
	 */
	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();

	if (IsValid(CharacterMesh))
	{
		CharacterMesh->SetAllBodiesSimulatePhysics(false);
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharacterMesh->SetVisibility(false, true);
	}

	RespawnController->UnPossess();

	/*
	 * RestartPlayer()가 현재 GameMode의
	 * ChoosePlayerStart_Implementation()을 호출한다.
	 */
	GameMode->RestartPlayer(RespawnController);

	APawn* NewPawn = RespawnController->GetPawn();

	if (!IsValid(NewPawn) || NewPawn == Character)
	{
		UE_LOG(LogTemp, Error, TEXT("[Respawn] PlayerStart respawn failed. Controller=%s"),
			*GetNameSafe(RespawnController));

		/*
		 * 새 Pawn 생성에 실패한 경우 이전 Pawn을 파괴하지 않는다.
		 * Controller는 UnPossess 상태이므로 실패 원인을 로그에서 확인해야 한다.
		 */
		return;
	}

	ApplyRespawnInvincibility(Cast<ADRPlayerCharacter>(NewPawn));

	UE_LOG(LogTemp, Log, TEXT("[Respawn] OldPawn=%s NewPawn=%s Location=%s"),
		*GetNameSafe(Character), *GetNameSafe(NewPawn), *NewPawn->GetActorLocation().ToString());

	Character->Destroy();
}

void UDRPlayerLifecycleComponent::RespawnAtRagdollLocation()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	AController* RespawnController = Character->GetController();

	AGameModeBase* GameMode = World->GetAuthGameMode();

	if (!IsValid(RespawnController) || !IsValid(GameMode))
	{
		UE_LOG(LogTemp, Error, TEXT( "[Respawn] " "Invalid Controller or GameMode. " "Character=%s Controller=%s " "GameMode=%s"), 
			*GetNameSafe(Character), *GetNameSafe( RespawnController), *GetNameSafe(GameMode));

		return;
	}

	/*
	 * 물리를 끄기 전에
	 * Ragdoll 주변의 안전 위치를 찾는다.
	 */
	FTransform RagdollRespawnTransform;

	const bool bFoundRagdollRespawnLocation = TryFindRagdollRespawnTransform(RagdollRespawnTransform);
	
	/*
	 * ASC는 PlayerState에 유지되므로
	 * 새 Pawn을 생성하기 전에 이전 생명주기의
	 * 상태를 먼저 정리한다.
	 */
	if (ADRPlayerState* PlayerState =
		Character->GetPlayerState<ADRPlayerState>())
	{
		PlayerState->ResetForRespawn();
	}
	
	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();

	if (IsValid(CharacterMesh))
	{
		CharacterMesh->SetAllBodiesSimulatePhysics(false);
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharacterMesh->SetVisibility(false, true);
	}

	RespawnController->SetIgnoreMoveInput(false);
	RespawnController->SetIgnoreLookInput(false);
	RespawnController->UnPossess();

	if (bFoundRagdollRespawnLocation)
	{
		GameMode->RestartPlayerAtTransform(RespawnController, RagdollRespawnTransform);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT( "[Respawn] " "Safe ragdoll location " "not found. " "Fallback to PlayerStart. " "Character=%s"), *GetNameSafe(Character));

		GameMode->RestartPlayer(RespawnController);
	}

	APawn* NewPawn = RespawnController->GetPawn();

	/*
	 * 안전 위치 Spawn 자체가 실패했다면
	 * PlayerStart로 한 번 더 시도.
	 */
	if ((!IsValid(NewPawn) || NewPawn == Character) && bFoundRagdollRespawnLocation)
	{
		UE_LOG(LogTemp, Warning, TEXT( "[Respawn] " "Ragdoll location spawn " "failed. Retrying at " "PlayerStart. " "Controller=%s"), 
			*GetNameSafe( RespawnController));

		GameMode->RestartPlayer(RespawnController);
		NewPawn = RespawnController->GetPawn();
	}

	if (!IsValid(NewPawn) || NewPawn == Character)
	{
		UE_LOG(LogTemp, Error, TEXT( "[Respawn] " "All respawn attempts " "failed. Controller=%s"), *GetNameSafe( RespawnController));

		/*
		 * 새 Pawn 생성 실패.
		 * 기존 Pawn은 Destroy하지 않는다.
		 */
		return;
	}

	ApplyRespawnInvincibility(Cast<ADRPlayerCharacter>(NewPawn));

	UE_LOG(LogTemp, Warning, TEXT( "[Respawn] " "OldPawn=%s NewPawn=%s " "UsedRagdollLocation=%d " "Location=%s"), 
		*GetNameSafe(Character), *GetNameSafe(NewPawn), bFoundRagdollRespawnLocation, *NewPawn-> GetActorLocation(). ToString());

	Character->Destroy();
}

void UDRPlayerLifecycleComponent::ApplyRespawnInvincibility(
	ADRPlayerCharacter* RespawnedCharacter) const
{
	if (!IsValid(RespawnedCharacter)
		|| !RespawnedCharacter->HasAuthority()
		|| !RespawnInvincibilityEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = RespawnedCharacter->GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Respawn] Invincibility skipped: missing ASC. Character=%s"),
			*GetNameSafe(RespawnedCharacter));
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		RespawnInvincibilityEffectClass, 1.f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle EffectHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	if (EffectHandle.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[Respawn] Invincibility applied. Character=%s Effect=%s"),
			*GetNameSafe(RespawnedCharacter), *GetNameSafe(RespawnInvincibilityEffectClass.Get()));
	}
}

bool UDRPlayerLifecycleComponent::TryFindRagdollRespawnTransform(FTransform& OutRespawnTransform) const
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	const UCapsuleComponent* CharacterCapsule = Character->GetCapsuleComponent();
	const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();

	if (!IsValid(World) || !IsValid(CharacterMesh) || !IsValid(CharacterCapsule) || !IsValid(MovementComponent))
	{
		return false;
	}

	FVector RagdollLocation = CharacterMesh->GetComponentLocation();

	if (CharacterMesh->DoesSocketExist(RespawnRagdollBoneName))
	{
		RagdollLocation = CharacterMesh->GetSocketLocation(RespawnRagdollBoneName);
	}

	const float CapsuleRadius = CharacterCapsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = CharacterCapsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	const FName CapsuleCollisionProfile = CharacterCapsule->GetCollisionProfileName();
	
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RagdollRespawnCapsuleSweep), false, Character);
	QueryParams.AddIgnoredActor(Character);
	
	TArray<FVector2D> SearchOffsets;
	SearchOffsets.Add(FVector2D::ZeroVector);

	constexpr int32 DirectionCount = 8;
	for (int32 RingIndex = 1; RingIndex <= RespawnSearchRingCount; ++RingIndex)
	{
		const float SearchDistance = RespawnSearchStep * RingIndex;

		for (int32 DirectionIndex = 0; DirectionIndex < DirectionCount; ++DirectionIndex)
		{
			const float AngleRadians = 2.f * PI * static_cast<float>(DirectionIndex) / static_cast<float>(DirectionCount);
			SearchOffsets.Add(FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)) * SearchDistance);
		}
	}

	for (const FVector2D& Offset : SearchOffsets)
	{
		const FVector SearchCenter(RagdollLocation.X + Offset.X, RagdollLocation.Y + Offset.Y, RagdollLocation.Z);
		const FVector SweepStart = SearchCenter + FVector(0.f, 0.f, RespawnSweepStartHeight);
		const FVector SweepEnd = SearchCenter - FVector(0.f, 0.f, RespawnGroundTraceDistance);

		FHitResult GroundHit;
		const bool bHitGround = World->SweepSingleByProfile(GroundHit, SweepStart, SweepEnd, FQuat::Identity, CapsuleCollisionProfile, CapsuleShape, QueryParams);
		if (!bHitGround || GroundHit.bStartPenetrating)
		{
			continue;
		}

		if (!MovementComponent->IsWalkable(GroundHit))
		{
			continue;
		}

		const FVector CandidateLocation = GroundHit.Location + FVector(0.f, 0.f, RespawnGroundClearance);
		const bool bBlocked = World->OverlapBlockingTestByProfile(CandidateLocation, FQuat::Identity, CapsuleCollisionProfile, CapsuleShape, QueryParams);

		if (bBlocked)
		{
			continue;
		}

		OutRespawnTransform = FTransform(FRotator(0.f, Character->GetActorRotation().Yaw, 0.f), CandidateLocation, FVector::OneVector);

#if ENABLE_DRAW_DEBUG

		DrawDebugCapsule(World, CandidateLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Green, false, 5.f);

#endif

		return true;
	}

#if ENABLE_DRAW_DEBUG

	DrawDebugSphere(World, RagdollLocation, 30.f, 16, FColor::Red, false, 5.f);

#endif

	return false;
}

void UDRPlayerLifecycleComponent::HandleControllerReady()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	AController* Controller = Character->GetController();

	if (!IsValid(Controller))
	{
		return;
	}

	Controller->SetIgnoreMoveInput(false);
	Controller->SetIgnoreLookInput(false);
}

void UDRPlayerLifecycleComponent::BindAbilitySystem(UAbilitySystemComponent* ASC)
{
	UnbindAbilitySystem();

	if (!IsValid(ASC))
	{
		return;
	}

	BoundASC = ASC;

	DeadTagChangedHandle = ASC->RegisterGameplayTagEvent(
		DRGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);
}

void UDRPlayerLifecycleComponent::UnbindAbilitySystem()
{
	if (BoundASC.IsValid() && DeadTagChangedHandle.IsValid())
	{
		BoundASC->RegisterGameplayTagEvent(DRGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagChangedHandle);
	}

	DeadTagChangedHandle.Reset();
	BoundASC.Reset();
}

void UDRPlayerLifecycleComponent::ExecuteFallSoundCueFromServer(bool bTookFallDamage, bool bDied)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayTag SoundTag;

	if (bDied)
	{
		SoundTag = DRGameplayTags::GameplayCue_Sound_Player_FallDeath;
	}
	else if (bTookFallDamage)
	{
		SoundTag = DRGameplayTags::GameplayCue_Sound_Player_FallDamage;
	}
	else
	{
		SoundTag = DRGameplayTags::GameplayCue_Sound_Player_Land;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Character, Character);

	FGameplayCueParameters Parameters(Context);
	Parameters.Location = Character->GetActorLocation();

	ASC->ExecuteGameplayCue(SoundTag, Parameters);
}

void UDRPlayerLifecycleComponent::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	/*
	 * State.Dead 제거.
	 *
	 * ASC가 PlayerState에 있기 때문에 Respawn 시
	 * 기존 Pawn의 Lifecycle도 이 이벤트를 받을 수 있다.
	 *
	 * 기존 Pawn의 Client Ragdoll 표현을 여기서 정리한다.
	 */
	if (NewCount <= 0)
	{
		ClearDeathRagdollPresentation();
		return;
	}

	/*
	 * State.Dead 추가.
	 */
	if (Character->HasAuthority())
	{
		HandleDeathFromServer();
		return;
	}

	/*
	 * Client는 State.Dead 복제를 통해
	 * 사망 표현만 실행한다.
	 */
	ApplyDeathRagdoll();
}
