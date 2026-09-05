#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SceneComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRVoxelContainmentComponent.h"
#include "VoxelComponents/VoxelNoClippingComponent.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"
#include "DeepRaiders/Player/Components/DRJetpackComponent.h"
#include "DeepRaiders/Player/Components/DRPlayerLifecycleComponent.h"
#include "DeepRaiders/Player/Components/DRHeldItemComponent.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"
#include "DeepRaiders/Player/Components/DRFreezeVisualComponent.h"
#include "DeepRaiders/Player/Components/DRSilhouetteComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DeepRaiders/Player/Components/DRPlayerCameraComponent.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/DRWeaponPresentationTypes.h"
#include "DeepRaiders/Player/Components/DRCharacterShadowComponent.h"
#include "Animation/AnimInstance.h"
#include "DeepRaiders/Item/Animation/DRHitReactionSet.h"
#include "DeepRaiders/UI/Nameplate/DRPlayerNameplateComponent.h"

ADRPlayerCharacter::ADRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 이 Actor가 서버에서 클라이언트로 복제되도록 설정
	bReplicates = true;

	// Actor 이동 정보도 복제
	SetReplicateMovement(true);

	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetHiddenInGame(false);
	GetMesh()->SetVisibility(true);

	VoxelNoClippingComponent = CreateDefaultSubobject<UVoxelNoClippingComponent>(TEXT("VoxelNoClippingComponent"));
	VoxelNoClippingComponent->SetupAttachment(GetCapsuleComponent());
	VoxelNoClippingComponent->TickRate = 0.03f;
	VoxelNoClippingComponent->SearchRange = 8;
	// 플러그인의 기본 텔레포트/Impulse가 지형 충돌 보정과 경쟁하지 않게 한다.
	VoxelNoClippingComponent->bEnableDefaultBehavior = false;
	VoxelNoClippingComponent->Speed = 6000.f;
	VoxelContainmentComponent = CreateDefaultSubobject<UDRVoxelContainmentComponent>(TEXT("VoxelContainmentComponent"));

	TeleportComponent = CreateDefaultSubobject<UDRTeleportComponent>(TEXT("TeleportComponent"));
	MeleeCombatComponent = CreateDefaultSubobject<UDRMeleeCombatComponent>(TEXT("MeleeCombatComponent"));
	JetpackComponent = CreateDefaultSubobject<UDRJetpackComponent>(TEXT("JetpackComponent"));
	PlayerLifecycleComponent = CreateDefaultSubobject<UDRPlayerLifecycleComponent>(TEXT("PlayerLifecycleComponent"));
	HeldItemComponent = CreateDefaultSubobject<UDRHeldItemComponent>(TEXT("HeldItemComponent"));
	FreezeVisualComponent = CreateDefaultSubobject<UDRFreezeVisualComponent>(TEXT("FreezeVisualComponent"));
	SilhouetteComponent = CreateDefaultSubobject<UDRSilhouetteComponent>(TEXT("SilhouetteComponent"));
	MovementActionComponent = CreateDefaultSubobject<UDRMovementActionComponent>(TEXT("MovementActionComponent"));
	PlayerNameplateComponent = CreateDefaultSubobject<UDRPlayerNameplateComponent>(TEXT("PlayerNameplateComponent"));

	CharacterShadowComponent = CreateDefaultSubobject<UDRCharacterShadowComponent>(TEXT("CharacterShadowComponent"));
	CharacterShadowComponent->SetupAttachment(GetCapsuleComponent());
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	CameraBoom->SocketOffset = FVector(0.f, 65.f, 20.f);
	CameraBoom->bUsePawnControlRotation = true;
	// 지형 충돌과 거리 복구는 UDRPlayerCameraComponent가 일관되게 처리한다.
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 90.f;

	PlayerCameraComponent = CreateDefaultSubobject<UDRPlayerCameraComponent>(TEXT("PlayerCameraComponent"));

	// 월드 손 장비
	WorldHandEquipmentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldHandEquipmentMesh"));
	WorldHandEquipmentMesh->SetupAttachment(GetMesh(), TEXT("S_HandGrip_R"));
	WorldHandEquipmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldHandEquipmentMesh->SetGenerateOverlapEvents(false);

	GameplayFireAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("GameplayFireAnchor"));
	GameplayFireAnchor->SetupAttachment(GetCapsuleComponent());
	GameplayFireAnchor->SetRelativeLocation(FVector(0.f, 10.f, 55.f));

	SkillFireAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("SkillFireAnchor"));
	SkillFireAnchor->SetupAttachment(GetCapsuleComponent());
	SkillFireAnchor->SetRelativeLocation(FVector(100.f, 10.f, 55.f));
	
	// 등 뒤에 달릴 장비 - 제트팩
	WorldBackEquipmentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldBackEquipmentMesh"));
	WorldBackEquipmentMesh->SetupAttachment(GetMesh(), TEXT("S_Back"));
	WorldBackEquipmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldBackEquipmentMesh->SetGenerateOverlapEvents(false);

	// 프로토타입에서는 자기 카메라에 제트팩이 끼어들지 않게 숨기는 편이 안전
	WorldBackEquipmentMesh->SetOwnerNoSee(false);
	WorldBackEquipmentMesh->SetCastHiddenShadow(true);
	WorldBackEquipmentMesh->SetIsReplicated(false);
	
	PlayerNameplateComponent->SetupAttachment(GetCapsuleComponent());
}

