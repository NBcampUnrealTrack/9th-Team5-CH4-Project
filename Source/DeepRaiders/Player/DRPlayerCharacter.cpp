#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DRPlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"

#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Components/AudioComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName FirstPersonSwingTrackName(
		TEXT("FirstPersonSwing"));
}

ADRPlayerCharacter::ADRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 이 Actor가 서버에서 클라이언트로 복제되도록 설정
	bReplicates = true;
	
	MiningComponent =
		CreateDefaultSubobject<UDRMiningComponent>(
			TEXT("MiningComponent"));

	TeleportComponent = CreateDefaultSubobject<UDRTeleportComponent>(TEXT("TeleportComponent"));

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
	
	FirstPersonItemSwingTimeline =
		CreateDefaultSubobject<UTimelineComponent>(
			TEXT("FirstPersonItemSwingTimeline"));
}

void ADRPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	/*
	 * 제트팩 이동은 CharacterMovementComponent가
	 * 소유 클라이언트와 서버에서 예측 처리한다.
	 *
	 * Character Tick은 서버의 연료 소비에만 사용한다.
	 */
	if (!HasAuthority() ||
		!bIsJetpackActive)
	{
		return;
	}

	UpdateJetpackFuel(DeltaSeconds);
}

UDRCharacterMovementComponent* ADRPlayerCharacter::GetDRCharacterMovementComponent() const
{
	return Cast<UDRCharacterMovementComponent>(GetCharacterMovement());
}

void ADRPlayerCharacter::Landed(const FHitResult& Hit)
{
	/*
	 * 착지 처리 이후에는 CharacterMovement의 수직 속도가
	 * 바뀔 수 있으므로 Super 호출 전에 저장한다.
	 *
	 * 하강 속도는 음수이므로 부호를 반대로 바꿔
	 * 양수 형태의 LandingSpeed로 사용한다.
	 */
	const float LandingSpeed =
		FMath::Max(
			0.f,
			-GetVelocity().Z);

	Super::Landed(Hit);

	if (IsLocallyControlled())
	{
		UDRCharacterMovementComponent* Movement =
			GetDRCharacterMovementComponent();

		if (IsValid(Movement))
		{
			Movement->SetWantsJetpack(false);
		}

		RefreshJetpackActivePresentation();
	}
	
	/*
	 * 낙하 피해와 제트팩 연료는 서버에서만 처리한다.
	 */
	if (!HasAuthority())
	{
		return;
	}

	StopJetpackFromServer();

	const float CalculatedFallDamage =
		CalculateFallDamage(LandingSpeed);

	ApplyFallDamage(LandingSpeed);

	ClientPlayFallSound(
		CalculatedFallDamage > KINDA_SMALL_NUMBER,
		IsDead());

	/*
	 * 낙하 피해로 사망했다면
	 * 사망 처리 중인 Pawn의 연료를 충전하지 않는다.
	 */
	if (IsDead())
	{
		return;
	}

	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (IsValid(DRPlayerState))
	{
		DRPlayerState->RefillJetpackFuel();
	}
}

float ADRPlayerCharacter::CalculateFallDamage(float LandingSpeed) const
{
	if (LandingSpeed <= MinFallDamageSpeed ||
		MaxHealth <= 0.f)
	{
		return 0.f;
	}

	/*
	 * 잘못된 설정으로 0 나누기가 발생하지 않도록 방지한다.
	 */
	if (MaxFallDamageSpeed <= MinFallDamageSpeed + KINDA_SMALL_NUMBER)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[FallDamage] Invalid speed range. "
				"Character=%s MinSpeed=%.1f MaxSpeed=%.1f"),
			*GetName(),
			MinFallDamageSpeed,
			MaxFallDamageSpeed);

		return 0.f;
	}

	/*
	 * MinFallDamageSpeed부터 MaxFallDamageSpeed까지를
	 * 0~1 범위로 정규화한다.
	 */
	const float NormalizedSpeed =
		FMath::Clamp(
			(LandingSpeed - MinFallDamageSpeed) / (MaxFallDamageSpeed - MinFallDamageSpeed),
			0.f,
			1.f);

	/*
	 * 기본값이 2이므로 제곱 곡선이 적용된다.
	 *
	 * 0.25 -> 0.0625
	 * 0.50 -> 0.25
	 * 0.75 -> 0.5625
	 * 1.00 -> 1.0
	 */
	const float DamageAlpha =
		FMath::Pow(
			NormalizedSpeed,
			FMath::Max(FallDamageExponent,0.01f));

	const float MaximumFallDamage =
		MaxHealth *
		FMath::Clamp(MaxFallDamageRatio, 0.f, 1.f);

	return MaximumFallDamage * DamageAlpha;
}

