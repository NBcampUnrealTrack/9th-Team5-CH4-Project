#include "DRForwardBarrierGenerator.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "Engine/StaticMesh.h"

ADRForwardBarrierGenerator::ADRForwardBarrierGenerator()
{
	ForwardBarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForwardBarrierMesh"));
	ForwardBarrierMesh->SetupAttachment(BreakableMeshComponent);
	ForwardBarrierMesh->SetAbsolute(false, false, true);
	ForwardBarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForwardBarrierMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ForwardBarrierMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ForwardBarrierMesh->SetCollisionResponseToChannel(DRCollisionChannels::Projectile, ECR_Overlap);
	ForwardBarrierMesh->SetGenerateOverlapEvents(true);
	ForwardBarrierMesh->SetCanEverAffectNavigation(false);
	ForwardBarrierMesh->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBarrierBeginOverlap);

	ForwardBarrierTraceCollision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForwardBarrierTraceCollision"));
	ForwardBarrierTraceCollision->SetupAttachment(BreakableMeshComponent);
	ForwardBarrierTraceCollision->SetAbsolute(false, false, true);
	ForwardBarrierTraceCollision->SetVisibility(false);
	ForwardBarrierTraceCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForwardBarrierTraceCollision->SetCollisionObjectType(DRCollisionChannels::BarrierTrace);
	ForwardBarrierTraceCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	ForwardBarrierTraceCollision->SetCollisionResponseToChannel(DRCollisionChannels::Projectile, ECR_Block);
	ForwardBarrierTraceCollision->SetGenerateOverlapEvents(false);
	ForwardBarrierTraceCollision->SetCanEverAffectNavigation(false);
}

UPrimitiveComponent* ADRForwardBarrierGenerator::GetBarrierCollisionComponent() const
{
	return ForwardBarrierMesh.Get();
}

void ADRForwardBarrierGenerator::BeginPlay()
{
	Super::BeginPlay();

	BarrierVisual->SetVisibility(false);
	BarrierCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierTraceCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForwardBarrierMesh->SetCollisionEnabled(
		HasAuthority() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	ForwardBarrierTraceCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ADRForwardBarrierGenerator::RefreshBarrierGeometry()
{
	AreaEffectCollision->SetSphereRadius(BarrierRadius);
	UStaticMesh* ForwardMesh = ForwardBarrierMesh->GetStaticMesh();
	if (!IsValid(ForwardMesh))
	{
		return;
	}

	ForwardBarrierTraceCollision->SetStaticMesh(ForwardMesh);
	const float MeshRadius = ForwardMesh->GetBounds().BoxExtent.GetMax();
	if (MeshRadius > KINDA_SMALL_NUMBER)
	{
		const FVector MeshScale(BarrierRadius / MeshRadius);
		ForwardBarrierMesh->SetRelativeScale3D(MeshScale);
		ForwardBarrierTraceCollision->SetRelativeScale3D(MeshScale);
	}
}