UAbilitySystemComponent* ADRPlayerCharacter::GetAbilitySystemComponent() const
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return nullptr;
	}

	return DRPlayerState->GetAbilitySystemComponent();
}

float ADRPlayerCharacter::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!HasAuthority() ||
		DamageAmount <= 0.f ||
		IsDead() ||
		!DamageEffectClass)
	{
		return 0.f;
	}

	UAbilitySystemComponent* ASC =
		GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return 0.f;
	}

	const float HealthBefore =
		GetCurrentHealth();

	FGameplayEffectContextHandle Context =
		ASC->MakeEffectContext();

	Context.AddInstigator(
		EventInstigator,
		DamageCauser);

	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(
			DamageEffectClass,
			1.f,
			Context);

	if (!SpecHandle.IsValid())
	{
		return 0.f;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Damage,
		DamageAmount);

	ASC->ApplyGameplayEffectSpecToSelf(
		*SpecHandle.Data.Get());

	return FMath::Max(
		0.f,
		HealthBefore - GetCurrentHealth());
}

float ADRPlayerCharacter::GetCurrentHealth() const
{
	const UDRPlayerAttributeSet* Attributes = GetPlayerAttributeSet();

	return IsValid(Attributes) ? Attributes->GetHealth() : 0.f;
}

float ADRPlayerCharacter::GetHealthRatio() const
{
	const UDRPlayerAttributeSet* Attributes = GetPlayerAttributeSet();

	if (!IsValid(Attributes) || Attributes->GetMaxHealth() <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Clamp(Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.f, 1.f);
}

bool ADRPlayerCharacter::IsDead() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(DRGameplayTags::State_Dead);
}

float ADRPlayerCharacter::GetMaxHealth() const
{
	const UDRPlayerAttributeSet* Attributes = GetPlayerAttributeSet();

	if (!IsValid(Attributes) || Attributes->GetMaxHealth() <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return Attributes->GetMaxHealth();
}

void ADRPlayerCharacter::Landed(const FHitResult& Hit)
{
	const float LandingSpeed = FMath::Max(0.f, -GetVelocity().Z);

	Super::Landed(Hit);

	if (HasAuthority())
	{
		LastLandedLocation = GetActorLocation();
	}

	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->HandleLanded(LandingSpeed);
	}
}

