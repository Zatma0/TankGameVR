using UnrealBuildTool;

public class TankGameVR : ModuleRules
{
	public TankGameVR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"HeadMountedDisplay"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"XRBase",
			"SlateCore"
		});

		// Editor-only helpers used by the content setup scripts (Testing/TankEditorUtils).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMGEditor" });
		}
	}
}
