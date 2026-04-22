#include "../Public/BlueprintMergeToolModule.h"
#include "../Public/BlueprintMergeToolStyle.h"
#include "../Public/BlueprintMergeToolCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "../Public/MergeUI.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

static const FName BlueprintMergeToolTabName("BlueprintMergeTool");

#define LOCTEXT_NAMESPACE "FBlueprintMergeToolModule"

void FBlueprintMergeToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBlueprintMergeToolStyle::Initialize();
	FBlueprintMergeToolStyle::ReloadTextures();

	FBlueprintMergeToolCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintMergeToolCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FBlueprintMergeToolModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintMergeToolModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BlueprintMergeToolTabName, FOnSpawnTab::CreateRaw(this, &FBlueprintMergeToolModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBlueprintMergeToolTabTitle", "Blueprint Merge Tool"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBlueprintMergeToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBlueprintMergeToolStyle::Shutdown();

	FBlueprintMergeToolCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlueprintMergeToolTabName);
}

TSharedRef<SDockTab> FBlueprintMergeToolModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Create the main merge UI widget
			SNew(SMergeUI)
		];
}

void FBlueprintMergeToolModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BlueprintMergeToolTabName);
}

void FBlueprintMergeToolModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FBlueprintMergeToolCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBlueprintMergeToolCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintMergeToolModule, BlueprintMergeTool)