void ADRPlayerCharacter::HandleJumpPressed()
{
	if (!IsLocallyControlled() || IsDead() || IsFrozen())
	{
		return;
	}

	if (IsValid(MovementActionComponent)
		&& MovementActionComponent->GetSimulationActionState().ActionType == EDRMovementActionType::Zipline)
	{
		MovementActionComponent->RequestCancelZipline();
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

void ADRPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	/*
	 * ASC가 PlayerState에 있으므로
	 * 이전 Pawn의 Dead/Frozen/Attribute 상태를
	 * 새 Avatar와 연결하기 전에 먼저 정리한다.
	 */
	if (HasAuthority())
	{
		ApplySpawnAttributeReset();
	}

	/*
	 * 깨끗한 ASC 상태가 된 후
	 * 새 Character를 Avatar로 연결한다.
	 */
	InitializeAbilitySystem();
	RefreshTeamColor();
	SilhouetteComponent->RefreshTeamSilhouette();

	UE_LOG(LogTemp, Warning, TEXT( "[GAS][PossessedBy] " "Character=%s " "Authority=%d " "Local=%d " "LocalRole=%d " "PlayerState=%s " "ASC=%s"), 
		*GetNameSafe(this), HasAuthority(), IsLocallyControlled(), static_cast<int32>(GetLocalRole()), *GetNameSafe(GetPlayerState()), *GetNameSafe( GetAbilitySystemComponent()));

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
	RefreshTeamColor();
	SilhouetteComponent->RefreshTeamSilhouette();

	UE_LOG(LogTemp, Warning, TEXT( "[GAS][OnRep_PlayerState] " "Character=%s " "Authority=%d " "Local=%d " "LocalRole=%d " "PlayerState=%s " "ASC=%s"), *GetNameSafe(this), HasAuthority(), IsLocallyControlled(), static_cast<int32>(GetLocalRole()), *GetNameSafe(GetPlayerState()), *GetNameSafe(GetAbilitySystemComponent()));
}

void ADRPlayerCharacter::RefreshTeamColor()
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	if (!IsValid(DRPlayerState) || !IsValid(GetMesh()) || !DRPlayerState->HasAssignedTeam())
	{
		return;
	}

	const int32 TeamId = DRPlayerState->GetTeamId();
	if (TeamId != 0 && TeamId != 1)
	{
		return;
	}
	
	UMaterialInterface* TeamMaterial = TeamId == 0 ? Team0Material.Get() : Team1Material.Get();
	if (!IsValid(TeamMaterial)
		|| TeamMaterialSlotIndex < 0 || TeamMaterialSlotIndex >= GetMesh()->GetNumMaterials())
	{
		return;
	}

	// 팀 ID 복제 후 각 클라이언트에서 같은 MI를 적용한다.
	GetMesh()->SetMaterial(TeamMaterialSlotIndex, TeamMaterial);
	
	// 팀 ID에 맞는 무기 메테리얼 사용
	RefreshHeldWeaponTeamMaterial();
}

void ADRPlayerCharacter::ApplyHandEquipmentVisual(UStaticMesh* WorldMesh, FName AttachSocketName, FTransform WorldItemOffset)
{
	if (!IsValid(WorldHandEquipmentMesh) || !IsValid(GetMesh()))
	{
		return;
	}

	if (AttachSocketName.IsNone())
	{
		AttachSocketName = TEXT("S_HandGrip_R");
	}

	WorldHandEquipmentMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);

	// 이전 무기의 팀별 Material Override 제거
	WorldHandEquipmentMesh->EmptyOverrideMaterials();

	WorldHandEquipmentMesh->SetStaticMesh(WorldMesh);
	WorldHandEquipmentMesh->SetRelativeTransform(WorldItemOffset);
	WorldHandEquipmentMesh->SetVisibility(IsValid(WorldMesh), true);

	// 현재 아이템이 팀 Material을 지원하는 무기면 다시 적용
	RefreshHeldWeaponTeamMaterial();
}

void ADRPlayerCharacter::ClearHandEquipmentVisual()
{
	if (!IsValid(WorldHandEquipmentMesh))
	{
		return;
	}

	WorldHandEquipmentMesh->EmptyOverrideMaterials();
	WorldHandEquipmentMesh->SetStaticMesh(nullptr);
	WorldHandEquipmentMesh->SetVisibility(false, true);
}