void ADRPlayerCharacter::ApplyFallDamage(
	float LandingSpeed)
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	const float CalculatedDamage = CalculateFallDamage(LandingSpeed);

	/*
	 * 테스트 중 모든 착지 속도를 확인할 수 있도록
	 * 피해 여부와 관계없이 로그를 남긴다.
	 */
	UE_LOG(
		LogTemp,
		Log,
		TEXT(
			"[FallDamage] Character=%s "
			"LandingSpeed=%.1f "
			"CalculatedDamage=%.1f"),
		*GetName(),
		LandingSpeed,
		CalculatedDamage);

	if (CalculatedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float HealthBeforeDamage = CurrentHealth;

	/*
	 * 기존 공격 피해와 동일한 TakeDamage 경로를 사용한다.
	 * 실제 체력 감소와 사망 판정은 TakeDamage가 담당한다.
	 */
	const float AppliedDamage =
		UGameplayStatics::ApplyDamage(
			this,
			CalculatedDamage,
			GetController(),
			this,
			UDamageType::StaticClass());

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[FallDamage] Applied Character=%s "
			"LandingSpeed=%.1f "
			"Damage=%.1f "
			"Health=%.1f->%.1f"),
		*GetName(),
		LandingSpeed,
		AppliedDamage,
		HealthBeforeDamage,
		CurrentHealth);
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
	if (!IsLocallyControlled() || IsDead() || !HasHeldItemAction(EDRItemActionType::MeleeAttack))
	{
		return;
	}
	
	// // 현재는 1인칭 공격 애니메이션이 없으므로 임시 표현만 실행한다.
	// PlayOwnerMeleeAttackPresentation();

	// 실제 공격 승인과 판정은 서버가 담당한다.
	ServerRequestMeleeAttack();
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

float ADRPlayerCharacter::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!HasAuthority() ||
		DamageAmount <= 0.f ||
		IsDead())
	{
		return 0.f;
	}

	const float AppliedDamage =
		FMath::Min(DamageAmount, CurrentHealth);

	CurrentHealth = FMath::Clamp(
		CurrentHealth - AppliedDamage,
		0.f,
		MaxHealth);
	
	if (IsDead())
	{
		HandleDeath();
	}

	ForceNetUpdate();

	return AppliedDamage;
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
	if (!HasAuthority())
	{
		return;
	}

	MulticastPlayWorldItemActionPresentation(
		EDRItemActionType::Dig);
}

void ADRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	PrintNetworkState(TEXT("BeginPlay"));

	// 장비 Root의 기본 위치 기억
	if (IsValid(FirstPersonEquipmentRoot))
	{
		FirstPersonEquipmentRootBaseTransform =
			FirstPersonEquipmentRoot->GetRelativeTransform();
	}

	UCurveFloat* InitialSwingCurve = nullptr;

	if (IsValid(FirstPersonDigPresentation.Curve))
	{
		InitialSwingCurve =
			FirstPersonDigPresentation.Curve;
	}
	else if (IsValid(FirstPersonMeleePresentation.Curve))
	{
		InitialSwingCurve =
			FirstPersonMeleePresentation.Curve;
	}

	if (IsValid(FirstPersonItemSwingTimeline) &&
		IsValid(InitialSwingCurve))
	{
		FOnTimelineFloat UpdateDelegate;

		UpdateDelegate.BindUFunction(
			this,
			FName("UpdateFirstPersonItemSwing"));

		FirstPersonItemSwingTimeline->AddInterpFloat(
			InitialSwingCurve,
			UpdateDelegate,
			NAME_None,
			FirstPersonSwingTrackName);

		FOnTimelineEvent FinishedDelegate;

		FinishedDelegate.BindUFunction(
			this,
			FName("FinishFirstPersonItemSwing"));

		FirstPersonItemSwingTimeline->SetTimelineFinishedFunc(
			FinishedDelegate);

		FirstPersonItemSwingTimeline->SetLooping(false);

		FirstPersonItemSwingTimeline->SetTimelineLengthMode(
			TL_LastKeyFrame);
	}
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

	RestoreControllerInput();

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

	RestoreControllerInput();

	PrintNetworkState(TEXT("OnRep_Controller"));
}

void ADRPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	RefreshJetpackVisual();
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
		bIsJetpackActive);
	
	DOREPLIFETIME(
		ADRPlayerCharacter,
		CurrentHealth);
	
	DOREPLIFETIME(
		ADRPlayerCharacter,
		HeldItemDefinition);
}

void ADRPlayerCharacter::ClientRejectJetpack_Implementation()
{
	UDRCharacterMovementComponent* Movement =
		GetDRCharacterMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(false);
	}

	RefreshJetpackActivePresentation();
}

