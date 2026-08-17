#include "DRGameplayTags.h"

namespace DRGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Frozen,
		"State.Frozen",
		"Player is frozen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Dead,
		"State.Dead",
		"Player is dead.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Absorbing,
		"State.Absorbing",
		"Player is absorbing snow.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack,
		"Ability.Attack",
		"Parent tag for attack abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Ranged,
		"Ability.Attack.Ranged",
		"Ranged attack ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Melee,
		"Ability.Attack.Melee",
		"Melee attack ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Snow_Absorb,
		"Ability.Snow.Absorb",
		"Snow absorption ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Snow_Amount,
		"Data.Snow.Amount",
		"Snow amount passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Freeze_Amount,
		"Data.Freeze.Amount",
		"Freeze amount passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Damage,
		"Data.Damage",
		"Damage amount passed through GAS.");
}