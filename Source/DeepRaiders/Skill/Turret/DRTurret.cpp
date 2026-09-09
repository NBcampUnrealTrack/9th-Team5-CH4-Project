#include "DRTurret.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/Effects/DRGE_SkillCooldown.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

ADRTurret::ADRTurret()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	TurretRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretRoot"));
	SetRootComponent(TurretRoot);
	BreakableMeshComponent->SetupAttachment(TurretRoot);

	TurretHolderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretHolderMesh"));
	TurretHolderMesh->SetupAttachment(TurretRoot);
	TurretHolderMesh->SetCollisionProfileName(TEXT("DRBreakable"));
	TurretAimPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretAimPivot"));
	TurretAimPivot->SetupAttachment(TurretRoot);
	TurretAimPivot->SetRelativeLocation(TurretAimPivotLocation);
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TurretAimPivot);
	TurretMesh->SetCollisionProfileName(TEXT("DRBreakable"));
	TurretMesh->SetRelativeLocation(-TurretAimPivotLocation);
	TurretMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("TurretMuzzle"));
	TurretMuzzle->SetupAttachment(TurretMesh);
	TurretTankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretTankMesh"));
	TurretTankMesh->SetupAttachment(TurretRoot);
	TurretTankMesh->SetCollisionProfileName(TEXT("DRBreakable"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HolderMesh(
		TEXT("/Game/Fab/Cryo_Cannons_for_Tower_Defence_Game/snow_towers/StaticMeshes/Tier1_Turrent_Holder.Tier1_Turrent_Holder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TurretMeshAsset(
		TEXT("/Game/Fab/Cryo_Cannons_for_Tower_Defence_Game/snow_towers/StaticMeshes/Tier1_Turrent.Tier1_Turrent"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TankMesh(
		TEXT("/Game/Fab/Cryo_Cannons_for_Tower_Defence_Game/snow_towers/StaticMeshes/Tier1_Tank1.Tier1_Tank1"));
	if (HolderMesh.Succeeded())
	{
		TurretHolderMesh->SetStaticMesh(HolderMesh.Object);
	}
	if (TurretMeshAsset.Succeeded())
	{
		TurretMesh->SetStaticMesh(TurretMeshAsset.Object);
	}
	if (TankMesh.Succeeded())
	{
		TurretTankMesh->SetStaticMesh(TankMesh.Object);
	}
}

void ADRTurret::InitializeTurret(ADRPlayerState* InInstallerPlayerState,
	const int32 InOwnerTeamId, const float InLifeSpan,
	UAbilitySystemComponent* InOwnerAbilitySystemComponent,
	const FGameplayTag InCooldownTag, const float InCooldownDuration,
	const FDRTurretWeaponSettings& InWeaponSettings)
{
	InstallerPlayerState = InInstallerPlayerState;
	OwnerTeamId = InOwnerTeamId;
	ConfiguredLifeSpan = FMath::Max(0.f, InLifeSpan);
	OwnerAbilitySystemComponent = InOwnerAbilitySystemComponent;
	CooldownTag = InCooldownTag;
	CooldownDuration = FMath::Max(0.f, InCooldownDuration);
	WeaponSettings = InWeaponSettings;
	ApplyTeamMaterial();
}

bool ADRTurret::IsInstalledBy(const ADRPlayerState* PlayerState) const
{
	return IsValid(PlayerState) && InstallerPlayerState == PlayerState;
}

float ADRTurret::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!CanReceiveDamageFrom(EventInstigator))
	{
		return 0.f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ADRTurret::BeginPlay()
{
	Super::BeginPlay();
	ApplyTeamMaterial();

	if (IsValid(TurretAimPivot))
	{
		TurretAimPivot->SetRelativeLocation(TurretAimPivotLocation);
		if (IsValid(TurretMesh))
		{
			TurretMesh->SetRelativeLocation(-TurretAimPivotLocation);
		}
		if (IsValid(TurretTankMesh))
		{
			TurretTankMesh->SetRelativeLocation(-TurretAimPivotLocation);
		}
	}

	if (HasAuthority() && ConfiguredLifeSpan > 0.f)
	{
		SetLifeSpan(ConfiguredLifeSpan);
	}
}

void ADRTurret::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDrawDetectionRange && WeaponSettings.MaxAttackDistance > 0.f)
	{
		DrawDebugSphere(
			GetWorld(),
			GetActorLocation(),
			WeaponSettings.MaxAttackDistance,
			32,
			FColor::Green,
			false,
			0.05f,
			0,
			2.f);
	}

	if (HasAuthority())
	{
		UpdateTargetAndFire(DeltaSeconds);
	}
}

void ADRTurret::UpdateTargetAndFire(const float DeltaSeconds)
{
	if (!WeaponSettings.ProjectileClass
		|| !IsValid(OwnerAbilitySystemComponent))
	{
		return;
	}

	FireTimer = FMath::Max(0.f, FireTimer - DeltaSeconds);
	DetectionTimer = FMath::Max(0.f, DetectionTimer - DeltaSeconds);
	if (DetectionTimer <= 0.f)
	{
		const int32 CurrentTeamId = GetCurrentOwnerTeamId();
		if (CurrentTeamId != INDEX_NONE && CurrentTeamId != OwnerTeamId)
		{
			OwnerTeamId = CurrentTeamId;
			ApplyTeamMaterial();
			ForceNetUpdate();
		}

		CurrentTarget = FindNearestEnemy();
		DetectionTimer = FMath::Max(WeaponSettings.DetectionInterval, 0.05f);
	}

	if (IsValid(CurrentTarget))
	{
		const FVector AimOrigin = IsValid(TurretMuzzle)
			? TurretMuzzle->GetComponentLocation()
			: GetActorLocation() + FVector::UpVector * 40.f;
		FVector LaunchVelocity;
		if (ResolveProjectileLaunchVelocity(
			AimOrigin,
			CurrentTarget->GetActorLocation(),
			LaunchVelocity)
			&& !LaunchVelocity.IsNearlyZero())
		{
			const FRotator DesiredRotation(0.f, LaunchVelocity.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(
				GetActorRotation(),
				DesiredRotation,
				DeltaSeconds,
				FMath::Max(WeaponSettings.RotationInterpSpeed, 0.1f)));

			if (IsValid(TurretAimPivot))
			{
				const float HorizontalDistance = LaunchVelocity.Size2D();
				const float DesiredPitch = FMath::RadiansToDegrees(
					FMath::Atan2(LaunchVelocity.Z, FMath::Max(HorizontalDistance, 1.f)));
				const FRotator DesiredPivotRotation(DesiredPitch, 0.f, 0.f);
				const FRotator NewPivotRotation = FMath::RInterpTo(
					TurretAimPivot->GetRelativeRotation(),
					DesiredPivotRotation,
					DeltaSeconds,
					FMath::Max(WeaponSettings.RotationInterpSpeed, 0.1f));
				TurretAimPivot->SetRelativeRotation(NewPivotRotation);
				AimPitch = NewPivotRotation.Pitch;
			}
		}
	}

	if (FireTimer > 0.f)
	{
		return;
	}

	if (FireAtTarget(CurrentTarget))
	{
		FireTimer = FMath::Max(WeaponSettings.FireInterval, 0.01f);
	}
}

APawn* ADRTurret::FindNearestEnemy() const
{
	UWorld* World = GetWorld();
	const int32 CurrentTeamId = GetCurrentOwnerTeamId();
	if (!IsValid(World) || !WeaponSettings.ProjectileClass)
	{
		return nullptr;
	}

	APawn* NearestPawn = nullptr;
	float NearestDistanceSquared = FMath::Square(WeaponSettings.MaxAttackDistance);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsValid(Candidate)
			|| DRCombatTeam::GetActorTeamId(Candidate) == INDEX_NONE
			|| DRCombatTeam::IsFriendlyTarget(CurrentTeamId, Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRTurretLineOfSight), false);
			QueryParams.AddIgnoredActor(this);
			if (IsValid(InstallerPlayerState) && IsValid(InstallerPlayerState->GetPawn()))
			{
				QueryParams.AddIgnoredActor(InstallerPlayerState->GetPawn());
			}

			FHitResult VisibilityHit;
			const FVector TraceStart = GetActorLocation() + FVector::UpVector * 40.f;
			const bool IsBlocked = World->LineTraceSingleByChannel(
				VisibilityHit,
				TraceStart,
				Candidate->GetActorLocation(),
				ECC_Visibility,
				QueryParams);
			if (IsBlocked && VisibilityHit.GetActor() != Candidate)
			{
				continue;
			}

			NearestDistanceSquared = DistanceSquared;
			NearestPawn = Candidate;
		}
	}

	return NearestPawn;
}

