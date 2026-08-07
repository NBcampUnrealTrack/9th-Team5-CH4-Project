#include "DRInteractionComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UDRInteractionComponent::UDRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InitSphereRadius(300.f);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetHiddenInGame(true);
	ShapeColor = FColor::Green;

	// 플레이어의 상호작용 범위 진입 및 이탈을 감지합니다.
	OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleEndOverlap);
}

void UDRInteractionComponent::Interact(APawn* Interactor)
{
	AActor* Owner = GetOwner();

	// 상호작용은 유효한 서버 오브젝트와 플레이어만 처리합니다.
	if (!IsValid(Owner) || !Owner->HasAuthority()
		|| !IsValid(Interactor))
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Interact: Interactor=%s, OverlappedObject=%s"),
		*GetNameSafe(Interactor),
		*GetNameSafe(Owner));

	// 상호작용 이벤트를 구독자에게 전달합니다.
	OnInteracted.Broadcast(Interactor);
}

void UDRInteractionComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool IsFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Interactor = Cast<APawn>(OtherActor);

	// 플레이어가 아닌 오브젝트의 오버랩은 무시합니다.
	if (!IsValid(Interactor))
	{
		return;
	}

	// 범위 진입 이벤트를 알리고 상호작용을 실행합니다.
	OnInteractionEntered.Broadcast(Interactor);
	Interact(Interactor);
}

void UDRInteractionComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	APawn* Interactor = Cast<APawn>(OtherActor);

	if (IsValid(Interactor))
	{
		// 범위를 벗어난 플레이어에게 이탈 이벤트를 알립니다.
		OnInteractionExited.Broadcast(Interactor);
	}
}
