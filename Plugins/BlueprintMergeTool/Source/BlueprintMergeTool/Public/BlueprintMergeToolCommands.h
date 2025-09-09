#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/**
 * Commands for Blueprint Merge Tool
 */
class FBlueprintMergeToolCommands : public TCommands<FBlueprintMergeToolCommands>
{
public:
	FBlueprintMergeToolCommands()
		: TCommands<FBlueprintMergeToolCommands>(TEXT("BlueprintMergeTool"), NSLOCTEXT("Contexts", "BlueprintMergeTool", "Blueprint Merge Tool Plugin"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> OpenPluginWindow;
};
