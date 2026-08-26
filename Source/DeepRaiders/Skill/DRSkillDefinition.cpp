#include "DRSkillDefinition.h"

UDRSkillDefinition::UDRSkillDefinition()
{
	Category = EDRItemCategory::Skill;
	MaxStackSize = 1;
	bCanBeDropped = false;
	bCanBeSold = false;
}
