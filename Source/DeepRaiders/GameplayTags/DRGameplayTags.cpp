#include "DRGameplayTags.h"

namespace DRGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Perk_Skill_Grab_Debuff, "Perk.Skill.Grab.Debuff");
	UE_DEFINE_GAMEPLAY_TAG(Perk_Skill_Grab_Enhancement, "Perk.Skill.Grab.Enhancement");
	UE_DEFINE_GAMEPLAY_TAG(Data_Perk_Grab_SnowReduction, "Data.Perk.Grab.SnowReduction");
	UE_DEFINE_GAMEPLAY_TAG(Data_Perk_Grab_RangeBonus, "Data.Perk.Grab.RangeBonus");
	UE_DEFINE_GAMEPLAY_TAG(Data_Perk_Grab_HitScaleBonus, "Data.Perk.Grab.HitScaleBonus");
	UE_DEFINE_GAMEPLAY_TAG(
		Perk_Skill_Barrier_PortableGenerator,
		"Perk.Skill.Barrier.PortableGenerator");
	UE_DEFINE_GAMEPLAY_TAG(
		Perk_Skill_Barrier_TeslaField,
		"Perk.Skill.Barrier.TeslaField");
	UE_DEFINE_GAMEPLAY_TAG(
		Perk_Skill_SuperJump_RocketBoots,
		"Perk.Skill.SuperJump.RocketBoots");
	UE_DEFINE_GAMEPLAY_TAG(
		Perk_Skill_SuperJump_HeroLanding,
		"Perk.Skill.SuperJump.HeroLanding");
	UE_DEFINE_GAMEPLAY_TAG(
		Data_Perk_SuperJump_RocketBoots_MoveSpeed,
		"Data.Perk.SuperJump.RocketBoots.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(
		Character_Upgrade_MaxHealth, 
		"Character.Upgrade.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(
		Character_Upgrade_MoveSpeed, 
		"Character.Upgrade.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(
		Data_Upgrade_MaxHealth, 
		"Data.Upgrade.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(
		Data_Upgrade_MoveSpeed, 
		"Data.Upgrade.MoveSpeed");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Frozen,
		"State.Frozen",
		"Player is frozen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_VoxelContained,
		"State.VoxelContained",
		"Player is fully contained by voxel terrain.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Dead,
		"State.Dead",
		"Player is dead.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Absorbing,
		"State.Absorbing",
		"Player is absorbing snow.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Overheated,
		"State.Overheated",
		"Player ranged weapon system is overheated.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_BlinkRecovery,
		"State.BlinkRecovery",
		"Player may move after blinking but cannot attack or use skills.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_PersonalShield,
		"State.PersonalShield",
		"Player currently has a consumable personal shield.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_RespawnInvincible,
		"State.RespawnInvincible",
		"Player is temporarily invincible after a death respawn.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_UI_InventoryOpen,
		"State.UI.InventoryOpen",
		"Local player's inventory UI is open.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_UI_ShopOpen,
		"State.UI.ShopOpen",
		"Local player's shop UI is open.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_UI_TeleportOpen,
		"State.UI.TeleportOpen",
		"Local player's teleport UI is open.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_MovementAction_Active,
		"State.MovementAction.Active",
		"Player is currently executing a movement action.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_MovementAction_Zipline,
		"State.MovementAction.Zipline",
		"Player is currently riding a zipline.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_QuickSlot_ActivationInterval,
		"State.QuickSlot.ActivationInterval",
		"Selected quick-slot item cannot be activated yet.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Stealthed,
		"State.Stealthed",
		"Player is currently stealthed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Aiming_Throw,
		"State.Aiming.Throw",
		"Player is aiming a throwable action.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_GamePreparing,
		"State.GamePreparing",
		"Player actions are blocked while the game is preparing.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Root,
		"Ability",
		"Root tag for all gameplay abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action,
		"Ability.Action",
		"Additive tag for player-driven abilities that may be blocked by action-locking states.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack,
		"Ability.Attack",
		"Parent tag for attack abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Ranged,
		"Ability.Attack.Ranged",
		"Ranged attack ability.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Projectile_Fire,
		"GameplayCue.Weapon.Projectile.Fire",
		"Data-driven projectile weapon fire presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Projectile_Impact,
		"GameplayCue.Weapon.Projectile.Impact",
		"Data-driven projectile weapon impact presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Sprayer_Active,
		"GameplayCue.Weapon.Sprayer.Active",
		"Data-driven active sprayer presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Absorb_Active,
		"GameplayCue.Weapon.Absorb.Active",
		"Persistent snow absorb beam and sound presentation.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Item_Throwable_Throw,
		"GameplayCue.Item.Throwable.Throw",
		"Throwable release presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Item_Throwable_Impact,
		"GameplayCue.Item.Throwable.Impact",
		"Throwable impact presentation.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Melee,
		"Ability.Attack.Melee",
		"Melee attack ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Snow_Absorb,
		"Ability.Snow.Absorb",
		"Snow absorption ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Throw,
		"Ability.Throw",
		"Active throwable action regardless of its source.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Item_Throw,
		"Ability.Item.Throw",
		"Item-based throwable ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill,
		"Ability.Skill",
		"Parent tag for character skills.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Blink,
		"Ability.Skill.Blink",
		"Blink skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_ForwardDash,
		"Ability.Skill.ForwardDash",
		"Forward dash skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_SuperJump,
		"Ability.Skill.SuperJump",
		"Super jump skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Perk_SuperJump_RocketBoots,
		"Ability.Perk.SuperJump.RocketBoots",
		"Horizontal air dash granted by the Super Jump rocket boots perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_CombatRoll,
		"Ability.Skill.CombatRoll",
		"Combat roll skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Search,
		"Ability.Skill.Search",
		"Search skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_HotPack,
		"Ability.Skill.HotPack",
		"Hot pack skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_SpearThrow,
		"Ability.Skill.SpearThrow",
		"Spear throw skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_SlowProjectile,
		"Ability.Skill.SlowProjectile",
		"Slow projectile skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_SnowWall,
		"Ability.Skill.SnowWall",
		"Snow wall skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_IceWall,
		"Ability.Skill.IceWall",
		"Ice wall skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Grab,
		"Ability.Skill.Grab",
		"Grab skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Grapple,
		"Ability.Skill.Grapple",
		"Grapple skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Barrier,
		"Ability.Skill.Barrier",
		"Deployable barrier skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Turret,
		"Ability.Skill.Turret",
		"Deployable turret skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_Charges,
		"Perk.Skill.Charges",
		"Changes the equipped skill to use rechargeable charges.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_Search_TeamShare,
		"Perk.Skill.Search.TeamShare",
		"Shares the search skill reveal with teammates.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_HotPack_InstantCare,
		"Perk.Skill.HotPack.InstantCare",
		"Changes Hot Pack into a throwable instant recovery skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_SlowProjectile_ExtremeSlow,
		"Perk.Skill.SlowProjectile.ExtremeSlow",
		"Strengthens the slow projectile movement speed reduction.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_SlowProjectile_AllySpeed,
		"Perk.Skill.SlowProjectile.AllySpeed",
		"Grants movement speed to allies in the impact radius.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_Turret_StatBoost,
		"Perk.Skill.Turret.StatBoost",
		"Increases turret damage, fire rate, and attack range.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_Turret_CannonProjectile,
		"Perk.Skill.Turret.CannonProjectile",
		"Changes the turret projectile to a cannon projectile.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle,
		"Weapon.Upgrade.Rifle",
		"Parent tag for rifle stat upgrade tracks.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle_Damage,
		"Weapon.Upgrade.Rifle.Damage",
		"Rifle damage upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle_FireInterval,
		"Weapon.Upgrade.Rifle.FireInterval",
		"Rifle fire interval upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle_SnowCost,
		"Weapon.Upgrade.Rifle.SnowCost",
		"Rifle snow cost upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle_HeatGeneration,
		"Weapon.Upgrade.Rifle.HeatGeneration",
		"Rifle heat generation upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle_SnowAbsorbPower,
		"Weapon.Upgrade.Rifle.SnowAbsorbPower",
		"Rifle snow absorb power upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Rifle_SnowAddAmount,
		"Weapon.Upgrade.Rifle.SnowAddAmount",
		"Rifle snow add amount upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun,
		"Weapon.Upgrade.Shotgun",
		"Parent tag for shotgun stat upgrade tracks.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_Damage,
		"Weapon.Upgrade.Shotgun.Damage",
		"Shotgun damage upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_FireInterval,
		"Weapon.Upgrade.Shotgun.FireInterval",
		"Shotgun fire interval upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_SnowCost,
		"Weapon.Upgrade.Shotgun.SnowCost",
		"Shotgun snow cost upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_ProjectileCount,
		"Weapon.Upgrade.Shotgun.ProjectileCount",
		"Shotgun projectile count upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_HeatGeneration,
		"Weapon.Upgrade.Shotgun.HeatGeneration",
		"Shotgun heat generation upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_SnowAbsorbPower,
		"Weapon.Upgrade.Shotgun.SnowAbsorbPower",
		"Shotgun snow absorb power upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Shotgun_SnowAddAmount,
		"Weapon.Upgrade.Shotgun.SnowAddAmount",
		"Shotgun snow add amount upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon,
		"Weapon.Upgrade.Cannon",
		"Parent tag for cannon stat upgrade tracks.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon_Damage,
		"Weapon.Upgrade.Cannon.Damage",
		"Cannon damage upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon_FireInterval,
		"Weapon.Upgrade.Cannon.FireInterval",
		"Cannon fire interval upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon_SnowCost,
		"Weapon.Upgrade.Cannon.SnowCost",
		"Cannon snow cost upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon_HeatGeneration,
		"Weapon.Upgrade.Cannon.HeatGeneration",
		"Cannon heat generation upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon_SnowAbsorbPower,
		"Weapon.Upgrade.Cannon.SnowAbsorbPower",
		"Cannon snow absorb power upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Cannon_SnowAddAmount,
		"Weapon.Upgrade.Cannon.SnowAddAmount",
		"Cannon snow add amount upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Sprayer,
		"Weapon.Upgrade.Sprayer",
		"Parent tag for sprayer stat upgrade tracks.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Sprayer_Damage,
		"Weapon.Upgrade.Sprayer.Damage",
		"Sprayer frozen-target damage upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Sprayer_FreezeAmount,
		"Weapon.Upgrade.Sprayer.FreezeAmount",
		"Sprayer freeze gauge application upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Sprayer_SnowCost,
		"Weapon.Upgrade.Sprayer.SnowCost",
		"Sprayer snow cost upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Sprayer_HeatGeneration,
		"Weapon.Upgrade.Sprayer.HeatGeneration",
		"Sprayer heat generation upgrade track.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Weapon_Upgrade_Sprayer_SnowAbsorbPower,
		"Weapon.Upgrade.Sprayer.SnowAbsorbPower",
		"Sprayer snow absorb power upgrade track.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_MovementAction,
		"Ability.MovementAction",
		"Parent tag for movement action abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Item_Grapple,
		"Ability.Item.Grapple",
		"Item-based grappling ability.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Input_SecondaryCancel,
		"Ability.Input.SecondaryCancel",
		"Active movement ability receives Secondary input as a cancel request.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_ActivateFail_Weapon_ResourceEmpty,
		"Ability.ActivateFail.Weapon.ResourceEmpty",
		"Ranged weapon activation or fire request failed because its resource is insufficient.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_Ability_Throw_Release,
		"Event.Ability.Throw.Release",
		"Throwable montage release point.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_MovementAction_Cancel,
		"Event.MovementAction.Cancel",
		"Requests cancellation of the active movement action.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Weapon_Ranged,
		"Cooldown.Weapon.Ranged",
		"Shared fire interval cooldown for ranged weapons.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_One,
		"Cooldown.Skill.One",
		"Cooldown for the first character skill slot.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Two,
		"Cooldown.Skill.Two",
		"Cooldown for the second character skill slot.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Blink,
		"Cooldown.Skill.Blink",
		"Cooldown for the blink skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_ForwardDash,
		"Cooldown.Skill.ForwardDash",
		"Cooldown for the forward dash skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_SuperJump,
		"Cooldown.Skill.SuperJump",
		"Cooldown for the super jump skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_CombatRoll,
		"Cooldown.Skill.CombatRoll",
		"Cooldown for the combat roll skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Search,
		"Cooldown.Skill.Search",
		"Cooldown for the search skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_HotPack,
		"Cooldown.Skill.HotPack",
		"Cooldown for the hot pack skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Grab,
		"Cooldown.Skill.Grab",
		"Cooldown for the grab skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Grapple,
		"Cooldown.Skill.Grapple",
		"Cooldown for the grapple skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_SnowWall,
		"Cooldown.Skill.SnowWall",
		"Cooldown for the snow wall skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_IceWall,
		"Cooldown.Skill.IceWall",
		"Cooldown for the ice wall skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_SpearThrow,
		"Cooldown.Skill.SpearThrow",
		"Cooldown for the spear throw skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_SlowProjectile,
		"Cooldown.Skill.SlowProjectile",
		"Cooldown for the slow projectile skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Barrier,
		"Cooldown.Skill.Barrier",
		"Cooldown for the deployable barrier skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Turret,
		"Cooldown.Skill.Turret",
		"Cooldown for the deployable turret skill.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Effect_Policy_PersistThroughDeath,
		"Effect.Policy.PersistThroghDeath",
		"Active GameplayEffect persists through death and respawn.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX,
		"GameplayCue.VFX",
		"Parent GameplayCue tag for data-driven visual effects");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Effect_Heal,
		"GameplayCue.VFX.Effect.Heal",
		"Heal effect visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Effect_MoveSpeed,
		"GameplayCue.VFX.Effect.MoveSpeed",
		"MoveSpeed effect visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Effect_SpeedSlow,
		"GameplayCue.VFX.Effect.SpeedSlow",
		"MoveSpeed slow effect visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Skill_Blink,
		"GameplayCue.VFX.Skill.Blink",
		"Blink skill visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Skill_ForwardDash,
		"GameplayCue.VFX.Skill.ForwardDash",
		"Persistent forward dash trail visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Skill_SuperJump_HeroLanding,
		"GameplayCue.VFX.Skill.SuperJump.HeroLanding",
		"Super Jump hero landing impact visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Skill_CombatRoll,
		"GameplayCue.VFX.Skill.CombatRoll",
		"Persistent combat roll trail visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Skill_Search,
		"GameplayCue.VFX.Skill.Search",
		"Search skill visual effects");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Skill_Blink_End,
		"GameplayCue.VFX.Skill.BlinkEnd",
		"Blink End effect visual effects");
	
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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Knockback_Distance,
		"Data.Knockback.Distance",
		"Target displacement requested by an instant knockback GameplayEffect.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Health_Heal,
		"Data.Health.Heal",
		"Health Heal amount passed through GAS.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Cooldown_Duration,
		"Data.Cooldown.Duration",
		"Cooldown duration passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_BlinkRecovery_Duration,
		"Data.BlinkRecovery.Duration",
		"Blink recovery duration passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_Charges_Max,
		"Data.Perk.Charges.Max",
		"Maximum skill charges granted by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Shield_Amount,
		"Data.Shield.Amount",
		"Personal shield amount passed through GAS.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Effect_MoveSpeed,
		"Data.Effect.MoveSpeed",
		"Effect MoveSpeed passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Effect_Duration,
		"Data.Effect.Duration",
		"Effect duration passed through GAS.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_QuickSlot_ActivationInterval_Duration,
		"Data.QuickSlot.ActivationInterval.Duration",
		"Quick-slot activation interval duration passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_HotPack_AreaRadius,
		"Data.Perk.HotPack.AreaRadius",
		"Additional radius granted to the hot pack area by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_HotPack_AreaDuration,
		"Data.Perk.HotPack.AreaDuration",
		"Additional duration granted to the hot pack area by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_HotPack_HealthRecovery,
		"Data.Perk.HotPack.HealthRecovery",
		"Additional health recovery granted to the hot pack by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_HotPack_FreezeGaugeRecovery,
		"Data.Perk.HotPack.FreezeGaugeRecovery",
		"Additional freeze gauge recovery granted to the hot pack by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_Turret_DamageBonusRatio,
		"Data.Perk.Turret.DamageBonusRatio",
		"Turret damage bonus ratio granted by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_Turret_FireRateBonusRatio,
		"Data.Perk.Turret.FireRateBonusRatio",
		"Turret fire rate bonus ratio granted by an equipped perk.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_Turret_RangeBonusRatio,
		"Data.Perk.Turret.RangeBonusRatio",
		"Turret attack range bonus ratio granted by an equipped perk.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Perk_Barrier_MaxHealthMultiplier,
		"Data.Perk.Barrier.MaxHealthMultiplier",
		"Additional maximum-health multiplier granted to a barrier by an equipped perk.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_DamageModifier,
		"Data.Weapon.DamageModifier",
		"Equipped weapon damage modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_FireIntervalModifier,
		"Data.Weapon.FireIntervalModifier",
		"Equipped weapon fire interval modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_SnowCostModifier,
		"Data.Weapon.SnowCostModifier",
		"Equipped weapon snow cost modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_ProjectileCountModifier,
		"Data.Weapon.ProjectileCountModifier",
		"Equipped weapon projectile count modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_HeatGenerationModifier,
		"Data.Weapon.HeatGenerationModifier",
		"Equipped weapon heat generation modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_SnowAbsorbPowerModifier,
		"Data.Weapon.SnowAbsorbPowerModifier",
		"Equipped weapon snow absorb power modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_SnowAddAmountModifier,
		"Data.Weapon.SnowAddAmountModifier",
		"Equipped weapon snow add amount modifier passed through SetByCaller.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Weapon_FreezeAmountModifier,
		"Data.Weapon.FreezeAmountModifier",
		"Equipped weapon freeze amount modifier passed through SetByCaller.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_HUD,
		"UI.Screen.HUD",
		"Main HUD screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_QuickSlot,
		"UI.Screen.QuickSlot",
		"Quick slot screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Inventory_Player,
		"UI.Screen.Inventory.Player",
		"Player inventory screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Inventory_Storage,
		"UI.Screen.Inventory.Storage",
		"Storage inventory screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Teleport,
		"UI.Screen.Teleport",
		"Teleport selection screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Shop,
		"UI.Screen.Shop",
		"Shop screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_StartingSelection,
		"UI.Screen.StartingSelection",
		"Starting weapon and skill selection screen.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Scoreboard,
		"UI.Screen.Scoreboard",
		"Match scoreboard screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Menu,
		"UI.Screen.Menu",
		"In-game menu screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Loading,
		"UI.Screen.Loading",
		"Join-in-progress terrain synchronization screen.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Miss,
		"GameplayCue.Sound.Melee.Attack.Miss",
		"Melee attack missed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Hit,
		"GameplayCue.Sound.Melee.Attack.Hit",
		"Melee attack hit.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Kill,
		"GameplayCue.Sound.Melee.Attack.Kill",
		"Melee attack killed the target.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Swing,
		"GameplayCue.Sound.Melee.Attack.Swing",
		"Melee weapon swing.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Projectile_Snow_Impact,
		"GameplayCue.Sound.Projectile.Snow.Impact",
		"Snow projectile impact at the collision location.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Skill_ForwardDash,
		"GameplayCue.Sound.Skill.ForwardDash",
		"Forward dash skill sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Skill_CombatRoll,
		"GameplayCue.Sound.Skill.CombatRoll",
		"Combat roll skill sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Skill_SuperJump,
		"GameplayCue.Sound.Skill.SuperJump",
		"Super jump skill sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Skill_SuperJump_HeroLanding,
		"GameplayCue.Sound.Skill.SuperJump.HeroLanding",
		"Super jump hero landing sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Skill_Turret_Install,
		"GameplayCue.Sound.Skill.Turret.Install",
		"Turret installation sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Snowball_Impact,
		"GameplayCue.Sound.Player.Snowball.Impact",
		"Snow projectile impact on a player.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Hit,
		"GameplayCue.Sound.Player.Hit",
		"Player received a resolved hostile hit.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Hit_LocalFeedback,
		"GameplayCue.Sound.Player.Hit.LocalFeedback",
		"Local 2D feedback for the player who received a hit.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Dig,
		"GameplayCue.Sound.Dig",
		"Digging sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_PickedUp,
		"GameplayCue.Sound.Item.PickedUp",
		"Item picked up.");
		
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
    	GameplayCue_Sound_Item_EffectPickedUp,
    	"GameplayCue.Sound.Item.EffectPickedUp",
    	"Effect Item picked up.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Ore_Dropped,
		"GameplayCue.Sound.Ore.Dropped",
		"Ore dropped.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Ore_Discovered,
		"GameplayCue.Sound.Ore.Discovered",
		"Ore discovered.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Land,
		"GameplayCue.Sound.Player.Land",
		"Player landed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_FallDamage,
		"GameplayCue.Sound.Player.FallDamage",
		"Player took fall damage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_FallDeath,
		"GameplayCue.Sound.Player.FallDeath",
		"Player died from fall damage.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Frozen_Enter,
		"GameplayCue.Sound.Player.Frozen.Enter",
		"Player entered frozen state.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Frozen_Death,
		"GameplayCue.Sound.Player.Frozen.Death",
		"Frozen player died.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Death,
		"GameplayCue.Sound.Player.Death",
		"Local feedback for the player who died.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Kill,
		"GameplayCue.Sound.Player.Kill",
		"Local feedback for the player who killed another player.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Footstep,
		"GameplayCue.Sound.Player.Footstep",
		"Player footstep sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Jump,
		"GameplayCue.Sound.Player.Jump",
		"Player jump takeoff sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_QuickSlot_Switch,
		"GameplayCue.Sound.Player.QuickSlot.Switch",
		"Local feedback when the selected quick slot changes.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_Throwable_Throw,
		"GameplayCue.Sound.Item.Throwable.Throw",
		"Throwable release sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_Throwable_Impact,
		"GameplayCue.Sound.Item.Throwable.Impact",
		"Throwable impact sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_Emergence,
		"GameplayCue.Sound.Item.Emergence",
		"World item emergence started.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_MovementAction_Grapple_Launch,
		"GameplayCue.Sound.MovementAction.Grapple.Launch",
		"Grapple hook launched.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_MovementAction_Grapple_Attach,
		"GameplayCue.Sound.MovementAction.Grapple.Attach",
		"Grapple hook attached to its target.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Breakable_Hit,
		"GameplayCue.Sound.Breakable.Hit",
		"Breakable actor took damage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Breakable_Destroyed,
		"GameplayCue.Sound.Breakable.Destroyed",
		"Breakable actor was destroyed.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Breakable_LootBox_Hit,
		"GameplayCue.Sound.Breakable.LootBox.Hit",
		"Breakable actor took damage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Breakable_LootBox_Destroyed,
		"GameplayCue.Sound.Breakable.LootBox.Destroyed",
		"Breakable actor was destroyed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Breakable_LootBox_Idle,
		"GameplayCue.Sound.Breakable.LootBox.Idle",
		"Looping positional sound emitted by an available loot box.");
	
	// =====================================================
	// Gameplay Cue - Sound - Weapon
	// =====================================================
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Rifle_Fire,
		"GameplayCue.Sound.Weapon.Rifle.Fire",
		"Rifle fire sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Rifle_Impact,
		"GameplayCue.Sound.Weapon.Rifle.Impact",
		"Rifle impact sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Shotgun_Fire,
		"GameplayCue.Sound.Weapon.Shotgun.Fire",
		"Shotgun fire sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Shotgun_Impact,
		"GameplayCue.Sound.Weapon.Shotgun.Impact",
		"Shotgun impact sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Cannon_Fire,
		"GameplayCue.Sound.Weapon.Cannon.Fire",
		"Cannon fire sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Cannon_Impact,
		"GameplayCue.Sound.Weapon.Cannon.Impact",
		"Cannon impact sound.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Sprayer_Active,
		"GameplayCue.Sound.Weapon.Sprayer.Active",
		"Active sprayer loop sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Absorb_Start,
		"GameplayCue.Sound.Weapon.Absorb.Start",
		"Snow absorb start sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Absorb_Loop,
		"GameplayCue.Sound.Weapon.Absorb.Loop",
		"Snow absorb loop sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Absorb_End,
		"GameplayCue.Sound.Weapon.Absorb.End",
		"Snow absorb end sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Absorb_GainPulse,
		"GameplayCue.Sound.Weapon.Absorb.GainPulse",
		"Repeated feedback after enough SnowGauge is gained by snow absorption.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_ResourceEmpty,
		"GameplayCue.Sound.Weapon.ResourceEmpty",
		"Local feedback when a ranged weapon lacks enough resources to fire.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Player_Hit,
		"GameplayCue.Player.Hit",
		"Player hit presentation.");
	
	// Gameplay Cue - Movement Action
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_MovementAction_Grapple_Active,
		"GameplayCue.MovementAction.Grapple.Active",
		"Persistent hook and cable presentation for an active grapple movement action.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_MovementAction_Grapple_Failed,
		"GameplayCue.MovementAction.Grapple.Failed",
		"One-shot cable presentation for a failed grapple attempt.");

	// Gameplay Cue - Skill
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Skill_Grab_Active,
		"GameplayCue.Skill.Grab.Active",
		"Persistent hook and cable presentation for an active grab projectile.");
	
	// Gameplay Cue - Sound - Skill
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	GameplayCue_Sound_Skill_Blink,
	"GameplayCue.Sound.Skill.Blink",
	"Blink skill sound.");
}
