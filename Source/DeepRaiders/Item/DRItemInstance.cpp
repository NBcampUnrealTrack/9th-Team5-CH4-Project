// Fill out your copyright notice in the Description page of Project Settings.


#include "DRItemInstance.h"
#include "DRProjectileWeaponDefinition.h"
#include "DRRangedWeaponDefinition.h"
#include "DeepRaiders/Item/Upgrade/DRWeaponUpgradeProfile.h"

FDRItemInstance DRItemInstanceFactory::Create(UDRItemDefinition* Definition, int32 Quantity /*= 1*/)
{
	FDRItemInstance Result;
	
	if (!IsValid(Definition)
		|| Quantity <= 0)
	{
		return Result;
	}
	
	Result.Definition = Definition;
	Result.InstanceId = FGuid::NewGuid();
	Result.Quantity = FMath::Clamp(Quantity, 1, FMath::Max(1, Definition->MaxStackSize));
	
	// Projectile은 기존 RuntimeState 정책을 그대로 유지한다.
	const UDRProjectileWeaponItemDefinition* ProjectileDefinition =
		Cast<UDRProjectileWeaponItemDefinition>(Definition);

	if (IsValid(ProjectileDefinition))
	{
		if (ProjectileDefinition->ResourceType == EDRProjectileWeaponResourceType::SnowGauge)
		{
			Result.RuntimeState.InitializeAs<FDRSnowProjectileWeaponRuntimeState>();
		}
		else if (ProjectileDefinition->ResourceType == EDRProjectileWeaponResourceType::InstanceAmmo)
		{
			Result.RuntimeState.InitializeAs<FDRProjectileWeaponRuntimeState>();

			FDRProjectileWeaponRuntimeState* State =
				Result.RuntimeState.GetMutablePtr<FDRProjectileWeaponRuntimeState>();

			check(State);
			State->CurrentAmmo = FMath::Max(0, ProjectileDefinition->InitialAmmo);
		}

		return Result;
	}

	// Sprayer처럼 Projectile이 아닌 Ranged Weapon은 Profile이 있을 때만
	// 기존 업그레이드 RuntimeState를 공용으로 사용한다.
	const UDRRangedWeaponDefinition* RangedDefinition = Cast<UDRRangedWeaponDefinition>(Definition);
	if (IsValid(RangedDefinition) && IsValid(RangedDefinition->GetUpgradeProfile()))
	{
		Result.RuntimeState.InitializeAs<FDRSnowProjectileWeaponRuntimeState>();
	}
	
	return Result;
}
