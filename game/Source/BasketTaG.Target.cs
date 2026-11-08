using UnrealBuildTool;

public class BasketTaGTarget : TargetRules
{
	public BasketTaGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BasketTaG");
	}
}
