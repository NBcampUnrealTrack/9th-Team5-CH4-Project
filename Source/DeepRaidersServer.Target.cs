using UnrealBuildTool;

public class DeepRaidersServerTarget : TargetRules
{
	public DeepRaidersServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("DeepRaiders");
	}
}