bool ADRTurret::FireAtTarget(APawn* TargetPawn)
{
	if (!IsValid(TargetPawn) || !WeaponSettings.ProjectileClass
		|| !IsValid(OwnerAbilitySystemComponent))
	{
		return false;
	}

	const FVector SpawnLocation = IsValid(TurretMuzzle)
		? TurretMuzzle->GetComponentLocation()
		: GetActorLocation() + FVector::UpVector * 40.f;
	FVector LaunchVelocity;
	if (!ResolveProjectileLaunchVelocity(
		SpawnLocation,
		TargetPawn->GetActorLocation(),
		LaunchVelocity))
	{
		return false;
	}

	const FVector LaunchDirection = LaunchVelocity.GetSafeNormal();
	const float AimError = FMath::Abs(FMath::FindDeltaAngleDegrees(
		GetActorRotation().Yaw,
		LaunchDirection.Rotation().Yaw));
	if (AimError > WeaponSettings.AimToleranceDegrees)
	{
		return false;
	}

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);
	FDRProjectileWorldImpactData WorldImpactData;
	WorldImpactData = WeaponSettings.WorldImpactData;

	const FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);
	ADRTurret* SourceActor = this;
	ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
		WeaponSettings.ProjectileClass,
		SpawnTransform,
		SourceActor,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Projectile))
	{
		return false;
	}

	Projectile->InitializeProjectile(
		OwnerAbilitySystemComponent,
		ImpactEffectSpecs,
		WeaponSettings.BreakableDamage,
		WorldImpactData,
		GetCurrentOwnerTeamId(),
		WeaponSettings.ProjectilePresentationDefinition,
		WeaponSettings.MaxAttackDistance,
		WeaponSettings.FalloffSettings);
	Projectile->SetInitialLaunchVelocity(LaunchVelocity);
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	MulticastPlayFirePresentation(
		WeaponSettings.ProjectilePresentationDefinition,
		SpawnLocation,
		LaunchDirection.Rotation());
	return true;
}

