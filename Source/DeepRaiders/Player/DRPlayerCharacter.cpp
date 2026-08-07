#include "DRPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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