void ADRPlayerCharacter::ApplyBackEquipmentVisual(UStaticMesh* BackMesh, const FTransform& BackTransform)
{
	WorldBackEquipmentMesh->SetStaticMesh(BackMesh);
	WorldBackEquipmentMesh->SetRelativeTransform(BackTransform);

	WorldBackEquipmentMesh->SetVisibility(IsValid(BackMesh), true);
}

void ADRPlayerCharacter::ClearBackEquipmentVisual()
{
	WorldBackEquipmentMesh->SetStaticMesh(nullptr);
	WorldBackEquipmentMesh->SetVisibility(false, true);
}

void ADRPlayerCharacter::SetLocalFirstPersonVisualsHidden(bool bHideForFirstPerson)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (IsValid(GetMesh()))
	{
		GetMesh()->SetOwnerNoSee(bHideForFirstPerson);
	}

	if (IsValid(WorldBackEquipmentMesh))
	{
		WorldBackEquipmentMesh->SetOwnerNoSee(bHideForFirstPerson);
	}

	// 1인칭에서도 현재 손 장비는 조준 피드백을 위해 계속 표시한다.
	if (IsValid(WorldHandEquipmentMesh))
	{
		WorldHandEquipmentMesh->SetOwnerNoSee(false);
	}
}

void ADRPlayerCharacter::RefreshJetpackVisual()
{
	if (IsValid(JetpackComponent))
	{
		JetpackComponent->RefreshVisual();
	}
}

