// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DeepRaiders : ModuleRules
{
	public DeepRaiders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore",
			
			// Online
			"OnlineSubsystem", "OnlineSubsystemUtils", "Sockets",
			
			// Voxel
			"Voxel",
			
			// GAS
			"GameplayAbilities", "GameplayTags", "GameplayTasks", "ModelViewViewModel",
			
			// Struct
			"StructUtils",
			
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CableComponent", "NetCore", "DeveloperSettings", "PhysicsCore", "Chaos",
			// FConvex/TBox 인라인 메서드가 ChaosCore의 TAABB 내보내기 심볼을 직접 참조한다.
			"ChaosCore"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