void ADRTurret::OnRep_AimPitch()
{
	if (IsValid(TurretAimPivot))
	{
		TurretAimPivot->SetRelativeRotation(FRotator(AimPitch, 0.f, 0.f));
	}
}

void ADRTurret::MulticastPlayFirePresentation_Implementation(
	UDRProjectileWeaponItemDefinition* PresentationDefinition,
	const FVector_NetQuantize FireLocation,
	const FRotator FireRotation)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(PresentationDefinition))
	{
		return;
	}

	const FDRWeaponPresentationData& Presentation =
		PresentationDefinition->FirePresentation;
	if (IsValid(Presentation.VFX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			Presentation.VFX,
			FireLocation,
			FireRotation);
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = FireLocation;
	Parameters.Instigator = this;
	Parameters.EffectCauser = this;
	Parameters.SourceObject = PresentationDefinition;
	UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(
		this,
		Presentation.SoundCueTag,
		Parameters);
}

bool ADRTurret::ResolveProjectileLaunchVelocity(
	const FVector& SpawnLocation,
	const FVector& AimPoint,
	FVector& OutLaunchVelocity) const
{
	OutLaunchVelocity = FVector::ZeroVector;
	if (!WeaponSettings.ProjectileClass)
	{
		return false;
	}

	const ADRProjectile* ProjectileDefault =
		WeaponSettings.ProjectileClass->GetDefaultObject<ADRProjectile>();
	UWorld* World = GetWorld();
	if (!IsValid(ProjectileDefault) || !IsValid(World))
	{
		return false;
	}

	const float LaunchSpeed = FMath::Max(
		ProjectileDefault->GetConfiguredInitialSpeed(),
		1.f);
	const float GravityScale = FMath::Max(
		ProjectileDefault->GetConfiguredGravityScale(),
		0.f);
	const FVector DirectDirection = (AimPoint - SpawnLocation).GetSafeNormal();
	if (DirectDirection.IsNearlyZero())
	{
		return false;
	}

	if (GravityScale <= KINDA_SMALL_NUMBER)
	{
		OutLaunchVelocity = DirectDirection * LaunchSpeed;
		return true;
	}

	UGameplayStatics::FSuggestProjectileVelocityParameters Parameters(
		this,
		SpawnLocation,
		AimPoint,
		LaunchSpeed);
	Parameters.bFavorHighArc = false;
	Parameters.CollisionRadius = 0.f;
	Parameters.OverrideGravityZ = World->GetGravityZ() * GravityScale;
	Parameters.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
	Parameters.bDrawDebug = false;
	Parameters.bAcceptClosestOnNoSolutions = true;

	if (UGameplayStatics::SuggestProjectileVelocity(Parameters, OutLaunchVelocity)
		&& !OutLaunchVelocity.IsNearlyZero())
	{
		return true;
	}

	OutLaunchVelocity = DirectDirection * LaunchSpeed;
	return true;
}

