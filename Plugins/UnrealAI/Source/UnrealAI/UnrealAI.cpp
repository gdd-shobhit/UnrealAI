#include "UnrealAI.h"
#include "UnrealAICommands.h"
#include "UnrealToolRegistry.h"
#include "UnrealBridgeClient.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "SlateBasics.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FUnrealAIModule"

void FUnrealAIModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FUnrealToolRegistry::RegisterBuiltInTools();
	FUnrealAICommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FUnrealAICommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FUnrealAIModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealAIModule::RegisterMenus));
	
	ProviderOptions.Add(MakeShareable(new FString(TEXT("Local LLM"))));
	ProviderOptions.Add(MakeShareable(new FString(TEXT("Claude API"))));
	ProviderOptions.Add(MakeShareable(new FString(TEXT("OpenAI API"))));
}

void FUnrealAIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FUnrealAICommands::Unregister();
}

void FUnrealAIModule::PluginButtonClicked()
{
	TSharedPtr<FString> CurrentInputText = MakeShareable(new FString());
	TSharedPtr<FString> CurrentLogText = MakeShareable(new FString(TEXT("Activity log (Cursor-style). Enter a prompt and click Send. Start the Python MCP bridge (see Plugins/UnrealAI/Scripts/) then connect.\n")));
	TSharedPtr<FString> SelectedProvider = MakeShareable(new FString(TEXT("Local LLM")));

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("UnrealAI (MCP)")))
		.ClientSize(FVector2D(800, 600))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.SizingRule(ESizingRule::UserSized);

	TSharedRef<SWidget> Content = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("UnrealAI (MCP)")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
			.ColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20, 0, 20, 10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Prompt:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text_Lambda([CurrentInputText]() { return FText::FromString(*CurrentInputText); })
				.HintText(FText::FromString(TEXT("What should the AI do? (Uses MCP tools: create_blueprint, create_node, link_nodes, create_component)")))
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
				.MinDesiredWidth(400.0f)
				.OnTextChanged_Lambda([CurrentInputText](const FText& NewText) { *CurrentInputText = NewText.ToString(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20, 0, 20, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Provider:"))).Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ProviderOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
				.OnSelectionChanged_Lambda([SelectedProvider](TSharedPtr<FString> NewSelection, ESelectInfo::Type) { if (NewSelection.IsValid()) *SelectedProvider = *NewSelection; })
				.Content()
				[
					SNew(STextBlock).Text_Lambda([SelectedProvider]() { return FText::FromString(*SelectedProvider); })
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20, 0, 20, 15)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Send")))
				.OnClicked_Lambda([CurrentInputText, CurrentLogText, SelectedProvider]()
				{
					if (CurrentInputText->IsEmpty())
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please enter a prompt first.")));
						return FReply::Handled();
					}
					UUnrealBridgeClient* Client = NewObject<UUnrealBridgeClient>();
					*CurrentLogText += FString::Printf(TEXT("[Send] Prompt: %s | Provider: %s\n"), **CurrentInputText, **SelectedProvider);
					Client->SendRunPrompt(*CurrentInputText, *SelectedProvider, [CurrentLogText, Client](bool bSuccess, const FString& SessionId)
					{
						if (!bSuccess)
						{
							*CurrentLogText += TEXT("[Error] Failed to start run. Is the bridge running? (Plugins/UnrealAI/Scripts/)\n");
							return;
						}
						*CurrentLogText += FString::Printf(TEXT("[Run] Session: %s\n"), *SessionId);
						TSharedPtr<TFunction<void()>> DoPoll = MakeShareable(new TFunction<void()>());
						*DoPoll = [CurrentLogText, Client, SessionId, DoPoll]()
						{
							Client->Poll(SessionId, [CurrentLogText, Client, SessionId, DoPoll](const FBridgePollResult& R)
							{
								for (const FString& L : R.LogLines)
									*CurrentLogText += L + TEXT("\n");
								if (!R.Error.IsEmpty())
									*CurrentLogText += FString::Printf(TEXT("[Error] %s\n"), *R.Error);
								if (!R.ToolCallId.IsEmpty() && !R.ToolName.IsEmpty())
								{
									FString ToolResult;
									FUnrealToolRegistry::ExecuteTool(FName(*R.ToolName), R.ToolArgsJson, ToolResult);
									*CurrentLogText += FString::Printf(TEXT("[Tool] %s -> %s\n"), *R.ToolName, *ToolResult);
									Client->SendToolResult(SessionId, R.ToolCallId, ToolResult, [DoPoll](bool)
									{
										(*DoPoll)();
									});
									return;
								}
								if (!R.bDone)
								{
									(*DoPoll)();
									return;
								}
								*CurrentLogText += TEXT("[Done]\n");
							});
						};
						(*DoPoll)();
					});
					return FReply::Handled();
				})
			]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20, 0, 20, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Clear")))
				.OnClicked_Lambda([CurrentLogText]()
				{
					*CurrentLogText = TEXT("Activity log (Cursor-style). Enter a prompt and click Send. Start the Python MCP bridge (see Plugins/UnrealAI/Scripts/) then connect.\n");
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(20, 0, 20, 10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Activity log:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(10.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([CurrentLogText]() -> FText
					{
						return FText::FromString(*CurrentLogText);
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
					.AutoWrapText(true)
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
				]
			]
		];

	Window->SetContent(Content);

	// Show the window
	FSlateApplication::Get().AddWindow(Window);
}

void FUnrealAIModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FUnrealAICommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FUnrealAICommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealAIModule, UnrealAI)