void ADRPlayerCharacter::OnRep_JetpackActive()
{
	/*
	 * 연료 소진, 착지, 사망 등 서버가 강제로 종료한 경우
	 * 소유 클라이언트의 예측 상태도 정리한다.
	 */
	if (!bIsJetpackActive && IsLocallyControlled())
	{
		UDRCharacterMovementComponent* Movement =
			GetDRCharacterMovementComponent();

		if (IsValid(Movement))
		{
			Movement->SetWantsJetpack(false);
		}
	}

	RefreshJetpackActivePresentation();
}

bool ADRPlayerCharacter::CanStartJetpack() const
{
	if (!HasAuthority() || IsDead())
	{
		return false;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	if (!IsValid(MovementComponent) || !MovementComponent->IsFalling())
	{
		return false;
	}

	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return false;
	}

	return DRPlayerState->HasJetpack() && DRPlayerState->GetJetpackFuel() > 0.f;
}

void ADRPlayerCharacter::ServerStartJetpack_Implementation()
{
	if (!CanStartJetpack())
	{
		ClientRejectJetpack();
		return;
	}

	StartJetpackFromServer();
}

void ADRPlayerCharacter::ServerStopJetpack_Implementation()
{
	StopJetpackFromServer();
}

void ADRPlayerCharacter::StartJetpackFromServer()
{
	if (!HasAuthority() || bIsJetpackActive)
	{
		return;
	}

	UDRCharacterMovementComponent* Movement = GetDRCharacterMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(true);
	}

	bIsJetpackActive = true;

	// 서버에서는 연료 소비를 위해 Character Tick 사용
	SetActorTickEnabled(true);

	RefreshJetpackActivePresentation();
	ForceNetUpdate();
}

void ADRPlayerCharacter::StopJetpackFromServer()
{
	if (!HasAuthority())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRCharacterMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(false);
	}

	if (!bIsJetpackActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	bIsJetpackActive = false;
	SetActorTickEnabled(false);

	RefreshJetpackActivePresentation();
	ForceNetUpdate();
}

void ADRPlayerCharacter::UpdateJetpackFuel(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRCharacterMovementComponent();

	ADRPlayerState* DRPlayerState =
		GetPlayerState<ADRPlayerState>();

	if (!IsValid(Movement) ||
		!IsValid(DRPlayerState) ||
		!Movement->IsFalling() ||
		!Movement->WantsJetpack() ||
		!DRPlayerState->HasJetpack())
	{
		StopJetpackFromServer();
		return;
	}

	const float FuelCost = JetpackFuelConsumptionPerSecond * DeltaSeconds;

	if (!DRPlayerState->ConsumeJetpackFuel(FuelCost))
	{
		StopJetpackFromServer();
		ClientRejectJetpack();
		return;
	}

	if (DRPlayerState->GetJetpackFuel() <= KINDA_SMALL_NUMBER)
	{
		StopJetpackFromServer();
		ClientRejectJetpack();
	}
}

void ADRPlayerCharacter::RefreshJetpackActivePresentation()
{
	const UDRCharacterMovementComponent* Movement =
		GetDRCharacterMovementComponent();

	const bool bPresentationActive =
		IsLocallyControlled()
			? IsValid(Movement) &&
				Movement->WantsJetpack()
			: bIsJetpackActive;

	// 기존 Jetpack Sound
	if (bPresentationActive)
	{
		if (!IsValid(JetpackAudioComponent) &&
			IsValid(JetpackSound))
		{
			JetpackAudioComponent =
				UGameplayStatics::SpawnSoundAttached(
					JetpackSound,
					GetRootComponent());
		}
	}
	else
	{
		if (IsValid(JetpackAudioComponent))
		{
			JetpackAudioComponent->Stop();
			JetpackAudioComponent = nullptr;
		}
	}

	// Camera Shake는 자기 화면에만
	if (!IsLocallyControlled())
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

	if (bPresentationActive)
	{
		if (!IsValid(JetpackCameraShakeInstance) &&
			JetpackCameraShakeClass)
		{
			JetpackCameraShakeInstance =
				PlayerController->PlayerCameraManager
					->StartCameraShake(
						JetpackCameraShakeClass,
						1.f,
						ECameraShakePlaySpace::CameraLocal,
						FRotator::ZeroRotator);
		}
	}
	else
	{
		if (IsValid(JetpackCameraShakeInstance))
		{
			PlayerController->PlayerCameraManager
				->StopCameraShake(
					JetpackCameraShakeInstance,
					false);

			JetpackCameraShakeInstance = nullptr;
		}
	}
}

void ADRPlayerCharacter::PlayOwnerMeleeAttackPresentation()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.8f,
			FColor::Yellow,
			TEXT("[Melee] 1인칭 공격 표현 실행"));
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT(
			"[Melee] Owner presentation "
			"Character=%s Authority=%d"),
		*GetName(),
		HasAuthority());
	
	// 향후
	// FirstPersonEquipmentRoot Timeline
	// 또는 1인칭 팔 Montage
}

