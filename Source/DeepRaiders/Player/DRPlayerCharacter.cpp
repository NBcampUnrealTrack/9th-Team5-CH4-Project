#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "VoxelComponents/VoxelNoClippingComponent.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"
#include "DeepRaiders/Player/Components/DRJetpackComponent.h"
#include "DeepRaiders/Player/Components/DRItemActionPresentationComponent.h"
#include "DeepRaiders/Player/Components/DRHealthComponent.h"
#include "DeepRaiders/Player/Components/DRPlayerLifecycleComponent.h"
#include "DeepRaiders/Player/Components/DRHeldItemComponent.h"

#include "AbilitySystemComponent.h"
#include "GAS/DRPlayerAttributeSet.h"

ADRPlayerCharacter::ADRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 이 Actor가 서버에서 클라이언트로 복제되도록 설정
	bReplicates = true;
	
	MiningComponent =
		CreateDefaultSubobject<UDRMiningComponent>(
			TEXT("MiningComponent"));

	VoxelNoClippingComponent =
		CreateDefaultSubobject<UVoxelNoClippingComponent>(
			TEXT("VoxelNoClippingComponent"));

	VoxelNoClippingComponent->SetupAttachment(
		GetCapsuleComponent());

	VoxelNoClippingComponent->TickRate = 0.03f;
	VoxelNoClippingComponent->SearchRange = 8;
	VoxelNoClippingComponent->bEnableDefaultBehavior = true;
	VoxelNoClippingComponent->Speed = 6000.f;

	TeleportComponent = CreateDefaultSubobject<UDRTeleportComponent>(TEXT("TeleportComponent"));

	MeleeCombatComponent = CreateDefaultSubobject<UDRMeleeCombatComponent>(TEXT("MeleeCombatComponent"));
	
	JetpackComponent = CreateDefaultSubobject<UDRJetpackComponent>(TEXT("JetpackComponent"));
	
	ItemActionPresentationComponent = CreateDefaultSubobject<UDRItemActionPresentationComponent>(TEXT("ItemActionPresentationComponent"));
	
	HealthComponent = CreateDefaultSubobject<UDRHealthComponent>(TEXT("HealthComponent"));
	
	PlayerLifecycleComponent = CreateDefaultSubobject<UDRPlayerLifecycleComponent>(TEXT("PlayerLifecycleComponent"));
	
	HeldItemComponent = CreateDefaultSubobject<UDRHeldItemComponent>(TEXT("HeldItemComponent"));
	
	// Actor 이동 정보도 복제
	SetReplicateMovement(true);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;

	FirstPersonCamera =
		CreateDefaultSubobject<UCameraComponent>(
			TEXT("FirstPersonCamera"));

	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// 1인칭 장비 위치 - 애니메이션 피벗
	// 이 장비 흔들때 나중에 이 피벗만 움직이도록 하기위해서 설정
	FirstPersonEquipmentRoot =
	CreateDefaultSubobject<USceneComponent>(
		TEXT("FirstPersonEquipmentRoot"));

	FirstPersonEquipmentRoot->SetupAttachment(FirstPersonCamera);
	
	// 1인칭 시점 손에 들릴 메쉬
	FirstPersonHandEquipmentMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("FirstPersonHandEquipmentMesh"));

	FirstPersonHandEquipmentMesh->SetupAttachment(
		FirstPersonEquipmentRoot);

	FirstPersonHandEquipmentMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	FirstPersonHandEquipmentMesh->SetGenerateOverlapEvents(false);
	FirstPersonHandEquipmentMesh->SetOnlyOwnerSee(true);
	FirstPersonHandEquipmentMesh->SetCastShadow(false);
	FirstPersonHandEquipmentMesh->SetIsReplicated(false);

	// 월드 손 장비
	WorldHandEquipmentMesh =
	CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("WorldHandEquipmentMesh"));

	WorldHandEquipmentMesh->SetupAttachment(
		GetMesh(),
		TEXT("S_HandGrip_R"));

	WorldHandEquipmentMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	WorldHandEquipmentMesh->SetGenerateOverlapEvents(false);

	// 자기 화면에서는 1인칭 장비를 별도로 사용하므로 숨김
	WorldHandEquipmentMesh->SetOwnerNoSee(true);
	WorldHandEquipmentMesh->SetCastHiddenShadow(true);
	WorldHandEquipmentMesh->SetIsReplicated(false);

	// 등 뒤에 달릴 장비 - 제트팩
	WorldBackEquipmentMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("WorldBackEquipmentMesh"));

	WorldBackEquipmentMesh->SetupAttachment(
		GetMesh(),
		TEXT("S_Back"));

	WorldBackEquipmentMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	WorldBackEquipmentMesh->SetGenerateOverlapEvents(false);

	// 프로토타입에서는 자기 카메라에 제트팩이 끼어들지 않게 숨기는 편이 안전
	WorldBackEquipmentMesh->SetOwnerNoSee(true);
	WorldBackEquipmentMesh->SetCastHiddenShadow(true);
	WorldBackEquipmentMesh->SetIsReplicated(false);
}