void ADRPlayerCharacter::MoveInput(const FVector2D& MoveInput)
{
	const bool bMovementInputChanged =
		!LatestMovementInput.Equals(MoveInput);

	LatestMovementInput = MoveInput;

	if (!HasAuthority()
		&& bMovementInputChanged)
	{
		/*
		 * GAS Ability는 PlayerState에서 실행되므로,
		 * 별도 Actor channel인 입력 RPC와 발동 RPC 사이의 순서를 전제하지 않는다.
		 * 대시는 TargetData를 우선 사용하고,
		 * 이 값은 Blink/Roll 등 서버 실행 경로의 입력 상태로 사용한다.
		 */
		ServerSetLatestMovementInput(MoveInput);
	}

	if (!Controller || IsDead() || IsFrozen())
	{
		return;
	}

	if (IsValid(MovementActionComponent))
	{
		const FDRMovementActionState& ActionState = MovementActionComponent->GetSimulationActionState();

		if (ActionState.ActionType == EDRMovementActionType::Zipline
			&& ActionState.ZiplineRideMode == EDRZiplineRideMode::ManualTraverse)
		{
			// ManualTraverse는 A/D를 무시하고 Rope 축 방향 W/S만 사용한다.
			FVector PositiveAxis =
				ActionState.GetZiplineManualPositiveAxis();

			if (PositiveAxis.IsNearlyZero())
			{
				return;
			}

			if (ActionState.ZiplineManualControlMode
				== EDRZiplineManualControlMode::ViewRelative)
			{
				const FVector ViewForward =
					Controller->GetControlRotation()
					.Vector()
					.GetSafeNormal();

				float ViewDot =
					FVector::DotProduct(
						ViewForward,
						PositiveAxis);

				/*
				 * Rope를 거의 정측면으로 보는 순간에는 작은 카메라 흔들림으로
				 * W 방향이 매 프레임 뒤집히지 않도록 Character Forward를 fallback으로 사용한다.
				 */
				if (FMath::Abs(ViewDot) < 0.1f)
				{
					ViewDot =
						FVector::DotProduct(
							GetActorForwardVector(),
							PositiveAxis);
				}

				if (ViewDot < 0.f)
				{
					PositiveAxis *= -1.f;
				}
			}

			AddMovementInput(
				PositiveAxis,
				MoveInput.Y);

			return;
		}
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(ForwardDirection, MoveInput.Y);
	AddMovementInput(RightDirection, MoveInput.X);
}

FVector ADRPlayerCharacter::GetSkillMovementDirection() const
{
	if (LatestMovementInput.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const AController* CurrentController =
		GetController();

	if (CurrentController == nullptr)
	{
		return FVector::ZeroVector;
	}

	const FRotator YawRotation(
		0.f,
		CurrentController->GetControlRotation().Yaw,
		0.f);

	const FVector ForwardDirection =
		FRotationMatrix(YawRotation)
		.GetUnitAxis(EAxis::X);

	const FVector RightDirection =
		FRotationMatrix(YawRotation)
		.GetUnitAxis(EAxis::Y);

	return (
		ForwardDirection * LatestMovementInput.Y
		+ RightDirection * LatestMovementInput.X
	)
	.GetSafeNormal2D();
}

void ADRPlayerCharacter::ServerSetLatestMovementInput_Implementation(
	const FVector2D InMovementInput)
{
	LatestMovementInput = InMovementInput;
}

void ADRPlayerCharacter::LookInput(const FVector2D& LookInput)
{
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

float ADRPlayerCharacter::GetDisplayedJetpackFuelRatio() const
{
	return IsValid(JetpackComponent) ? JetpackComponent->GetDisplayedFuelRatio() : 0.f;
}

void ADRPlayerCharacter::ReconcileJetpackFuelFromServer(float ServerFuel)
{
	if (IsValid(JetpackComponent))
	{
		JetpackComponent->ReconcileFuelFromServer(ServerFuel);
	}
}

bool ADRPlayerCharacter::IsFrozen() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	return IsValid(ASC) && ASC->HasMatchingGameplayTag(DRGameplayTags::State_Frozen);
}

UDRItemAnimationSet* ADRPlayerCharacter::GetCurrentItemAnimationSet() const
{
	if (!IsValid(HeldItemComponent))
	{
		return nullptr;
	}

	const UDRItemDefinition* ItemDefinition = HeldItemComponent->GetHeldItemDefinition();

	return IsValid(ItemDefinition) ? ItemDefinition->ItemAnimationSet : nullptr;
}

float ADRPlayerCharacter::GetAimPitchDegrees() const
{
	const FRotator BaseAimRotation = GetBaseAimRotation();
	const FRotator ActorRotation = GetActorRotation();

	const FRotator DeltaRotation =
		(BaseAimRotation - ActorRotation).GetNormalized();

	return DeltaRotation.Pitch;
}

bool ADRPlayerCharacter::CalculateGameplayFireOrigin(const FVector& AimDirection, FVector& OutFireOrigin) const
{
	if (!IsValid(GameplayFireAnchor))
	{
		return false;
	}

	const FVector SafeAimDirection = AimDirection.GetSafeNormal();

	if (SafeAimDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector AnchorLocation = GameplayFireAnchor->GetComponentLocation();
	OutFireOrigin = AnchorLocation + SafeAimDirection * GameplayFireForwardDistance;

	return !OutFireOrigin.ContainsNaN();
}

void ADRPlayerCharacter::MulticastStartSharedSearchReveal_Implementation(
	int32 SourceTeamId,
	int32 SourcePlayerId,
	FGuid RevealId,
	float Duration,
	int32 StencilValue)
{
	const APlayerController* LocalController = GetWorld()->GetFirstPlayerController();
	const ADRPlayerState* LocalPlayerState = IsValid(LocalController)
		? LocalController->GetPlayerState<ADRPlayerState>()
		: nullptr;
	if (!IsValid(LocalPlayerState)
		|| LocalPlayerState->GetPlayerId() == SourcePlayerId
		|| LocalPlayerState->GetTeamId() != SourceTeamId
		|| !IsValid(SilhouetteComponent))
	{
		return;
	}

	SilhouetteComponent->StartSearchReveal(RevealId, Duration, StencilValue);
}

void ADRPlayerCharacter::MulticastStopSharedSearchReveal_Implementation(FGuid RevealId)
{
	if (IsValid(SilhouetteComponent))
	{
		SilhouetteComponent->StopSearchReveal(RevealId);
	}
}

bool ADRPlayerCharacter::CalculateSkillFireOrigin(FVector& OutFireOrigin) const
{
	if (!IsValid(SkillFireAnchor))
	{
		return false;
	}

	OutFireOrigin = SkillFireAnchor->GetComponentLocation();

	return !OutFireOrigin.ContainsNaN();
}

void ADRPlayerCharacter::PlayProjectileFireVFXFromNotify()
{
	if (GetNetMode() == NM_DedicatedServer
		|| !IsValid(HeldItemComponent)
		|| !IsValid(WorldHandEquipmentMesh))
	{
		return;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		Cast<UDRProjectileWeaponItemDefinition>(
			HeldItemComponent->GetHeldItemDefinition());

	if (!IsValid(WeaponDefinition))
	{
		return;
	}

	const FDRWeaponPresentationData& Presentation =
		WeaponDefinition->FirePresentation;

	if (!IsValid(Presentation.VFX))
	{
		return;
	}

	const FName SocketName =
		Presentation.AttachSocketName.IsNone()
			? TEXT("VFXPoint")
			: Presentation.AttachSocketName;

	if (!WorldHandEquipmentMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[WeaponFireVFX] Socket missing. Weapon=%s Socket=%s"),
			*GetNameSafe(WeaponDefinition),
			*SocketName.ToString());

		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAttached(
		Presentation.VFX,
		WorldHandEquipmentMesh,
		SocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}

void ADRPlayerCharacter::PlayHitReaction(
	const FVector& ImpactLocation)
{
	if (GetNetMode() == NM_DedicatedServer
		|| !IsValid(GetMesh())
		|| IsDead())
	{
		return;
	}

	const UDRItemAnimationSet* AnimationSet =
		GetCurrentItemAnimationSet();

	if (!IsValid(AnimationSet)
		|| !IsValid(AnimationSet->HitReactionSet))
	{
		return;
	}

	const UDRHitReactionSet* HitReactionSet =
		AnimationSet->HitReactionSet;

	UWorld* World = GetWorld();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!IsValid(World)
		|| !IsValid(AnimInstance))
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	if (CurrentTime - LastHitReactionTime
		< HitReactionSet->MinReplayInterval)
	{
		return;
	}

	FVector ToImpact =
		ImpactLocation - GetActorLocation();

	ToImpact.Z = 0.f;
	ToImpact = ToImpact.GetSafeNormal();

	if (ToImpact.IsNearlyZero())
	{
		return;
	}

	const float ForwardDot =
		FVector::DotProduct(
			GetActorForwardVector(),
			ToImpact);

	const float RightDot =
		FVector::DotProduct(
			GetActorRightVector(),
			ToImpact);

	UAnimSequenceBase* HitAnimation = nullptr;

	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		HitAnimation =
			ForwardDot >= 0.f
				? HitReactionSet->Front
				: HitReactionSet->Back;
	}
	else
	{
		HitAnimation =
			RightDot >= 0.f
				? HitReactionSet->Right
				: HitReactionSet->Left;
	}

	if (!IsValid(HitAnimation))
	{
		return;
	}

	LastHitReactionTime = CurrentTime;

	AnimInstance->PlaySlotAnimationAsDynamicMontage(
		HitAnimation,
		HitReactionSet->SlotName,
		HitReactionSet->BlendInTime,
		HitReactionSet->BlendOutTime,
		1.f,
		1);
}

FLinearColor ADRPlayerCharacter::GetTeamDisplayColor() const
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return FLinearColor::White;
	}

	switch (DRPlayerState->GetTeamId())
	{
	case 0:
		return Team0Color;

	case 1:
		return Team1Color;

	default:
		return FLinearColor::White;
	}
}

void ADRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint component template에 예전 값이 남아 있어도 기본 텔레포트/Impulse는 실행하지 않는다.
	if (IsValid(VoxelNoClippingComponent))
	{
		VoxelNoClippingComponent->bEnableDefaultBehavior = false;
	}

	if (IsValid(PlayerCameraComponent))
	{
		PlayerCameraComponent->ConfigureCamera(
			CameraBoom,
			FollowCamera,
			Cast<UDRCharacterMovementComponent>(GetCharacterMovement()));
	}

	if (HasAuthority())
	{
		// 첫 착지 전에는 스폰 위치를 안전한 반환점으로 사용한다.
		LastLandedLocation = GetActorLocation();
	}
}

