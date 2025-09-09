#include "../Public/BlueprintMergeToolCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintMergeToolModule"

void FBlueprintMergeToolCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Blueprint Merge Tool", "Bring up Blueprint Merge Tool window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