bool ADRPlayerCharacter::CanStartMeleeAttack() const
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!HasHeldItemAction(EDRItemActionType::MeleeAttack))
	{
		return false;
	}
	
	if (bIsMeleeAttacking)
	{
		return false;
	}

	if (CurrentHealth <= 0.f)
	{
		return false;
	}

	return true;
}

void ADRPlayerCharacter::PerformMeleeHitCheck()
{
	if (!HasAuthority() ||
		!bIsMeleeAttacking)
	{
		return;
	}

	const FVector TraceStart =
		GetPawnViewLocation();

	const FRotator AimRotation =
		GetBaseAimRotation();

	const FVector TraceEnd =
		TraceStart +
		AimRotation.Vector() * MeleeAttackRange;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MeleeAttackTrace),
		false,
		this);

	QueryParams.AddIgnoredActor(this);

	FHitResult HitResult;

	const bool bHit =
		GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);

#if ENABLE_DRAW_DEBUG
	DrawDebugLine(
		GetWorld(),
		TraceStart,
		TraceEnd,
		bHit ? FColor::Green : FColor::Red,
		false,
		1.5f,
		0,
		2.f);
#endif

	if (!bHit)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Melee] Miss Character=%s"),
			*GetName());

		return;
	}

	ADRPlayerCharacter* HitPlayer =
		Cast<ADRPlayerCharacter>(
			HitResult.GetActor());

	if (!IsValid(HitPlayer) ||
		HitPlayer == this)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT(
				"[Melee] Hit non-player actor=%s"),
			*GetNameSafe(HitResult.GetActor()));

		return;
	}
	
	const float AppliedDamage =
		UGameplayStatics::ApplyDamage(
			HitPlayer,
			MeleeAttackDamage,
			GetController(),
			this,
			UDamageType::StaticClass());

	if (AppliedDamage <= 0.f)
	{
		return;
	}

	const bool bKilled = HitPlayer->IsDead();

	// 공격자
	ClientPlayMeleeHitFeedback(bKilled);

	// 피격자
	HitPlayer->ClientPlayMeleeDamagedFeedback(
		bKilled);

	MulticastPlayMeleeImpactSound(
		bKilled,
		HitResult.ImpactPoint);
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[Melee] Attacker=%s Target=%s Damage=%.1f"),
		*GetName(),
		*GetNameSafe(HitPlayer),
		MeleeAttackDamage);
}

void ADRPlayerCharacter::FinishMeleeAttack()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsMeleeAttacking = false;
}

void ADRPlayerCharacter::HandleDeath()
{
	if (!HasAuthority() ||
		!IsDead())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(
		MeleeHitTimerHandle);

	GetWorldTimerManager().ClearTimer(
		MeleeFinishTimerHandle);

	bIsMeleeAttacking = false;

	StopJetpackFromServer();

	/*
	 * 사망 순간의 Actor 위치는 저장하지 않는다.
	 * 리스폰 직전에 서버 래그돌 위치를 조회한다.
	 */
	ApplyDeathRagdoll();

	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&ThisClass::RespawnAtRagdollLocation,
		RespawnDelay,
		false);
	
	OnPlayerCharacterDeathDelegate.Broadcast();
	
	ForceNetUpdate();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[Death] Character=%s RespawnDelay=%.1f"),
		*GetName(),
		RespawnDelay);
}

void ADRPlayerCharacter::ApplyDeathRagdoll()
{
	if (bDeathRagdollApplied)
	{
		return;
	}

	bDeathRagdollApplied = true;

	// 공격 몽타주를 포함한 현재 몽타주 정지
	StopAnimMontage();

	UCharacterMovementComponent* MovementComponent =
		GetCharacterMovement();

	if (IsValid(MovementComponent))
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();

	if (IsValid(CapsuleComp))
	{
		CapsuleComp->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* CharacterMesh =
		GetMesh();

	if (IsValid(CharacterMesh))
	{
		/*
		 * Character Mesh는 원래 Capsule에 붙어 있으므로,
		 * 월드 위치를 유지하면서 분리한 뒤 물리를 활성화한다.
		 */
		CharacterMesh->DetachFromComponent(
			FDetachmentTransformRules::KeepWorldTransform);

		CharacterMesh->SetCollisionProfileName(
			TEXT("Ragdoll"));

		CharacterMesh->SetCollisionEnabled(
			ECollisionEnabled::QueryAndPhysics);

		CharacterMesh->SetAllBodiesSimulatePhysics(true);
		CharacterMesh->SetSimulatePhysics(true);
		CharacterMesh->WakeAllRigidBodies();
	}

	// 본인 화면의 1인칭 장비는 숨김
	if (IsValid(FirstPersonHandEquipmentMesh))
	{
		FirstPersonHandEquipmentMesh->SetVisibility(
			false,
			true);
	}

	// 자신의 입력 차단
	if (AController* OwningController =
			GetController())
	{
		OwningController->SetIgnoreMoveInput(true);
		OwningController->SetIgnoreLookInput(true);
	}

	if (IsLocallyControlled() &&
		GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			RespawnDelay,
			FColor::Red,
			TEXT("YOU DIED"));
	}
}

