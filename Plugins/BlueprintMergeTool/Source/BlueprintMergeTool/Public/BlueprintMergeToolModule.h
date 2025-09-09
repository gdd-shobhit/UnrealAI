#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Docking/SDockTab.h"

class FToolBarBuilder;
class FMenuBuilder;

/**
 * Blueprint Merge Tool Module
 * Provides advanced Blueprint merging and diffing capabilities with three-way merge support
 */
class FBlueprintMergeToolModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command */
	void PluginButtonClicked();

private:
	void RegisterMenus();
	
	/** Creates the main merge tool window */
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};

