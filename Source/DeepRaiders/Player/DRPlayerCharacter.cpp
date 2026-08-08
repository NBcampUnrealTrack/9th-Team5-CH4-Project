#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DRPlayerState.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

ADRPlayerCharacter::ADRPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 이 Actor가 서버에서 클라이언트로 복제되도록 설정
	bReplicates = true;
	
	MiningComponent =
		CreateDefaultSubobject<UDRMiningComponent>(
			TEXT("MiningComponent"));

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

void ADRPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bIsJetpackActive)
	{
		return;
	}

	UpdateJetpack(DeltaSeconds);
}

void ADRPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (HasAuthority())
	{
		StopJetpackFromServer();
	}

	ADRPlayerState* DRPlayerState =
		GetPlayerState<ADRPlayerState>();

	if (IsValid(DRPlayerState))
	{
		DRPlayerState->RefillJetpackFuel();
	}
}
void ADRPlayerCharacter::RequestMine()
{
	if (IsDead())
	{
		return;
	}
	
	if (!IsValid(MiningComponent))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[%s] MiningComponent is invalid"),
			*GetName());

		return;
	}

	MiningComponent->TryMine();
}

void ADRPlayerCharacter::RequestMeleeAttack()
{
	if (!IsLocallyControlled() || IsDead())
	{
		return;
	}
	
	// 현재는 1인칭 공격 애니메이션이 없으므로 임시 표현만 실행한다.
	PlayOwnerMeleeAttackPresentation();

	// 실제 공격 승인과 판정은 서버가 담당한다.
	ServerRequestMeleeAttack();
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

void ADRPlayerCharacter::RequestNetworkTest()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	ServerToggleNetworkTest();
}

void ADRPlayerCharacter::PossessedBy(
	AController* NewController)
{
	Super::PossessedBy(NewController);

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
		bNetworkTestActive);

	DOREPLIFETIME(
		ADRPlayerCharacter,
		bIsJetpackActive);
	
	DOREPLIFETIME(
		ADRPlayerCharacter,
		CurrentHealth);
}

void ADRPlayerCharacter::ServerToggleNetworkTest_Implementation()
{
	bNetworkTestActive = !bNetworkTestActive;

	// 리슨 서버 월드의 외형 갱신
	ApplyNetworkTestState();

	ForceNetUpdate();
}

void ADRPlayerCharacter::OnRep_NetworkTestActive()
{
	// 복제 값을 받은 클라이언트의 외형 갱신
	ApplyNetworkTestState();
}

void ADRPlayerCharacter::OnRep_JetpackActive()
{
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

	bIsJetpackActive = true;

	// 서버에서 상승력과 연료를 처리한다.
	SetActorTickEnabled(true);

	// 서버에서는 RepNotify가 자동 실행되지 않으므로 직접 반영
	RefreshJetpackActivePresentation();

	ForceNetUpdate();
}

void ADRPlayerCharacter::StopJetpackFromServer()
{
	if (!HasAuthority() || !bIsJetpackActive)
	{
		return;
	}

	bIsJetpackActive = false;

	SetActorTickEnabled(false);

	RefreshJetpackActivePresentation();

	ForceNetUpdate();
}

void ADRPlayerCharacter::UpdateJetpack(
	float DeltaSeconds)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(MovementComponent) ||
		!IsValid(DRPlayerState) ||
		!MovementComponent->IsFalling() ||
		!DRPlayerState->HasJetpack())
	{
		StopJetpackFromServer();
		return;
	}

	const float FuelCost = JetpackFuelConsumptionPerSecond * DeltaSeconds;

	if (!DRPlayerState->ConsumeJetpackFuel(FuelCost))
	{
		StopJetpackFromServer();
		return;
	}

	MovementComponent->Velocity.Z =
		FMath::Min(
			MovementComponent->Velocity.Z + JetpackAcceleration * DeltaSeconds,
			MaxJetpackRiseSpeed);

	if (DRPlayerState->GetJetpackFuel() <= KINDA_SMALL_NUMBER)
	{
		StopJetpackFromServer();
	}
}

void ADRPlayerCharacter::RefreshJetpackActivePresentation()
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[Jetpack] Character=%s Active=%d "
			"Authority=%d Local=%d"),
		*GetName(),
		bIsJetpackActive,
		HasAuthority(),
		IsLocallyControlled());

	/*
	 * 이후 추가할 항목:
	 *
	 * 제트팩 불꽃 Niagara 활성화
	 * 제트팩 사운드 재생
	 * 카메라 흔들림
	 * 캐릭터 애니메이션
	 */
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

	UGameplayStatics::ApplyDamage(
		HitPlayer,
		MeleeAttackDamage,
		GetController(),
		this,
		UDamageType::StaticClass());

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

	// 사망 직전에 예약된 공격 판정이 실행되지 않게 정리
	GetWorldTimerManager().ClearTimer(
		MeleeHitTimerHandle);

	GetWorldTimerManager().ClearTimer(
		MeleeFinishTimerHandle);

	bIsMeleeAttacking = false;

	// 제트팩 종료
	StopJetpackFromServer();

	// 제트팩용 Character Tick도 종료
	SetActorTickEnabled(false);

	/*
	 * 서버에서는 CurrentHealth의 RepNotify가 자동 실행되지 않으므로
	 * 리슨 서버와 서버 인스턴스에는 직접 적용한다.
	 */
	ApplyDeathRagdoll();

	ForceNetUpdate();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Death] Character=%s"),
		*GetName());
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
			5.f,
			FColor::Red,
			TEXT("YOU DIED"));
	}
}

void ADRPlayerCharacter::ServerRequestMeleeAttack_Implementation()
{
	if (!CanStartMeleeAttack())
	{
		return;
	}

	bIsMeleeAttacking = true;

	// 다른 플레이어가 보는 3인칭 공격 연출
	MulticastPlayWorldMeleeAttack();

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

void ADRPlayerCharacter::MulticastPlayWorldMeleeAttack_Implementation()
{
	/*
	 * 공격한 본인은 1인칭 표현을 사용한다.
	 * 본인 클라이언트에서는 월드 Manny 몽타주를 생략한다.
	 */
	if (IsLocallyControlled())
	{
		return;
	}

	PlayWorldMeleeAttackPresentation();
}

void ADRPlayerCharacter::PlayWorldMeleeAttackPresentation()
{
	if (!IsValid(WorldMeleeAttackMontage))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[Melee] WorldMeleeAttackMontage "
				"is invalid. Character=%s"),
			*GetName());

		return;
	}

	PlayAnimMontage(WorldMeleeAttackMontage);
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

void ADRPlayerCharacter::ApplyNetworkTestState()
{
	if (bNetworkTestActive)
	{
		ApplyHandEquipmentVisual(
			EquipmentTestMesh,
			EquipmentTestMesh,
			TestFirstPersonTransform,
			TestWorldHandTransform);

		ApplyBackEquipmentVisual(
			EquipmentTestMesh,
			TestWorldBackTransform);
	}
	else
	{
		ClearHandEquipmentVisual();
		ClearBackEquipmentVisual();
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[EquipmentVisualTest] "
			"Name=%s Active=%d Authority=%d Local=%d"),
		*GetName(),
		bNetworkTestActive,
		HasAuthority(),
		IsLocallyControlled());
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

	// 활성 여부와 관계없이 서버에 중지 요청해도 안전하다.
	ServerStopJetpack();
}