void ADRPlayerCharacter::RespawnAtRagdollLocation()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    AController* RespawnController =
        GetController();

    AGameModeBase* GameMode =
        World->GetAuthGameMode();

    if (!IsValid(RespawnController) ||
        !IsValid(GameMode))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "[Respawn] Invalid Controller or GameMode. "
                "Character=%s Controller=%s GameMode=%s"),
            *GetName(),
            *GetNameSafe(RespawnController),
            *GetNameSafe(GameMode));

        return;
    }

    /*
     * 래그돌 물리를 끄기 전에 서버 래그돌 주변에서
     * 실제 Capsule이 들어갈 위치를 탐색한다.
     */
    FTransform RagdollRespawnTransform;

    const bool bFoundRagdollRespawnLocation =
        TryFindRagdollRespawnTransform(
            RagdollRespawnTransform);

    USkeletalMeshComponent* CharacterMesh =
        GetMesh();

    if (IsValid(CharacterMesh))
    {
        CharacterMesh->SetAllBodiesSimulatePhysics(false);
        CharacterMesh->SetSimulatePhysics(false);

        CharacterMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision);

        CharacterMesh->SetVisibility(
            false,
            true);
    }

    RespawnController->SetIgnoreMoveInput(false);
    RespawnController->SetIgnoreLookInput(false);

    RespawnController->UnPossess();

    if (bFoundRagdollRespawnLocation)
    {
        GameMode->RestartPlayerAtTransform(
            RespawnController,
            RagdollRespawnTransform);
    }
    else
    {
        /*
         * 래그돌 주변에 안전한 공간이 없으면
         * 공중의 래그돌 위치에 억지로 생성하지 않는다.
         * GameMode의 기본 PlayerStart를 사용한다.
         */
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[Respawn] Safe ragdoll location not found. "
                "Fallback to PlayerStart. Character=%s"),
            *GetName());

        GameMode->RestartPlayer(
            RespawnController);
    }

    APawn* NewPawn =
        RespawnController->GetPawn();

    /*
     * 안전하다고 판단한 위치에서도 Spawn Collision 설정 등에
     * 의해 실패할 가능성이 있으므로 PlayerStart를 한 번 더 시도한다.
     */
    if ((!IsValid(NewPawn) ||
         NewPawn == this) &&
        bFoundRagdollRespawnLocation)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[Respawn] Ragdoll location spawn failed. "
                "Retrying at PlayerStart. Controller=%s"),
            *GetNameSafe(RespawnController));

        GameMode->RestartPlayer(
            RespawnController);

        NewPawn =
            RespawnController->GetPawn();
    }

    if (!IsValid(NewPawn) ||
        NewPawn == this)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "[Respawn] All respawn attempts failed. "
                "Controller=%s"),
            *GetNameSafe(RespawnController));

        /*
         * 새 Pawn 생성에 성공하지 않았으므로
         * 기존 Pawn을 Destroy하지 않는다.
         */
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[Respawn] OldPawn=%s NewPawn=%s "
            "UsedRagdollLocation=%d Location=%s"),
        *GetName(),
        *GetNameSafe(NewPawn),
        bFoundRagdollRespawnLocation,
        *NewPawn->GetActorLocation().ToString());

    Destroy();
}

