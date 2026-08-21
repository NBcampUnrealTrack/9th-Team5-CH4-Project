// Fill out your copyright notice in the Description page of Project Settings.


#include "DRWorldItemActor.h"
#include "AbilitySystemGlobals.h"
#include "DRItemInstance.h"
#include "DRItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "GameplayCueManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "DeepRaiders/DeepRaiders.h"

ADRWorldItemActor::ADRWorldItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	SetRootComponent(StaticMeshComponent);
	
	StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	StaticMeshComponent->SetSimulatePhysics(true);
	StaticMeshComponent->SetNotifyRigidBodyCollision(true);
	StaticMeshComponent->BodyInstance.bStartAwake = false;
	StaticMeshComponent->OnComponentHit.AddDynamic(this, &ThisClass::HandleStaticMeshHit);
}

void ADRWorldItemActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority())
	{
		// 클라이언트는 OnRep_ItemInstance에서 초기화 됨
		return;
	}
	
	if (!ItemInstance.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Spawned without Iteminstance"), *GetName());

		Destroy();
		return;
	}
	
	RefreshItemPresentation();
	
	if (StaticMeshComponent->IsSimulatingPhysics())
	{
		StaticMeshComponent->PutRigidBodyToSleep();
	}
}

void ADRWorldItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, ItemInstance);
	DOREPLIFETIME(ThisClass, WorldItemState);
	DOREPLIFETIME(ThisClass, ThrowingPawn);
}

bool ADRWorldItemActor::SetInitialItemInstance(FDRItemInstance InItemInstance)
{
	if (!HasAuthority())
	{
		return false;
	}
	
	// ItemInstance 설정은 BeginPlay 전에 완료되어야 함.
	if (HasActorBegunPlay())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] ItemInstance must be assigned before BeginPlay"), *GetName());
		
		return false;
	}
	
	if (ItemInstance.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] ItemInstance is already assigned."), *GetName());
		
		return false;
	}
	
	if (!InItemInstance.IsValid()
		|| !InItemInstance.InstanceId.IsValid())
	{
		return false;
	}
	
	ItemInstance = InItemInstance;
	
	return true;
}

void ADRWorldItemActor::ApplyDropImpulse(const FVector& Impulse)
{
	if (!HasAuthority()
		|| !IsValid(StaticMeshComponent)
		|| !StaticMeshComponent->IsSimulatingPhysics())
	{
		return;
	}
	
	ArmGroundHitEvent();
	StaticMeshComponent->WakeAllRigidBodies();
	StaticMeshComponent->AddImpulse(Impulse, NAME_None, true);
}

void ADRWorldItemActor::MarkAsThrown(APawn* Thrower)
{
	if (!HasAuthority() || !IsValid(Thrower))
	{
		return;
	}

	ThrowingPawn = Thrower;
	WorldItemState = EDRWorldItemState::Thrown;
	ApplyWorldItemCollision();
	ForceNetUpdate();
}

void ADRWorldItemActor::BroadcastMined()
{
	if (HasAuthority())
	{
		MulticastPlayActiveSound();
	}
}

void ADRWorldItemActor::MulticastPlayActiveSound_Implementation()
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	if (IsValid(Definition) && IsValid(Definition->ActiveSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Definition->ActiveSound, GetActorLocation());
	}
}

void ADRWorldItemActor::ArmGroundHitEvent()
{
	bGroundHitEventArmed = true;
	GroundHitArmHeight = GetActorLocation().Z;
}

void ADRWorldItemActor::BroadcastDropped()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bGroundHitEventArmed)
	{
		return;
	}

	const float FallHeight = GroundHitArmHeight - GetActorLocation().Z;

	// 첫 바닥 판정 후 다음 분리 또는 투척까지 잠근다.
	bGroundHitEventArmed = false;
	if (FallHeight < MinimumDropSoundHeight)
	{
		return;
	}

	MulticastPlayDroppedSound();
}

void ADRWorldItemActor::HandleStaticMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	if (WorldItemState == EDRWorldItemState::Thrown)
	{
		WorldItemState = EDRWorldItemState::Dropped;
		ThrowingPawn = nullptr;
		ApplyWorldItemCollision();
		ForceNetUpdate();
	}

	if (bGroundHitEventArmed && Hit.ImpactNormal.Z >= 0.5f)
	{
		BroadcastDropped();
	}
}

void ADRWorldItemActor::MulticastPlayPickupSound_Implementation(APawn* Interactor)
{
	if (!IsValid(Interactor))
	{
		return;
	}

	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	FGameplayCueParameters CueParameters;
	CueParameters.Location = GetActorLocation();
	CueParameters.Instigator = Interactor;
	CueParameters.EffectCauser = this;
	CueParameters.SourceObject = Definition;

	if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		CueManager->HandleGameplayCue(Interactor,
			DRGameplayTags::GameplayCue_Sound_Item_PickedUp,
			EGameplayCueEvent::Executed, CueParameters);
	}
}

