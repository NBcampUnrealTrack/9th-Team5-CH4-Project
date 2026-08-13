#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "VoxelComponents/VoxelNoClippingComponent.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"
#include "DeepRaiders/Player/Components/DRJetpackComponent.h"
#include "DeepRaiders/Player/Components/DRItemActionPresentationComponent.h"
#include "DeepRaiders/Player/Components/DRHealthComponent.h"
#include "DeepRaiders/Player/Components/DRPlayerLifecycleComponent.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"

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

void ADRPlayerCharacter::Landed(
	const FHitResult& Hit)
{
	/*
	 * Super 이후 Z Velocity가 바뀔 수 있으므로
	 * 착지 직전 속도 저장.
	 */
	const float LandingSpeed =
		FMath::Max(
			0.f,
			-GetVelocity().Z);

	Super::Landed(Hit);

	if (IsValid(JetpackComponent))
	{
		JetpackComponent->
			HandleLanded();
	}

	if (IsValid(
			PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->
			HandleLanded(
				LandingSpeed);
	}
}

bool ADRPlayerCharacter::RequestMine()
{
	if (!IsLocallyControlled() ||
		IsDead() ||
		!HasHeldItemAction(EDRItemActionType::Dig))
	{
		return false;
	}

	if (!IsValid(MiningComponent))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Mining] MiningComponent is invalid. "
				"Character=%s"),
			*GetName());

		return false;
	}

	return MiningComponent->TryMine();
}

void ADRPlayerCharacter::RequestMeleeAttack()
{
	if (IsValid(MeleeCombatComponent))
	{
		MeleeCombatComponent->RequestAttack();
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
	if (!IsLocallyControlled() ||
		IsDead() ||
		!IsValid(HeldItemDefinition))
	{
		return;
	}

	if (HeldItemDefinition->PrimaryActionTriggerEvent != TriggerEvent)
	{
		return;
	}

	ExecuteHeldItemAction(HeldItemDefinition->PrimaryAction);
}

void ADRPlayerCharacter::RequestSecondaryItemAction(
	EDRItemActionTriggerEvent TriggerEvent)
{
	if (!IsLocallyControlled() ||
		IsDead() ||
		!IsValid(HeldItemDefinition))
	{
		return;
	}

	if (HeldItemDefinition->SecondaryActionTriggerEvent != TriggerEvent)
	{
		return;
	}

	ExecuteHeldItemAction(HeldItemDefinition->SecondaryAction);
}

bool ADRPlayerCharacter::HasHeldItemAction(EDRItemActionType ActionType) const
{
	if (!IsValid(HeldItemDefinition) || ActionType == EDRItemActionType::None)
	{
		return false;
	}

	return HeldItemDefinition->PrimaryAction == ActionType || HeldItemDefinition->SecondaryAction == ActionType;
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

	PrintNetworkState(TEXT("BeginPlay"));
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

	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->HandleControllerReady();
	}

	if (HasAuthority())
	{
		ADRPlayerState* DRPlayerState =
			GetPlayerState<ADRPlayerState>();

		if (IsValid(DRPlayerState))
		{
			// 임시 테스트: 스폰 즉시 제트팩 지급
			DRPlayerState->GrantJetpack();
		}
	}

	PrintNetworkState(TEXT("PossessedBy"));
}

void ADRPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->HandleControllerReady();
	}

	PrintNetworkState(TEXT("OnRep_Controller"));
}

void ADRPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (IsValid(JetpackComponent))
	{
		JetpackComponent->
			HandlePlayerStateReady();
	}
}

void ADRPlayerCharacter::PrintNetworkState(const TCHAR* Context) const
{
	const TCHAR* NetModeString = TEXT("Unknown");

	switch (GetNetMode())
	{
	case NM_Standalone:
		NetModeString = TEXT("Standalone");
		break;

	case NM_ListenServer:
		NetModeString = TEXT("ListenServer");
		break;

	case NM_DedicatedServer:
		NetModeString = TEXT("DedicatedServer");
		break;

	case NM_Client:
		NetModeString = TEXT("Client");
		break;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[%s] Name=%s NetMode=%s Authority=%d Local=%d Controller=%s Owner=%s LocalRole=%s"),
		Context,
		*GetName(),
		NetModeString,
		HasAuthority(),
		IsLocallyControlled(),
		*GetNameSafe(GetController()),
		*GetNameSafe(GetOwner()),
		*UEnum::GetValueAsString(GetLocalRole())
	);
}

void ADRPlayerCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(
		ADRPlayerCharacter,
		HeldItemDefinition);
}

void ADRPlayerCharacter::ExecuteHeldItemAction(
	EDRItemActionType ActionType)
{
	if (!CanStartLocalItemAction())
	{
		return;
	}

	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		{
			if (!RequestMine())
			{
				return;
			}

			NextLocalItemActionTime =
				GetWorld()->GetTimeSeconds() +
				GetItemActionCooldown(
					EDRItemActionType::Dig);

			PlayFirstPersonItemActionPresentation(
				EDRItemActionType::Dig);

			break;
		}

	case EDRItemActionType::MeleeAttack:
		{
			NextLocalItemActionTime =
				GetWorld()->GetTimeSeconds() +
				GetItemActionCooldown(
					EDRItemActionType::MeleeAttack);

			PlayFirstPersonItemActionPresentation(
				EDRItemActionType::MeleeAttack);

			RequestMeleeAttack();
			break;
		}

	case EDRItemActionType::Throw:
		RequestThrowHeldItem();
		break;

	case EDRItemActionType::None:
	default:
		break;
	}
}

