using UnrealBuildTool;

public class SinkMonitor : ModuleRules
{
	public SinkMonitor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject",
			"Engine",
			"DeveloperSettings",
			"PhysicsCore",
			"InputCore",
			"AssetRegistry",
			"SlateCore", "Slate", "UMG",
			"RenderCore",
			"NetCore",
			"GameplayTags",
			"Json", "JsonUtilities",
			"DummyHeaders",
			"FactoryGame",
			"SML"
		});

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PublicDependencyModuleNames.Add("AnimGraph");
		}
	}
}