void ADRWorldItemActor::MulticastPlayDroppedSound_Implementation()
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	if (IsValid(Definition) && Definition->Category == EDRItemCategory::Ore)
	{
		FGameplayCueParameters CueParameters;
		CueParameters.OriginalTag = DRGameplayTags::GameplayCue_Sound_Ore_Dropped;
		CueParameters.Location = GetActorLocation();
		CueParameters.EffectCauser = this;
		CueParameters.SourceObject = Definition;

		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueManager->HandleGameplayCue(this, DRGameplayTags::GameplayCue_Sound_Ore_Dropped,
				EGameplayCueEvent::Executed, CueParameters);
		}

		return;
	}

	if (IsValid(Definition) && IsValid(Definition->DroppedSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Definition->DroppedSound, GetActorLocation(),
			FRotator::ZeroRotator, 1.f, 1.f, 0.f, nullptr, nullptr, this);
	}
}

bool ADRWorldItemActor::IsPickupAvailable() const
{
	// 기본적으로 모든 WorldItemActor는 인벤토리에 넣을 수 있다.
	return true;
}

bool ADRWorldItemActor::FinalizePickup()
{
	return Destroy();
}

void ADRWorldItemActor::ResetInteractionState()
{
	bInteractionInProgress = false;
}

void ADRWorldItemActor::OnRep_ItemInstance()
{
	RefreshItemPresentation();
	
	const FRepMovement& RepMovement = GetReplicatedMovement();
	
	if (RepMovement.bRepPhysics 
		&& RepMovement.bSimulatedPhysicSleep
		&& StaticMeshComponent->IsSimulatingPhysics())
	{
		StaticMeshComponent->PutRigidBodyToSleep();
	}
}

void ADRWorldItemActor::OnRep_WorldItemState()
{
	ApplyWorldItemCollision();
}

void ADRWorldItemActor::ApplyWorldItemCollision()
{
	if (!IsValid(StaticMeshComponent))
	{
		return;
	}

	if (IgnoredThrower.IsValid() && IgnoredThrower.Get() != ThrowingPawn)
	{
		StaticMeshComponent->IgnoreActorWhenMoving(IgnoredThrower.Get(), false);
		IgnoredThrower.Reset();
	}

	if (WorldItemState == EDRWorldItemState::Thrown)
	{
		StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	}
	else
	{
		// Dropped 상태에서는 지면과 다른 물리 아이템만 막는다.
		StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		StaticMeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		StaticMeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		StaticMeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		StaticMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	if (IsValid(ThrowingPawn))
	{
		StaticMeshComponent->IgnoreActorWhenMoving(ThrowingPawn, true);
		IgnoredThrower = ThrowingPawn;
	}
}

void ADRWorldItemActor::RefreshItemPresentation()
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	
	if (Definition == nullptr
		|| !ItemInstance.IsValid())
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
		StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		SetActorHiddenInGame(true);
		return;
	}
	
	StaticMeshComponent->SetStaticMesh(Definition->WorldMesh);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ApplyWorldItemCollision();
	
	SetActorHiddenInGame(false);	
}

#pragma region Interactable
bool ADRWorldItemActor::CanInteract_Implementation(APawn* Interactor) const
{
	if (!HasAuthority()
		|| bInteractionInProgress
		|| !ItemInstance.IsValid()
		|| !IsValid(Interactor)
		|| !IsPickupAvailable())
	{
		return false;
	}
	
	const ADRPlayerController* Controller = Cast<ADRPlayerController>(Interactor->GetController());
	const UDRInventoryComponent* Inventory = IsValid(Controller) ? Controller->GetInventoryComponent() : nullptr;
	
	return IsValid(Inventory) && Inventory->CanAddItemInstance(ItemInstance);	
}

bool ADRWorldItemActor::Interact_Implementation(APawn* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}
	
	ADRPlayerController* Controller = Cast<ADRPlayerController>(Interactor->GetController());
	UDRInventoryComponent* Inventory = IsValid(Controller) ? Controller->GetInventoryComponent() : nullptr;
	
	if (!IsValid(Inventory))
	{
		return false;
	}
	
	bInteractionInProgress = true;
	
	if (!Inventory->TryAddItemInstance(ItemInstance))
	{
		ResetInteractionState();
		return false;
	}
	
	MulticastPlayPickupSound(Interactor);
	
	if (!FinalizePickup())
	{
		DR_ERROR(TEXT("[%s] Failed to finalize pickup."), *GetName());
		
		/*
		 * 인벤토리 추가는 이미 완료
		 * 상호작용 허용 시 아이템이 복제될 수 있으므로
		 * bInteractionInProgress 유지
		 */
		
		return false;
	}
	
	return true;
}
#pragma endregion
