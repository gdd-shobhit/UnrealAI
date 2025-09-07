using UnrealBuildTool;

public class UnrealAI : ModuleRules
{
	public UnrealAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add other public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"PropertyEditor",
				"BlueprintGraph",
				"KismetCompiler",
				"GraphEditor",
				"ContentBrowser",
				"AssetRegistry",
				"EditorSubsystem",
				"ToolMenus",
				"HTTP",
				"Json",
				"JsonUtilities",
				"UMG",
				"UMGEditor",
				"Settings",
				"SettingsEditor",

			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"PropertyEditor",
				"BlueprintGraph",
				"KismetCompiler",
				"GraphEditor",
				"ContentBrowser",
				"AssetRegistry",
				"EditorSubsystem",
				"ToolMenus",
				"HTTP",
				"Json",
				"JsonUtilities",
				"UMG",
				"UMGEditor",
				"Settings",
				"SettingsEditor"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