void ADRPlayerCharacter::InitializeAbilitySystem()
{
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return;
	}

	UAbilitySystemComponent* ASC = DRPlayerState->GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return;
	}

	ASC->InitAbilityActorInfo(DRPlayerState, this);

	if (IsValid(PlayerLifecycleComponent))
	{
		PlayerLifecycleComponent->BindAbilitySystem(ASC);
	}

	if (UDRCharacterMovementComponent* MovementComponent =
		Cast<UDRCharacterMovementComponent>(GetCharacterMovement()))
	{
		MovementComponent->BindAbilitySystem(ASC);
	}

	if (IsValid(VoxelContainmentComponent))
	{
		VoxelContainmentComponent->BindAbilitySystem(ASC);
	}
	
	if (IsValid(CharacterShadowComponent))
	{
		CharacterShadowComponent->BindAbilitySystem(ASC);
	}

	const UDRPlayerAttributeSet* RegisteredAttributeSet = ASC->GetSet<UDRPlayerAttributeSet>();
	if (!IsValid(RegisteredAttributeSet))
	{
		return;
	}

	if (IsValid(FreezeVisualComponent))
	{
		FreezeVisualComponent->BindAbilitySystem(ASC);
	}
	
	UE_LOG(LogTemp, Warning, TEXT( "[GAS][AttributeSet] " "Direct=%s Registered=%s Same=%d"), 
		*GetNameSafe(DRPlayerState->GetPlayerAttributeSet()), *GetNameSafe(RegisteredAttributeSet), DRPlayerState->GetPlayerAttributeSet() == RegisteredAttributeSet);

	/*
	 * 이 Character에서 PlayerState / ASC /
	 * AttributeSet을 사용할 준비가 완료된 시점.
	 */
	if (!bAbilitySystemReady || ReadyAbilitySystemComponent.Get() != ASC)
	{
		ReadyAbilitySystemComponent = ASC;
		bAbilitySystemReady = true;

		OnAbilitySystemReady.Broadcast(ASC);
	}
}

