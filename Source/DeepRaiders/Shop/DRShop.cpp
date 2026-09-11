#include "DRShop.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Shop/Components/DRShopAreaComponent.h"
#include "DeepRaiders/Shop/Components/DRShopComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ADRShop::ADRShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 상점 액터는 접근 범위와 거래 기능을 컴포넌트로 구성합니다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ShopAreaComponent = CreateDefaultSubobject<UDRShopAreaComponent>(
		TEXT("ShopAreaComponent"));
	ShopAreaComponent->SetupAttachment(Root);

	ShopComponent = CreateDefaultSubobject<UDRShopComponent>(
		TEXT("ShopComponent"));

	static ConstructorHelpers::FObjectFinder<USoundBase> PurchaseSoundAsset(
		TEXT("/Game/DeepRaiders/Sound/SoundWave/Item/Shop_Buy.Shop_Buy"));

	PurchaseSound = PurchaseSoundAsset.Object;
}

bool ADRShop::IsPawnInShopArea(const APawn* Pawn) const
{
	return IsValid(Pawn)
		&& IsValid(ShopAreaComponent)
		&& ShopAreaComponent->IsOverlappingActor(Pawn);
}

bool ADRShop::CanInteract_Implementation(APawn* Interactor) const
{
	FVector InteractionLocation;
	return FindInteractionPoint(Interactor, InteractionLocation);
}

bool ADRShop::Interact_Implementation(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor))
	{
		return false;
	}

	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Interactor->GetController());

	if (!IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->ClientToggleShop(this);
	return true;
}

bool ADRShop::GetInteractionPromptData_Implementation(
	APawn* Interactor,
	FDRInteractionPromptData& OutPromptData) const
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}

	OutPromptData.TitleText = NSLOCTEXT("DRShop", "InteractionTitle", "상점");
	OutPromptData.ActionText = NSLOCTEXT("DRShop", "InteractionAction", "상점 이용");
	return true;
}

bool ADRShop::GetInteractionLocation_Implementation(
	APawn* Interactor,
	FVector& OutInteractionLocation) const
{
	return FindInteractionPoint(Interactor, OutInteractionLocation);
}

bool ADRShop::FindInteractionPoint(
	APawn* Interactor,
	FVector& OutInteractionLocation) const
{
	if (!IsValid(Interactor) || !IsValid(Interactor->GetController()))
	{
		return false;
	}

	UStaticMeshComponent* ShopMesh =
		FindComponentByClass<UStaticMeshComponent>();

	if (!IsValid(ShopMesh))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Interactor->GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRShopInteraction), true, Interactor);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * WORLD_MAX;

	if (!ShopMesh->LineTraceComponent(HitResult, ViewLocation, TraceEnd, QueryParams))
	{
		return false;
	}

	OutInteractionLocation = HitResult.ImpactPoint;
	return true;
}

void ADRShop::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMeshComponent* ShopMesh =
		FindComponentByClass<UStaticMeshComponent>())
	{
		ShopMesh->SetCollisionResponseToChannel(
			DRCollisionChannels::Interaction,
			ECR_Overlap);
	}

	ShopAreaComponent->OnPawnExited.AddDynamic(
		this,
		&ThisClass::HandleShopAreaExited);
}

void ADRShop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ShopAreaComponent))
	{
		ShopAreaComponent->OnPawnExited.RemoveDynamic(
			this,
			&ThisClass::HandleShopAreaExited);
	}

	Super::EndPlay(EndPlayReason);
}

void ADRShop::HandleShopAreaExited(APawn* Pawn)
{
	if (!IsValid(Pawn)
		|| (!Pawn->IsLocallyControlled() && !Pawn->HasAuthority()))
	{
		return;
	}

	if (ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(Pawn->GetController()))
	{
		PlayerController->NotifyShopAreaExited(this);
	}
}
