#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

ADRPlayerCharacter::ADRPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 이 Actor가 서버에서 클라이언트로 복제되도록 설정
	bReplicates = true;

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

void ADRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	PrintNetworkState(TEXT("BeginPlay"));
}

void ADRPlayerCharacter::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInput)
	{
		return;
	}

	if (IsValid(MoveAction.Get()))
	{
		EnhancedInput->BindAction(
			MoveAction.Get(),
			ETriggerEvent::Triggered,
			this,
			&ThisClass::Move);
	}

	if (IsValid(LookAction.Get()))
	{
		EnhancedInput->BindAction(
			LookAction.Get(),
			ETriggerEvent::Triggered,
			this,
			&ThisClass::Look);
	}

	if (IsValid(JumpAction.Get()))
	{
		EnhancedInput->BindAction(
			JumpAction.Get(),
			ETriggerEvent::Started,
			this,
			&ACharacter::Jump);

		EnhancedInput->BindAction(
			JumpAction.Get(),
			ETriggerEvent::Completed,
			this,
			&ACharacter::StopJumping);
	}
	
	if (IsValid(NetworkTestAction.Get()))
	{
		EnhancedInput->BindAction(
			NetworkTestAction.Get(),
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleNetworkTest);
	}
}

void ADRPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D MoveInput = Value.Get<FVector2D>();

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(
		0.f,
		ControlRotation.Yaw,
		0.f);

	const FVector ForwardDirection =
		FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	const FVector RightDirection =
		FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MoveInput.Y);
	AddMovementInput(RightDirection, MoveInput.X);
}

void ADRPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void ADRPlayerCharacter::HandleNetworkTest(
	const FInputActionValue& Value)
{
	/*
	 * 이 함수는 입력을 가진 Pawn에서만 실행되어야 한다.
	 *
	 * 호스트 자기 Pawn:
	 * Authority = true
	 * Local = true
	 *
	 * 게스트 자기 Pawn:
	 * Authority = false
	 * Local = true
	 */
	if (!IsLocallyControlled())
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[HandleNetworkTest] Name=%s "
			"Authority=%d Local=%d"
		),
		*GetName(),
		HasAuthority(),
		IsLocallyControlled());

	// 실제 상태 변경은 서버에 요청
	ServerToggleNetworkTest();
}

void ADRPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	PrintNetworkState(TEXT("PossessedBy"));
}

void ADRPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	PrintNetworkState(TEXT("OnRep_Controller"));
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

void ADRPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	APlayerController* PlayerController =
		Cast<APlayerController>(GetController());

	if (!IsValid(PlayerController))
	{
		return;
	}

	// 입력은 현재 PC에서 직접 조종하는 캐릭터에만 등록한다.
	if (!PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			LocalPlayer);

	if (!IsValid(InputSubsystem))
	{
		return;
	}

	// TObjectPtr 내부의 UInputMappingContext*를 꺼내서 검사한다.
	if (!IsValid(DefaultMappingContext.Get()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[%s] DefaultMappingContext is null"),
			*GetName());

		return;
	}

	UInputMappingContext* MappingContext =
		DefaultMappingContext.Get();

	// 리스폰 또는 재빙의 때 중복 등록되는 것을 방지한다.
	InputSubsystem->RemoveMappingContext(MappingContext);
	InputSubsystem->AddMappingContext(MappingContext, 0);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[%s] Added Mapping Context: %s"),
		*GetName(),
		*GetNameSafe(MappingContext));
}

void ADRPlayerCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(
		ADRPlayerCharacter,
		bNetworkTestActive);
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