#include "DRCNPlayerCharacter.h"

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

ADRCNPlayerCharacter::ADRCNPlayerCharacter()
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

	FirstPersonEquipmentMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("FirstPersonEquipmentMesh"));

	FirstPersonEquipmentMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonEquipmentMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	FirstPersonEquipmentMesh->SetOnlyOwnerSee(true);
	FirstPersonEquipmentMesh->SetIsReplicated(false);

	WorldEquipmentMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("WorldEquipmentMesh"));

	WorldEquipmentMesh->SetupAttachment(
		GetMesh(),
		TEXT("hand_rSocket"));

	WorldEquipmentMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	WorldEquipmentMesh->SetOwnerNoSee(true);

	// 자기 화면에서는 전신 스틱맨을 숨긴다.
	// GetMesh()->SetOwnerNoSee(true);

	// 그림자 보이기
	GetMesh()->SetCastHiddenShadow(true);
}

void ADRCNPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	PrintNetworkState(TEXT("BeginPlay"));
}

void ADRCNPlayerCharacter::SetupPlayerInputComponent(
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

void ADRCNPlayerCharacter::Move(const FInputActionValue& Value)
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

void ADRCNPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void ADRCNPlayerCharacter::HandleNetworkTest(
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

void ADRCNPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	PrintNetworkState(TEXT("PossessedBy"));
}

void ADRCNPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	PrintNetworkState(TEXT("OnRep_Controller"));
}

void ADRCNPlayerCharacter::PrintNetworkState(const TCHAR* Context) const
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

void ADRCNPlayerCharacter::PawnClientRestart()
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

void ADRCNPlayerCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(
		ADRCNPlayerCharacter,
		bNetworkTestActive);
}

void ADRCNPlayerCharacter::ServerToggleNetworkTest_Implementation()
{
	/*
	 * 이 함수는 서버에서만 실행된다.
	 * 따라서 여기에서 다시 HasAuthority()를 검사할 필요는 없다.
	 */

	bNetworkTestActive = !bNetworkTestActive;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[ServerToggleNetworkTest] Name=%s "
			"NewState=%d Authority=%d"
		),
		*GetName(),
		bNetworkTestActive,
		HasAuthority());

	/*
	 * C++에서 서버가 값을 직접 변경해도
	 * 서버 자신의 OnRep는 자동 호출되지 않는다.
	 *
	 * 리슨 서버 호스트 화면에도 효과를 적용하기 위해
	 * 서버에서는 직접 호출한다.
	 */
	OnRep_NetworkTestActive();

	/*
	 * 다음 일반 네트워크 업데이트를 기다리지 않고
	 * 해당 Actor의 복제를 가능한 한 빨리 요청한다.
	 *
	 * 필수는 아니지만 테스트 반응을 확인하기 편하다.
	 */
	ForceNetUpdate();
}

void ADRCNPlayerCharacter::OnRep_NetworkTestActive()
{
	/*
	 * 테스트 상태가 true일 때 Mesh 숨김.
	 * 다시 F를 누르면 false가 되면서 Mesh가 나타난다.
	 */
	GetMesh()->SetVisibility(
		!bNetworkTestActive,
		true);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[OnRep_NetworkTestActive] "
			"Name=%s State=%d "
			"NetMode=%d Authority=%d Local=%d"
		),
		*GetName(),
		bNetworkTestActive,
		static_cast<int32>(GetNetMode()),
		HasAuthority(),
		IsLocallyControlled());
}