const UDRPlayerAttributeSet* ADRPlayerCharacter::GetPlayerAttributeSet() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	return IsValid(ASC) ? ASC->GetSet<UDRPlayerAttributeSet>() : nullptr;
}

void ADRPlayerCharacter::ApplySpawnAttributeReset()
{
	if (!HasAuthority())
	{
		return;
	}

	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return;
	}

	DRPlayerState->ResetForRespawn();
}

void ADRPlayerCharacter::RefreshHeldWeaponTeamMaterial()
{
	if (!IsValid(WorldHandEquipmentMesh) || !IsValid(HeldItemComponent) || !IsValid(WorldHandEquipmentMesh->GetStaticMesh()))
	{
		return;
	}

	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState) || !DRPlayerState->HasAssignedTeam())
	{
		return;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = Cast<UDRProjectileWeaponItemDefinition>(HeldItemComponent->GetHeldItemDefinition());

	if (!IsValid(WeaponDefinition))
	{
		return;
	}

	const int32 TeamId = DRPlayerState->GetTeamId();

	UMaterialInterface* TeamMaterial = nullptr;

	switch (TeamId)
	{
	case 0:
		TeamMaterial = WeaponDefinition->Team0Material;
		break;

	case 1:
		TeamMaterial = WeaponDefinition->Team1Material;
		break;

	default:
		return;
	}

	if (!IsValid(TeamMaterial))
	{
		return;
	}

	const int32 MaterialIndex = WeaponDefinition->TeamMaterialSlotIndex;

	if (MaterialIndex < 0 || MaterialIndex >= WorldHandEquipmentMesh->GetNumMaterials())
	{
		return;
	}

	WorldHandEquipmentMesh->SetMaterial(MaterialIndex, TeamMaterial);
}

void ADRPlayerCharacter::SetHeldItemDefinition(UDRItemDefinition* NewItemDefinition)
{
	if (IsValid(HeldItemComponent))
	{
		HeldItemComponent->SetHeldItemDefinition(NewItemDefinition);
	}
}
