#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FUnrealAICommands : public TCommands<FUnrealAICommands>
{
public:

	FUnrealAICommands()
		: TCommands<FUnrealAICommands>(TEXT("UnrealAI"), NSLOCTEXT("Contexts", "UnrealAI", "UnrealAI Plugin"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

	TSharedPtr< FUICommandInfo > PluginAction;
};