UAbilitySystemComponent* ADRPlayerCharacter::GetAbilitySystemComponent() const
{
	const ADRPlayerState* DRPlayerState =
		GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return nullptr;
	}

	return DRPlayerState->GetAbilitySystemComponent();
}

void ADRPlayerCharacter::Landed(
	const FHitResult& Hit)
{
	const float LandingSpeed =
		FMath::Max(
			0.f,
			-GetVelocity().Z);

	Super::Landed(Hit);

	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->HandleLanded(
			LandingSpeed);
	}
}

void ADRPlayerCharacter::RequestThrowHeldItem()
{
	if (!IsLocallyControlled() || IsDead() || !HasHeldItemAction(EDRItemActionType::Throw))
	{
		return;
	}

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetController()))
	{
		PlayerController->RequestThrowHeldItem();
	}
}

float ADRPlayerCharacter::GetCurrentHealth() const
{
	return IsValid(HealthComponent)
		? HealthComponent->GetCurrentHealth()
		: 0.f;
}

float ADRPlayerCharacter::GetMaxHealth() const
{
	return IsValid(HealthComponent)
		? HealthComponent->GetMaxHealth()
		: 0.f;
}

float ADRPlayerCharacter::GetHealthRatio() const
{
	return IsValid(HealthComponent)
		? HealthComponent->GetHealthRatio()
		: 0.f;
}

bool ADRPlayerCharacter::IsDead() const
{
	return IsValid(HealthComponent) &&
		HealthComponent->IsDead();
}

float ADRPlayerCharacter::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!HasAuthority() ||
		!IsValid(HealthComponent))
	{
		return 0.f;
	}

	return HealthComponent->ApplyDamage(DamageAmount);
}

void ADRPlayerCharacter::RequestPrimaryItemAction(
	EDRItemActionTriggerEvent TriggerEvent)
{
	if (IsValid(HeldItemComponent))
	{
		HeldItemComponent->
			RequestPrimaryAction(
				TriggerEvent);
	}
}

void ADRPlayerCharacter::RequestSecondaryItemAction(
	EDRItemActionTriggerEvent TriggerEvent)
{
	if (IsValid(HeldItemComponent))
	{
		HeldItemComponent->RequestSecondaryAction(TriggerEvent);
	}
}

bool ADRPlayerCharacter::HasHeldItemAction(EDRItemActionType ActionType) const
{
	return IsValid(HeldItemComponent) &&
		HeldItemComponent->HasAction(ActionType);
}

void ADRPlayerCharacter::NotifyMineConfirmedFromServer()
{
	if (!HasAuthority() ||
		!IsValid(
			ItemActionPresentationComponent))
	{
		return;
	}

	ItemActionPresentationComponent->PlayWorldActionFromServer(EDRItemActionType::Dig);
}

void ADRPlayerCharacter::PlayMeleeWorldPresentationFromServer()
{
	if (!HasAuthority() ||
		!IsValid(
			ItemActionPresentationComponent))
	{
		return;
	}

	ItemActionPresentationComponent->PlayWorldActionFromServer(EDRItemActionType::MeleeAttack);
}

void ADRPlayerCharacter::PlayMeleeHitPresentationFromServer(
	ADRPlayerCharacter* HitPlayer,
	bool bKilled,
	const FVector& ImpactLocation)
{
	if (!HasAuthority() ||
		!IsValid(HitPlayer) ||
		!IsValid(
			ItemActionPresentationComponent))
	{
		return;
	}

	ItemActionPresentationComponent->
		PlayMeleeHitFeedbackFromServer(
			HitPlayer,
			bKilled,
			ImpactLocation);
}

float ADRPlayerCharacter::GetDisplayedJetpackFuelRatio() const
{
	return IsValid(JetpackComponent)
		? JetpackComponent->
			GetDisplayedFuelRatio()
		: 0.f;
}

void ADRPlayerCharacter::ReconcileJetpackFuelFromServer(
	float ServerFuel)
{
	if (IsValid(JetpackComponent))
	{
		JetpackComponent->
			ReconcileFuelFromServer(
				ServerFuel);
	}
}

void ADRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ADRPlayerCharacter::InitializeAbilitySystem()
{
	ADRPlayerState* DRPlayerState =
		GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		DRPlayerState->GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return;
	}

	ASC->InitAbilityActorInfo(
		DRPlayerState,
		this);
	
	const UDRPlayerAttributeSet* RegisteredAttributeSet =
		ASC->GetSet<UDRPlayerAttributeSet>();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GAS][AttributeSet] Direct=%s Registered=%s Same=%d"),
		*GetNameSafe(DRPlayerState->GetPlayerAttributeSet()),
		*GetNameSafe(RegisteredAttributeSet),
		DRPlayerState->GetPlayerAttributeSet() == RegisteredAttributeSet);
}