bool ADRPlayerCharacter::CanStartLocalItemAction() const
{
	const UWorld* World = GetWorld();

	return IsValid(World) &&
		World->GetTimeSeconds() >= NextLocalItemActionTime;
}

float ADRPlayerCharacter::GetItemActionCooldown(
	EDRItemActionType ActionType) const
{
	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		return DigActionCooldown;

	case EDRItemActionType::MeleeAttack:
		return IsValid(MeleeCombatComponent)
			? MeleeCombatComponent->GetAttackDuration()
			: 0.f;

	default:
		return 0.f;
	}
}

void ADRPlayerCharacter::PlayFirstPersonItemActionPresentation(
	EDRItemActionType ActionType)
{
	if (IsValid(
			ItemActionPresentationComponent))
	{
		ItemActionPresentationComponent->
			PlayFirstPersonAction(
				ActionType);
	}
}

void ADRPlayerCharacter::ServerRequestDigPresentation_Implementation()
{
	if (IsDead() ||
		!HasHeldItemAction(
			EDRItemActionType::Dig) ||
		!IsValid(
			ItemActionPresentationComponent))
	{
		return;
	}

	ItemActionPresentationComponent->PlayWorldActionFromServer(EDRItemActionType::Dig);
}

void ADRPlayerCharacter::PlayLocalCameraShake(
	TSubclassOf<UCameraShakeBase> ShakeClass,
	float Scale)
{
	if (!IsLocallyControlled() ||
		!ShakeClass)
	{
		return;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(GetController());

	if (!IsValid(PlayerController) ||
		!IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}

	PlayerController->PlayerCameraManager->StartCameraShake(
		ShakeClass,
		Scale,
		ECameraShakePlaySpace::CameraLocal,
		FRotator::ZeroRotator);
}

void ADRPlayerCharacter::ClientPlayDamagedCameraShake_Implementation()
{
	if (IsValid(
			ItemActionPresentationComponent))
	{
		ItemActionPresentationComponent->
			PlayDamagedFeedbackLocal();
	}
}

void ADRPlayerCharacter::SetHeldItemDefinition(
	UDRItemDefinition* NewItemDefinition)
{
	if (!HasAuthority() ||
		HeldItemDefinition == NewItemDefinition)
	{
		return;
	}

	HeldItemDefinition = NewItemDefinition;

	RefreshHeldItemVisual();
	RefreshHeldItemMiningSettings();

	if (IsLocallyControlled() &&
		IsValid(HeldItemDefinition) &&
		IsValid(EquipSound))
	{
		UGameplayStatics::PlaySound2D(
			this,
			EquipSound);
	}

	ForceNetUpdate();
}

void ADRPlayerCharacter::OnRep_HeldItemDefinition()
{
	RefreshHeldItemVisual();
	RefreshHeldItemMiningSettings();

	if (IsLocallyControlled() &&
		IsValid(HeldItemDefinition) &&
		IsValid(EquipSound))
	{
		UGameplayStatics::PlaySound2D(
			this,
			EquipSound);
	}
}

void ADRPlayerCharacter::RefreshHeldItemVisual()
{
	if (!IsValid(HeldItemDefinition))
	{
		ClearHandEquipmentVisual();
		return;
	}
	
	UStaticMesh* VisualMesh = HeldItemDefinition->WorldMesh;
	FTransform FirstPersonVisualTransform = HeldItemDefinition->SpawnOffsetTransform 
		* HeldItemDefinition->FirstPersonVisualOffsetTransform;
	
	// 당장은 특별한 처리 없이 기본 크기 적용.
	FTransform ThirdPersonVisualTransform = HeldItemDefinition->SpawnOffsetTransform;
	
	ApplyHandEquipmentVisual(VisualMesh, VisualMesh
		, FirstPersonVisualTransform, ThirdPersonVisualTransform);	
}

void ADRPlayerCharacter::RefreshHeldItemMiningSettings()
{
	if (IsValid(MiningComponent))
	{
		MiningComponent->ApplyItemDefinition(HeldItemDefinition);
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

	UCharacterMovementComponent* Movement =
		GetCharacterMovement();

	if (!IsValid(Movement))
	{
		return;
	}

	if (Movement->IsMovingOnGround())
	{
		Jump();
		return;
	}

	if (Movement->IsFalling() &&
		IsValid(JetpackComponent))
	{
		JetpackComponent->RequestStart();
	}
}

void ADRPlayerCharacter::HandleJumpReleased()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	StopJumping();

	if (IsValid(JetpackComponent))
	{
		JetpackComponent->RequestStop();
	}
}