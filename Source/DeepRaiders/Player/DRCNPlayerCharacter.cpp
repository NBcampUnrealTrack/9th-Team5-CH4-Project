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

ADRCNPlayerCharacter::ADRCNPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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