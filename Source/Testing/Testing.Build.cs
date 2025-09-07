// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Testing : ModuleRules
{
	public Testing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Add editor-specific modules when building for editor
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"BlueprintGraph", 
				"UnrealEd", 
				"KismetCompiler", 
				"AssetRegistry"
			});
		}
	}
}