void ADRPlayerCharacter::MoveInput(
	const FVector2D& MoveInput)
{
	if (!Controller)
	{
		return;
	}

	const FRotator ControlRotation =
		Controller->GetControlRotation();

	const FRotator YawRotation(
		0.f,
		ControlRotation.Yaw,
		0.f);

	const FVector ForwardDirection =
		FRotationMatrix(YawRotation)
		.GetUnitAxis(EAxis::X);

	const FVector RightDirection =
		FRotationMatrix(YawRotation)
		.GetUnitAxis(EAxis::Y);

	AddMovementInput(
		ForwardDirection,
		MoveInput.Y);

	AddMovementInput(
		RightDirection,
		MoveInput.X);
}

void ADRPlayerCharacter::LookInput(
	const FVector2D& LookInput)
{
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void ADRPlayerCharacter::PossessedBy(
	AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const UDRPlayerAttributeSet* AttributeSet =
			ASC->GetSet<UDRPlayerAttributeSet>();

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GAS][GE_TestAddSnow] Snow=%.1f"),
			AttributeSet
				? AttributeSet->GetSnowGauge()
				: -1.f);
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[GAS][PossessedBy] "
			"Character=%s "
			"Authority=%d "
			"Local=%d "
			"LocalRole=%d "
			"PlayerState=%s "
			"ASC=%s"),
		*GetNameSafe(this),
		HasAuthority(),
		IsLocallyControlled(),
		static_cast<int32>(GetLocalRole()),
		*GetNameSafe(GetPlayerState()),
		*GetNameSafe(GetAbilitySystemComponent()));
	
	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->HandleControllerReady();
	}
}

void ADRPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->HandleControllerReady();
	}
}

void ADRPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[GAS][OnRep_PlayerState] "
			"Character=%s "
			"Authority=%d "
			"Local=%d "
			"LocalRole=%d "
			"PlayerState=%s "
			"ASC=%s"),
		*GetNameSafe(this),
		HasAuthority(),
		IsLocallyControlled(),
		static_cast<int32>(GetLocalRole()),
		*GetNameSafe(GetPlayerState()),
		*GetNameSafe(GetAbilitySystemComponent()));
}

void ADRPlayerCharacter::SetHeldItemDefinition(
	UDRItemDefinition* NewItemDefinition)
{
	if (IsValid(HeldItemComponent))
	{
		HeldItemComponent->SetHeldItemDefinition(NewItemDefinition);
	}
}

void ADRPlayerCharacter::ApplyHandEquipmentVisual(
	UStaticMesh* FirstPersonMesh,
	UStaticMesh* WorldMesh,
	const FTransform& FirstPersonTransform,
	const FTransform& WorldTransform)
{
	// 월드 전용 메시가 없으면 1인칭 메시를 대신 사용한다.
	UStaticMesh* EffectiveWorldMesh =
		IsValid(WorldMesh) ? WorldMesh : FirstPersonMesh;

	FirstPersonHandEquipmentMesh->SetStaticMesh(FirstPersonMesh);
	FirstPersonHandEquipmentMesh->SetRelativeTransform(
		FirstPersonTransform);
	FirstPersonHandEquipmentMesh->SetVisibility(
		IsValid(FirstPersonMesh),
		true);

	WorldHandEquipmentMesh->SetStaticMesh(EffectiveWorldMesh);
	WorldHandEquipmentMesh->SetRelativeTransform(WorldTransform);
	WorldHandEquipmentMesh->SetVisibility(
		IsValid(EffectiveWorldMesh),
		true);
}

void ADRPlayerCharacter::ClearHandEquipmentVisual()
{
	FirstPersonHandEquipmentMesh->SetStaticMesh(nullptr);
	FirstPersonHandEquipmentMesh->SetVisibility(false, true);

	WorldHandEquipmentMesh->SetStaticMesh(nullptr);
	WorldHandEquipmentMesh->SetVisibility(false, true);
}

void ADRPlayerCharacter::ApplyBackEquipmentVisual(
	UStaticMesh* BackMesh,
	const FTransform& BackTransform)
{
	WorldBackEquipmentMesh->SetStaticMesh(BackMesh);
	WorldBackEquipmentMesh->SetRelativeTransform(BackTransform);

	WorldBackEquipmentMesh->SetVisibility(
		IsValid(BackMesh),
		true);
}

void ADRPlayerCharacter::ClearBackEquipmentVisual()
{
	WorldBackEquipmentMesh->SetStaticMesh(nullptr);
	WorldBackEquipmentMesh->SetVisibility(false, true);
}

void ADRPlayerCharacter::RefreshJetpackVisual()
{
	if (IsValid(JetpackComponent))
	{
		JetpackComponent->RefreshVisual();
	}
}

void ADRPlayerCharacter::HandleJumpPressed()
{
	if (!IsLocallyControlled() ||
		IsDead())
	{
		return;
	}

	Jump();
}

void ADRPlayerCharacter::HandleJumpReleased()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	StopJumping();
}