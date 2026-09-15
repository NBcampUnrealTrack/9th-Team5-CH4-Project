using UnrealBuildTool;

public class DeepRaidersEditor : ModuleRules
{
	public DeepRaidersEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"DeepRaiders",
			"GoogleSheetLoader"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"UnrealEd"
		});
	}
}
