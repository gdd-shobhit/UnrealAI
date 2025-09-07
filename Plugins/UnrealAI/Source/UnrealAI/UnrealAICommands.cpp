#include "UnrealAICommands.h"

#define LOCTEXT_NAMESPACE "FUnrealAIModule"

void FUnrealAICommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "UnrealAI", "Execute UnrealAI action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
