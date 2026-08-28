
#include "DRThrowActionTypes.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

FVector DRThrow::ResolveLaunchLocation(const AActor* AvatarActor, const FDRThrowActionSettings& Settings,
	const FVector& AimDirection)
{
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}
	
	const ACharacter* Character = Cast<ACharacter>(AvatarActor);
	const USkeletalMeshComponent* CharacterMesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	
	if (IsValid(CharacterMesh)
		&& CharacterMesh->DoesSocketExist(Settings.ThrowSocketName))
	{
		return CharacterMesh->GetSocketLocation(Settings.ThrowSocketName);
	}
	
	FVector Forward = AimDirection.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = AvatarActor->GetActorForwardVector();
	}
	
	return AvatarActor->GetActorLocation() + Forward * Settings.FallbackForwardOffset 
		+ AvatarActor->GetActorRightVector() * Settings.FallbackRightOffset + FVector::UpVector * Settings.FallbackHeightOffset;
	
}
