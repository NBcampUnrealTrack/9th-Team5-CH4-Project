#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DRPlayerState.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

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

float ADRPlayerCharacter::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!HasAuthority() ||
		DamageAmount <= 0.f ||
		CurrentHealth <= 0.f)
	{
		return 0.f;
	}

	const float AppliedDamage =
		FMath::Min(DamageAmount, CurrentHealth);

	CurrentHealth = FMath::Clamp(
		CurrentHealth - AppliedDamage,
		0.f,
		MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		// TODO: 사망 처리
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
	if (!HasAuthority())
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

void ADRPlayerCharacter::SetHeldItemDefinition(UDRItemDefinition* NewItemDefinition)
{
	if (!HasAuthority()
		|| HeldItemDefinition == NewItemDefinition)
	{
		return;
	}
	
	HeldItemDefinition = NewItemDefinition;
	RefreshHeldItemVisual();
	ForceNetUpdate();
}

void ADRPlayerCharacter::OnRep_HeldItemDefinition()
{
	RefreshHeldItemVisual();
}

void ADRPlayerCharacter::RefreshHeldItemVisual()
{
	if (!IsValid(HeldItemDefinition))
	{
		ClearHandEquipmentVisual();
		return;
	}
	
	UStaticMesh* VisualMesh = HeldItemDefinition->WorldMesh;
	FTransform VisualOffset = HeldItemDefinition->OffsetTransform;
	
	ApplyHandEquipmentVisual(VisualMesh, VisualMesh, VisualOffset, VisualOffset);
	
}

void ADRPlayerCharacter::OnRep_CurrentHealth()
{
	/*
	 * ProgressBar 바인딩 방식이면 비어 있어도 된다.
	 * 나중에는 HUD 갱신 델리게이트를 호출할 수 있다.
	 */
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
	if (!IsLocallyControlled())
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