bool ADRPlayerCharacter::TryFindRagdollRespawnTransform(
    FTransform& OutRespawnTransform) const
{
    const UWorld* World = GetWorld();

    const USkeletalMeshComponent* CharacterMesh =
        GetMesh();

    const UCapsuleComponent* CharacterCapsule =
        GetCapsuleComponent();

    const UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    if (!IsValid(World) ||
        !IsValid(CharacterMesh) ||
        !IsValid(CharacterCapsule) ||
        !IsValid(MovementComponent))
    {
        return false;
    }

    FVector RagdollLocation =
        CharacterMesh->GetComponentLocation();

    if (CharacterMesh->DoesSocketExist(
            RespawnRagdollBoneName))
    {
        RagdollLocation =
            CharacterMesh->GetSocketLocation(
                RespawnRagdollBoneName);
    }

    const float CapsuleRadius =
        CharacterCapsule->GetScaledCapsuleRadius();

    const float CapsuleHalfHeight =
        CharacterCapsule->GetScaledCapsuleHalfHeight();

    const FCollisionShape CapsuleShape =
        FCollisionShape::MakeCapsule(
            CapsuleRadius,
            CapsuleHalfHeight);

    const FName CapsuleCollisionProfile =
        CharacterCapsule->GetCollisionProfileName();

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(RagdollRespawnCapsuleSweep),
        false,
        this);

    // 기존 래그돌과 캡슐은 탐색에서 제외한다.
    QueryParams.AddIgnoredActor(this);

    /*
     * 0번은 래그돌 바로 아래다.
     * 이후에는 8방향으로 탐색 반경을 넓힌다.
     */
    TArray<FVector2D> SearchOffsets;
    SearchOffsets.Add(FVector2D::ZeroVector);

    constexpr int32 DirectionCount = 8;

    for (int32 RingIndex = 1; RingIndex <= RespawnSearchRingCount; ++RingIndex)
    {
        const float SearchDistance =
            RespawnSearchStep * RingIndex;

        for (int32 DirectionIndex = 0; DirectionIndex < DirectionCount; ++DirectionIndex)
        {
            const float AngleRadians =
                2.f *
                PI *
                static_cast<float>(DirectionIndex) / static_cast<float>(DirectionCount);

            SearchOffsets.Add(
                FVector2D(
                    FMath::Cos(AngleRadians),
                    FMath::Sin(AngleRadians)) * SearchDistance);
        }
    }

    for (const FVector2D& Offset : SearchOffsets)
    {
        const FVector SearchCenter(
            RagdollLocation.X + Offset.X,
            RagdollLocation.Y + Offset.Y,
            RagdollLocation.Z);

        /*
         * 전체 캐릭터 Capsule을 위에서 아래로 Sweep한다.
         * 따라서 절벽 모서리처럼 Capsule 일부가 걸치는 위치를
         * LineTrace보다 먼저 걸러낼 수 있다.
         */
        const FVector SweepStart =
            SearchCenter +
            FVector(
                0.f,
                0.f,
                RespawnSweepStartHeight);

        const FVector SweepEnd =
            SearchCenter -
            FVector(
                0.f,
                0.f,
                RespawnGroundTraceDistance);

        FHitResult GroundHit;

        const bool bHitGround =
            World->SweepSingleByProfile(
                GroundHit,
                SweepStart,
                SweepEnd,
                FQuat::Identity,
                CapsuleCollisionProfile,
                CapsuleShape,
                QueryParams);

        if (!bHitGround ||
            GroundHit.bStartPenetrating)
        {
            continue;
        }

        // 벽이나 너무 가파른 경사면은 바닥으로 사용하지 않는다.
        if (!MovementComponent->IsWalkable(GroundHit))
        {
            continue;
        }

        /*
         * Capsule Sweep의 Location은 충돌 당시 Capsule 중심점이다.
         * ImpactPoint에 HalfHeight를 다시 더하지 않는다.
         */
        const FVector CandidateLocation =
            GroundHit.Location +
            FVector(
                0.f,
                0.f,
                RespawnGroundClearance);

        /*
         * 최종 위치에서 실제 Capsule 전체가 다른 지형이나
         * 다른 플레이어와 겹치지 않는지 다시 확인한다.
         */
        const bool bBlocked =
            World->OverlapBlockingTestByProfile(
                CandidateLocation,
                FQuat::Identity,
                CapsuleCollisionProfile,
                CapsuleShape,
                QueryParams);

        if (bBlocked)
        {
            continue;
        }

        OutRespawnTransform =
            FTransform(
                FRotator(
                    0.f,
                    GetActorRotation().Yaw,
                    0.f),
                CandidateLocation,
                FVector::OneVector);

#if ENABLE_DRAW_DEBUG
        DrawDebugCapsule(
            World,
            CandidateLocation,
            CapsuleHalfHeight,
            CapsuleRadius,
            FQuat::Identity,
            FColor::Green,
            false,
            5.f);
#endif

        return true;
    }

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(
        World,
        RagdollLocation,
        30.f,
        16,
        FColor::Red,
        false,
        5.f);
#endif

    return false;
}

void ADRPlayerCharacter::RestoreControllerInput()
{
	AController* OwningController =
		GetController();

	if (!IsValid(OwningController))
	{
		return;
	}

	OwningController->SetIgnoreMoveInput(false);
	OwningController->SetIgnoreLookInput(false);
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

void ADRPlayerCharacter::ServerRequestMeleeAttack_Implementation()
{
	if (!CanStartMeleeAttack())
	{
		return;
	}

	bIsMeleeAttacking = true;

	// 서버에서 Melee가 승인됐으므로 3인칭 Melee 연출 실행
	MulticastPlayWorldItemActionPresentation(
		EDRItemActionType::MeleeAttack);
	
	// 공격 애니메이션의 타격 시점에 서버 판정
	GetWorldTimerManager().SetTimer(
		MeleeHitTimerHandle,
		this,
		&ThisClass::PerformMeleeHitCheck,
		MeleeAttackHitTime,
		false);

	// 공격 종료 후 다시 공격 가능
	GetWorldTimerManager().SetTimer(
		MeleeFinishTimerHandle,
		this,
		&ThisClass::FinishMeleeAttack,
		MeleeAttackDuration,
		false);
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
		return MeleeAttackDuration;

	default:
		return 0.f;
	}
}

