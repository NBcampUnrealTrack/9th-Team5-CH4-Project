#include "DRSkillViewModel.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DeepRaiders/UI/ViewModel/DRSkillSlotViewModel.h"

void UDRSkillViewModel::Initialize(ADRPlayerCharacter* InPlayerCharacter)
{
	Deinitialize();

	if (!IsValid(InPlayerCharacter))
	{
		return;
	}

	if (!IsValid(SkillOne))
	{
		UE_MVVM_SET_PROPERTY_VALUE(
			SkillOne,
			NewObject<UDRSkillSlotViewModel>(this));
	}

	if (!IsValid(SkillTwo))
	{
		UE_MVVM_SET_PROPERTY_VALUE(
			SkillTwo,
			NewObject<UDRSkillSlotViewModel>(this));
	}

	SkillOne->Initialize(
		InPlayerCharacter,
		EDRSkillSlot::One);
	SkillTwo->Initialize(
		InPlayerCharacter,
		EDRSkillSlot::Two);
}

void UDRSkillViewModel::Deinitialize()
{
	if (IsValid(SkillOne))
	{
		SkillOne->Deinitialize();
	}

	if (IsValid(SkillTwo))
	{
		SkillTwo->Deinitialize();
	}
}
