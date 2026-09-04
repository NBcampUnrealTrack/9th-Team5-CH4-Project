// Fill out your copyright notice in the Description page of Project Settings.


#include "DRItemInstance.h"
#include "DRProjectileWeaponDefinition.h"

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
	
	// 현재 인스턴스화가 필요한 아이템은 ProjectileWeapon이 유일하다.
	const UDRProjectileWeaponItemDefinition* ProjectileDefinition = Cast<UDRProjectileWeaponItemDefinition>(Definition);

	if (!IsValid(ProjectileDefinition))
	{
		return Result;
	}

	if (ProjectileDefinition->ResourceType == EDRProjectileWeaponResourceType::SnowGauge)
	{
		Result.RuntimeState.InitializeAs<FDRSnowProjectileWeaponRuntimeState>();
	}
	else if (ProjectileDefinition->ResourceType == EDRProjectileWeaponResourceType::InstanceAmmo)
	{
		Result.RuntimeState.InitializeAs<FDRProjectileWeaponRuntimeState>();
		
		FDRProjectileWeaponRuntimeState* State = Result.RuntimeState.GetMutablePtr<FDRProjectileWeaponRuntimeState>();
		
		check(State);
		State->CurrentAmmo = FMath::Max(0, ProjectileDefinition->InitialAmmo);
	}
	
	return Result;
}
