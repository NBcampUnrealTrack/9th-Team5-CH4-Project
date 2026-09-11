using UnrealBuildTool;

public class RoomService : ModuleRules
{
	public RoomService(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "DeveloperSettings", "HTTP", "Json"
		});
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"HTTPServer", "Sockets"
		});
	}
}
