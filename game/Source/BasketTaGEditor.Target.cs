using UnrealBuildTool;

public class BasketTaGEditorTarget : TargetRules
{
	public BasketTaGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BasketTaG");
	}
}