void ADRPlayerCharacter::PlayFirstPersonItemSwing(
	const FDRFirstPersonSwingPresentation& Presentation)
{
	if (!IsLocallyControlled() ||
		!IsValid(FirstPersonItemSwingTimeline) ||
		!IsValid(FirstPersonEquipmentRoot) ||
		!IsValid(Presentation.Curve))
	{
		return;
	}

	// 혹시 기존 스윙이 재생 중이었다면 정리
	FirstPersonItemSwingTimeline->Stop();

	// 항상 기본 위치에서 새 Action 시작
	FirstPersonEquipmentRoot->SetRelativeTransform(
		FirstPersonEquipmentRootBaseTransform);

	// 이번 Action에서 사용할 Transform 데이터 저장
	ActiveFirstPersonSwingRotation =
		Presentation.RotationOffset;

	ActiveFirstPersonSwingLocation =
		Presentation.LocationOffset;

	// 이번 Action에 맞는 Curve로 교체
	FirstPersonItemSwingTimeline->SetFloatCurve(
		Presentation.Curve,
		FirstPersonSwingTrackName);

	FirstPersonItemSwingTimeline->PlayFromStart();
}

void ADRPlayerCharacter::UpdateFirstPersonItemSwing(
	float CurveValue)
{
	if (!IsLocallyControlled() ||
		!IsValid(FirstPersonEquipmentRoot))
	{
		return;
	}

	const FVector BaseLocation =
		FirstPersonEquipmentRootBaseTransform.GetLocation();

	const FRotator BaseRotation =
		FirstPersonEquipmentRootBaseTransform.Rotator();

	const FVector NewLocation =
		BaseLocation +
		ActiveFirstPersonSwingLocation * CurveValue;

	const FRotator RotationOffset =
		ActiveFirstPersonSwingRotation * CurveValue;

	const FRotator NewRotation =
		BaseRotation + RotationOffset;

	FirstPersonEquipmentRoot->SetRelativeLocationAndRotation(
		NewLocation,
		NewRotation);
}

void ADRPlayerCharacter::FinishFirstPersonItemSwing()
{
	if (!IsValid(FirstPersonEquipmentRoot))
	{
		return;
	}

	FirstPersonEquipmentRoot->SetRelativeTransform(
		FirstPersonEquipmentRootBaseTransform);
}

void ADRPlayerCharacter::PlayFirstPersonItemActionPresentation(
	EDRItemActionType ActionType)
{
	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		PlayFirstPersonItemSwing(
			FirstPersonDigPresentation);
		if (IsValid(DigSound))
		{
			UGameplayStatics::PlaySound2D(
				this,
				DigSound);
		}
		break;

	case EDRItemActionType::MeleeAttack:
		PlayFirstPersonItemSwing(
			FirstPersonMeleePresentation);

		if (IsValid(MeleeAirSound))
		{
			UGameplayStatics::PlaySound2D(
				this,
				MeleeAirSound);
		}
		
		break;

	case EDRItemActionType::Throw:
	case EDRItemActionType::None:
	default:
		break;
	}
}

void ADRPlayerCharacter::PlayWorldItemActionPresentation(
	EDRItemActionType ActionType)
{
	UAnimMontage* Montage =
		ResolveWorldItemActionMontage(ActionType);

	if (!IsValid(Montage))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[ItemAction] World montage is invalid. "
				"Character=%s Action=%s"),
			*GetName(),
			*UEnum::GetValueAsString(ActionType));

		return;
	}

	PlayAnimMontage(Montage);
}

UAnimMontage* ADRPlayerCharacter::ResolveWorldItemActionMontage(
	EDRItemActionType ActionType) const
{
	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		return WorldDigMontage;

	case EDRItemActionType::MeleeAttack:
		return WorldMeleeAttackMontage;

	case EDRItemActionType::Throw:
	case EDRItemActionType::None:
	default:
		return nullptr;
	}
}

void ADRPlayerCharacter::ServerRequestDigPresentation_Implementation()
{
	if (IsDead() ||
		!HasHeldItemAction(EDRItemActionType::Dig))
	{
		return;
	}

	MulticastPlayWorldItemActionPresentation(
		EDRItemActionType::Dig);
}

