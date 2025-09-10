using UnrealBuildTool;

public class BlueprintMergeTool : ModuleRules
{
	public BlueprintMergeTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"EditorWidgets",
				"Slate",
				"SlateCore",
				"InputCore",
				"Json",
				"JsonUtilities",
				"HTTP",
				"BlueprintGraph",
				"KismetCompiler",
				"ToolMenus",
				"WorkspaceMenuStructure",
				"EditorSubsystem",
				"AssetTools",
				"ContentBrowser"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorWidgets",
				"ToolMenus",
				"Slate",
				"SlateCore",
				"AppFramework",
				"EditorSubsystem",
				"GraphEditor",
				"Kismet",
				"KismetWidgets",
				"PropertyEditor",
				"RenderCore",
				"CoreUObject",
				"Engine",
				"TargetPlatform",
				"DeveloperSettings",
				"RHI",
				"UnrealEd",
				"BlueprintGraph",
				"KismetCompiler",
				"SourceControl",
				"AssetRegistry",
				"GameProjectGeneration",
				"DerivedDataCache",
				"Json",
				"JsonUtilities",
				"HTTP",
				"DesktopPlatform"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
