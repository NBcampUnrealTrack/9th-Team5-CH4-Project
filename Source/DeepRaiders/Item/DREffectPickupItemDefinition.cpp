
#include "DREffectPickupItemDefinition.h"

#include "DREffectPickupWorldItemActor.h"

UDREffectPickupItemDefinition::UDREffectPickupItemDefinition()
{
	Category = EDRItemCategory::Consumable;
	MaxStackSize = 1;
	bCanBeDropped = false;
	bCanBeSold = false;
	ActorClass = ADREffectPickupWorldItemActor::StaticClass();
}
