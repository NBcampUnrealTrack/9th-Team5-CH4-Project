#include "DRBarrierGenerator.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

ADRBarrierGenerator::ADRBarrierGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicateMovement(false);
	// 충돌은 구형 BarrierCollision이 전담한다. BreakableMesh는 생성기 외형용이다.
	BreakableMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BrokenLifeSpan = 0.f;

	BarrierCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BarrierCollision"));
	BarrierCollision->SetupAttachment(BreakableMeshComponent);
	// 생성기 메시의 편집 스케일이 배리어 판정 크기에 섞이지 않게 한다.
	BarrierCollision->SetAbsolute(false, false, true);
	BarrierCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BarrierCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BarrierCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BarrierCollision->SetCollisionResponseToChannel(DRCollisionChannels::Projectile, ECR_Overlap);
	BarrierCollision->SetGenerateOverlapEvents(true);
	BarrierCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBarrierBeginOverlap);

	// 물리 투사체와는 Ignore로 결합되고, DRProjectile 채널 Trace에만 Block으로 응답한다.
	// 따라서 이동 중인 아군 투사체를 정지시키지 않으면서 히트스캔은 배리어를 맞출 수 있다.
	BarrierTraceCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BarrierTraceCollision"));
	BarrierTraceCollision->SetupAttachment(BreakableMeshComponent);
	BarrierTraceCollision->SetAbsolute(false, false, true);
	BarrierTraceCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BarrierTraceCollision->SetCollisionObjectType(DRCollisionChannels::BarrierTrace);
	BarrierTraceCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BarrierTraceCollision->SetCollisionResponseToChannel(DRCollisionChannels::Projectile, ECR_Block);
	BarrierTraceCollision->SetGenerateOverlapEvents(false);

	BarrierVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierVisual"));
	BarrierVisual->SetupAttachment(BreakableMeshComponent);
	BarrierVisual->SetAbsolute(false, false, true);
	BarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierVisual->SetCanEverAffectNavigation(false);
}

USphereComponent* ADRBarrierGenerator::GetBarrierCollisionComponent() const
{
	return BarrierCollision.Get();
}

void ADRBarrierGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, BarrierRadius);
	DOREPLIFETIME(ThisClass, BarrierDuration);
	DOREPLIFETIME(ThisClass, OwnerTeamId);
}

void ADRBarrierGenerator::Initialize(ADRPlayerCharacter* SourceCharacter, const float InBarrierRadius,
	const float InBarrierDuration, const float InBarrierMaxHealth)
{
	if (!HasAuthority() || !IsValid(SourceCharacter))
	{
		return;
	}

	OwnerTeamId = DRCombatTeam::GetActorTeamId(SourceCharacter);
	BarrierRadius = FMath::Max(1.f, InBarrierRadius);
	BarrierDuration = FMath::Max(0.1f, InBarrierDuration);
	MaxHealth = FMath::Max(1.f, InBarrierMaxHealth);
	SetLifeSpan(BarrierDuration);
}

void ADRBarrierGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshBarrierGeometry();
}

void ADRBarrierGenerator::BeginPlay()
{
	Super::BeginPlay();

	// BP에 과거 Component 충돌값이 남아 있어도 런타임 정책은 네이티브에서 확정한다.
	BreakableMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BarrierCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BarrierCollision->SetCollisionResponseToChannel(DRCollisionChannels::Projectile, ECR_Overlap);
	BarrierCollision->SetGenerateOverlapEvents(true);
	BarrierTraceCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BarrierTraceCollision->SetCollisionObjectType(DRCollisionChannels::BarrierTrace);
	BarrierTraceCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BarrierTraceCollision->SetCollisionResponseToChannel(DRCollisionChannels::Projectile, ECR_Block);
	BarrierTraceCollision->SetGenerateOverlapEvents(false);

	RefreshBarrierGeometry();

	// 물리 투사체의 판정과 배리어 내구도 변경은 서버만 처리한다.
	if (HasAuthority())
	{
		BarrierCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		BarrierCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (HasAuthority() && OwnerTeamId == INDEX_NONE)
	{
		Destroy();
	}
}

void ADRBarrierGenerator::OnRep_BarrierRadius()
{
	RefreshBarrierGeometry();
}

void ADRBarrierGenerator::RefreshBarrierGeometry()
{
	BarrierCollision->SetSphereRadius(BarrierRadius);
	BarrierTraceCollision->SetSphereRadius(BarrierRadius);

	if (const UStaticMesh* VisualMesh = BarrierVisual->GetStaticMesh())
	{
		const float MeshRadius = VisualMesh->GetBounds().SphereRadius;
		if (MeshRadius > KINDA_SMALL_NUMBER)
		{
			const float UniformScale = BarrierRadius / MeshRadius;
			BarrierVisual->SetRelativeScale3D(FVector(UniformScale));
		}
	}
}

void ADRBarrierGenerator::HandleBarrierBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority() || IsBroken())
	{
		return;
	}

	if (ADRProjectile* Projectile = Cast<ADRProjectile>(OtherActor))
	{
		Projectile->HandleBarrierOverlap(this);
	}
}

float ADRBarrierGenerator::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 같은 팀 공격은 투사체/히트스캔 단계에서 통과시키며, 직접 전달된 피해도 안전하게 무시한다.
	if (OwnerTeamId != INDEX_NONE && DRCombatTeam::IsFriendlyTarget(OwnerTeamId, DamageCauser))
	{
		return 0.f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}
