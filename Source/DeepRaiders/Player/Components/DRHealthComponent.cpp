#include "DRHealthComponent.h"

#include "Net/UnrealNetwork.h"

UDRHealthComponent::UDRHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UDRHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor =
		GetOwner();

	if (!IsValid(OwnerActor))
	{
		return;
	}

	/*
	 * 실제 초기 체력 결정은 서버.
	 */
	if (OwnerActor->HasAuthority())
	{
		CurrentHealth = MaxHealth;
	}

	LastObservedHealth =
		CurrentHealth;
}

void UDRHealthComponent::
GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>&
		OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(
		OutLifetimeProps);

	DOREPLIFETIME(
		UDRHealthComponent,
		CurrentHealth);
}
float UDRHealthComponent::ApplyDamage(
	float DamageAmount)
{
	AActor* OwnerActor =
		GetOwner();

	if (!IsValid(OwnerActor) ||
		!OwnerActor->HasAuthority() ||
		DamageAmount <= 0.f ||
		IsDead())
	{
		return 0.f;
	}

	const float OldHealth =
		CurrentHealth;

	const float AppliedDamage =
		FMath::Min(
			DamageAmount,
			CurrentHealth);

	CurrentHealth =
		FMath::Clamp(
			CurrentHealth - AppliedDamage,
			0.f,
			MaxHealth);

	LastObservedHealth =
		CurrentHealth;

	OnHealthChanged.Broadcast(
		OldHealth,
		CurrentHealth);

	if (IsDead())
	{
		OnHealthDepleted.Broadcast();
	}

	OwnerActor->ForceNetUpdate();

	return AppliedDamage;
}

void UDRHealthComponent::OnRep_CurrentHealth()
{
	const float OldHealth =
		LastObservedHealth;

	LastObservedHealth =
		CurrentHealth;

	OnHealthChanged.Broadcast(
		OldHealth,
		CurrentHealth);

	if (IsDead())
	{
		OnHealthDepleted.Broadcast();
	}
}