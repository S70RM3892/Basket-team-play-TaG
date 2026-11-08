using UnrealBuildTool;

public class BasketTaG : ModuleRules
{
	public BasketTaG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// The UI files each keep their own file-local helpers (White, T, GetGS...): never merge them into one unity blob
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"Sockets",
			"AssetRegistry"
		});

		PublicIncludePaths.Add(ModuleDirectory);
	}
}