int32 ADRTurret::GetCurrentOwnerTeamId() const
{
	return IsValid(InstallerPlayerState)
		? InstallerPlayerState->GetTeamId()
		: OwnerTeamId;
}

void ADRTurret::BuildImpactEffectSpecs(
	TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();
	if (!IsValid(OwnerAbilitySystemComponent))
	{
		return;
	}

	const float DamageMultiplier = OwnerAbilitySystemComponent->GetNumericAttribute(
		UDRPlayerAttributeSet::GetWeaponDamageMultiplierAttribute());
	for (const FDRGameplayEffectData& EffectData : WeaponSettings.ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext =
			OwnerAbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		FGameplayEffectSpecHandle EffectSpec =
			OwnerAbilitySystemComponent->MakeOutgoingSpec(
				EffectData.EffectClass,
				EffectData.EffectLevel,
				EffectContext);
		if (!EffectSpec.IsValid())
		{
			continue;
		}

		for (const TPair<FGameplayTag, float>& Pair : EffectData.SetByCallerMagnitudes)
		{
			const float Magnitude = Pair.Key == DRGameplayTags::Data_Damage
				? Pair.Value * DamageMultiplier
				: Pair.Value;
			EffectSpec.Data->SetSetByCallerMagnitude(Pair.Key, Magnitude);
		}
		OutEffectSpecs.Add(EffectSpec);
	}
}

void ADRTurret::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		ApplyOwnerCooldown();
	}

	Super::EndPlay(EndPlayReason);
}

void ADRTurret::ApplyBrokenPresentation()
{
	Super::ApplyBrokenPresentation();

	SetActorTickEnabled(false);
	CurrentTarget = nullptr;

	TurretHolderMesh->SetVisibility(false, true);
	TurretMesh->SetVisibility(false, true);
	TurretTankMesh->SetVisibility(false, true);
}

void ADRTurret::ApplyTeamMaterial()
{
	UMaterialInterface* TeamMaterial = OwnerTeamId == 0 ? RedTeamMaterial.Get() : BlueTeamMaterial.Get();
	if ((OwnerTeamId != 0 && OwnerTeamId != 1) || !IsValid(TeamMaterial))
	{
		return;
	}

	TurretHolderMesh->SetMaterial(0, TeamMaterial);
	TurretMesh->SetMaterial(0, TeamMaterial);
	TurretTankMesh->SetMaterial(0, TeamMaterial);
}

void ADRTurret::OnRep_OwnerTeamId()
{
	ApplyTeamMaterial();
}

bool ADRTurret::CanReceiveDamageFrom(const AController* EventInstigator) const
{
	const ADRPlayerState* AttackerPlayerState = IsValid(EventInstigator)
		? EventInstigator->GetPlayerState<ADRPlayerState>()
		: nullptr;
	if (!IsValid(AttackerPlayerState) || OwnerTeamId == INDEX_NONE)
	{
		return false;
	}

	return AttackerPlayerState->GetTeamId() != OwnerTeamId;
}

void ADRTurret::ApplyOwnerCooldown() const
{
	if (!IsValid(OwnerAbilitySystemComponent) || !CooldownTag.IsValid() || CooldownDuration <= 0.f)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = OwnerAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle CooldownSpec = OwnerAbilitySystemComponent->MakeOutgoingSpec(
		UDRGE_SkillCooldown::StaticClass(), 1.f, EffectContext);
	if (!CooldownSpec.IsValid())
	{
		return;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Cooldown_Duration, CooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AddTag(CooldownTag);
	OwnerAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
}

void ADRTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, OwnerTeamId);
	DOREPLIFETIME(ThisClass, InstallerPlayerState);
	DOREPLIFETIME(ThisClass, AimPitch);
}