void ADRPlayerCharacter::MulticastPlayWorldItemActionPresentation_Implementation(
	EDRItemActionType ActionType)
{
	if (IsLocallyControlled())
	{
		return;
	}

	PlayWorldItemActionPresentation(ActionType);

	USoundBase* ActionSound = nullptr;

	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		ActionSound = DigSound;
		break;

	case EDRItemActionType::MeleeAttack:
		ActionSound = MeleeAirSound;
		break;

	default:
		break;
	}

	if (IsValid(ActionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ActionSound,
			GetActorLocation());
	}
}

void ADRPlayerCharacter::ClientPlayFallSound_Implementation(
	bool bTookFallDamage,
	bool bDied)
{
	USoundBase* SoundToPlay = nullptr;

	if (bDied)
	{
		SoundToPlay = FallDeadSound;
	}
	else if (bTookFallDamage)
	{
		SoundToPlay = FallDamageSound;
	}
	else
	{
		SoundToPlay = FallSound;
	}

	if (IsValid(SoundToPlay))
	{
		UGameplayStatics::PlaySound2D(
			this,
			SoundToPlay);
	}

	// 낙하 피해가 발생한 착지에만 Camera Shake
	if (bTookFallDamage || bDied)
	{
		PlayLocalCameraShake(
			FallDamageCameraShakeClass,
			bDied ? 1.4f : 1.f);
	}
}

void ADRPlayerCharacter::MulticastPlayMeleeImpactSound_Implementation(
	bool bKilled,
	FVector_NetQuantize ImpactLocation)
{
	USoundBase* SoundToPlay =
		bKilled
			? MeleeKillSound
			: MeleeHitSound;

	if (!IsValid(SoundToPlay))
	{
		return;
	}

	// 공격한 본인은 1인칭 피드백으로 바로 들음
	if (IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(
			this,
			SoundToPlay);

		return;
	}

	// 다른 플레이어는 실제 맞은 위치에서 들음
	UGameplayStatics::PlaySoundAtLocation(
		this,
		SoundToPlay,
		ImpactLocation);
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

void ADRPlayerCharacter::ClientPlayMeleeHitFeedback_Implementation(
	bool bKilled)
{
	PlayLocalCameraShake(
		MeleeHitConfirmCameraShakeClass,
		bKilled ? 1.3f : 1.f);
}

void ADRPlayerCharacter::ClientPlayMeleeDamagedFeedback_Implementation(
	bool bKilled)
{
	PlayLocalCameraShake(
		MeleeDamagedCameraShakeClass,
		bKilled ? 1.2f : 1.f);
}

void ADRPlayerCharacter::ClientPlayDamagedCameraShake_Implementation()
{
	PlayLocalCameraShake(
		MeleeDamagedCameraShakeClass,
		1.f);
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

void ADRPlayerCharacter::OnRep_CurrentHealth()
{
	/*
	 * ProgressBar 바인딩 방식이면 비어 있어도 된다.
	 * 나중에는 HUD 갱신 델리게이트를 호출할 수 있다.
	 */
	
	if (IsDead())
	{
		ApplyDeathRagdoll();
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
	const ADRPlayerState* DRPlayerState =
		GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		ClearBackEquipmentVisual();
		return;
	}

	if (DRPlayerState->HasJetpack())
	{
		ApplyBackEquipmentVisual(
			JetpackMesh,
			JetpackRelativeTransform);
	}
	else
	{
		ClearBackEquipmentVisual();
	}
}

void ADRPlayerCharacter::HandleJumpPressed()
{
	if (!IsLocallyControlled() || IsDead())
	{
		return;
	}

	UCharacterMovementComponent* MovementComponent =
		GetCharacterMovement();

	if (!IsValid(MovementComponent))
	{
		return;
	}

	// 지상에서 처음 누르면 일반 점프
	if (MovementComponent->IsMovingOnGround())
	{
		Jump();
		return;
	}

	// 공중에서 다시 누르면 제트팩 요청
	if (MovementComponent->IsFalling())
	{
		ADRPlayerState* DRPlayerState =
			GetPlayerState<ADRPlayerState>();

		if (!IsValid(DRPlayerState) ||
			!DRPlayerState->HasJetpack() ||
			DRPlayerState->GetJetpackFuel() <=
				KINDA_SMALL_NUMBER)
		{
			return;
		}

		/*
		 * 서버 응답을 기다리지 않고 소유 클라이언트에서
		 * CharacterMovement 예측을 즉시 시작한다.
		 */
		UDRCharacterMovementComponent* DRMovement =
			GetDRCharacterMovementComponent();

		if (IsValid(DRMovement))
		{
			DRMovement->SetWantsJetpack(true);
		}

		RefreshJetpackActivePresentation();
		ServerStartJetpack();
	}
}

void ADRPlayerCharacter::HandleJumpReleased()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	StopJumping();

	UDRCharacterMovementComponent* Movement =
		GetDRCharacterMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(false);
	}

	RefreshJetpackActivePresentation();
	ServerStopJetpack();
}

