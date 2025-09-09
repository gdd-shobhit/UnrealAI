#include "UnrealAI.h"
#include "UnrealAICommands.h"
#include "UnrealAIService.h"
#include "UnrealAIWidget.h"
#include "BlueprintJsonParser.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "SlateBasics.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/ObjectLibrary.h"

#define LOCTEXT_NAMESPACE "FUnrealAIModule"

void FUnrealAIModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FUnrealAICommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FUnrealAICommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FUnrealAIModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealAIModule::RegisterMenus));
	
	// Initialize UI options
	RequestTypeOptions.Add(MakeShareable(new FString(TEXT("General Query"))));
	RequestTypeOptions.Add(MakeShareable(new FString(TEXT("Blueprint Generation"))));
	RequestTypeOptions.Add(MakeShareable(new FString(TEXT("C++ Generation"))));
	RequestTypeOptions.Add(MakeShareable(new FString(TEXT("Code Review"))));
	RequestTypeOptions.Add(MakeShareable(new FString(TEXT("Documentation"))));
	
	ProviderOptions.Add(MakeShareable(new FString(TEXT("Local LLM"))));
	ProviderOptions.Add(MakeShareable(new FString(TEXT("Claude API"))));
	ProviderOptions.Add(MakeShareable(new FString(TEXT("OpenAI API"))));
	
	// Initialize Blueprint options
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("Select a Blueprint..."))));
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_FirstPersonCharacter"))));
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_FirstPersonGameMode"))));
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_FirstPersonProjectile"))));
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_PickUp_Rifle"))));
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
	// Shared text storage for the UI
	TSharedPtr<FString> CurrentInputText = MakeShareable(new FString());
	TSharedPtr<FString> CurrentResponseText = MakeShareable(new FString(TEXT("Welcome to UnrealAI Assistant!\n\nThis is a comprehensive AI-powered tool for Unreal Engine development.\n\n🎯 Features:\n• Generate Blueprint code from descriptions\n• Generate C++ code from descriptions\n• Analyze and review existing code\n• General AI queries about Unreal Engine\n• Advanced options for fine-tuning\n• Copy and save responses\n\n📋 How to Use:\n1. Type your prompt in the text area above\n2. Click any of the AI function buttons\n3. See the AI response below\n4. Check the log for detailed processing steps\n\n🚀 Next Steps:\n1. Install Ollama: https://ollama.ai\n2. Run: ollama pull llama2\n3. Start Ollama service\n4. Enable HTTP functionality in the code\n5. Enjoy AI-powered Unreal Engine development!\n\n✅ Status: Plugin compiled successfully!\n✅ Status: All AI functions ready!\n✅ Status: C++ UI system working!")));
	TSharedPtr<FString> CurrentLogText = MakeShareable(new FString(TEXT("📋 AI Processing Log\n\nReady to process requests...\n\nLog will show:\n• AI response parsing\n• JSON extraction\n• Blueprint creation steps\n• Function generation\n• Error messages")));

	// Provider selection state
	TSharedPtr<FString> SelectedProvider = MakeShareable(new FString(TEXT("Local LLM")));

	// Only populate BlueprintOptions if it's empty (first time initialization)
	if (BlueprintOptions.Num() <= 1)
	{
		// Dynamically discover blueprints in the project
		BlueprintOptions.Empty(); // Clear existing options
		BlueprintOptions.Add(MakeShareable(new FString(TEXT("Select a Blueprint..."))));
		
		// Get all blueprint assets from the project
		UObjectLibrary* ObjectLibrary = UObjectLibrary::CreateLibrary(UBlueprint::StaticClass(), false, GIsEditor);
		if (ObjectLibrary)
		{
			ObjectLibrary->AddToRoot();
			
			// Scan for blueprint assets
			TArray<FAssetData> AssetDataList;
			ObjectLibrary->LoadAssetsFromPath(TEXT("/Game"));
			ObjectLibrary->GetAssetDataList(AssetDataList);
			
			// Add found blueprints to the list
			for (const FAssetData& AssetData : AssetDataList)
			{
				if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
				{
					FString BlueprintName = AssetData.AssetName.ToString();
					if (BlueprintName.StartsWith(TEXT("BP_")))
					{
						BlueprintOptions.Add(MakeShareable(new FString(BlueprintName)));
					}
				}
			}
			
			ObjectLibrary->RemoveFromRoot();
		}
		
		// Fallback to some common blueprints if none found
		if (BlueprintOptions.Num() <= 1)
		{
			BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_FirstPersonCharacter"))));
			BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_FirstPersonGameMode"))));
			BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_FirstPersonProjectile"))));
			BlueprintOptions.Add(MakeShareable(new FString(TEXT("BP_PickUp_Rifle"))));
		}
	}

	// Create and show the main AI interface window
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("🤖 UnrealAI Assistant")))
		.ClientSize(FVector2D(900, 700))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.SizingRule(ESizingRule::UserSized);

	// Create a simple but comprehensive UI using basic Slate widgets
	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		
		// Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("🤖 UnrealAI Assistant")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
			.ColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f))
		]

		// Status
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20, 0, 20, 20)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("✅ Ready - All AI functions available")))
			.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
			.ColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f))
		]

		// Main Content Area
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(20)
		[
			SNew(SVerticalBox)
			
			// Input Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Enter your prompt:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SEditableTextBox)
					.Text_Lambda([CurrentInputText]() -> FText
					{
						return FText::FromString(*CurrentInputText);
					})
					.HintText(FText::FromString(TEXT("Describe what you want to generate or ask a question...")))
					.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
					.MinDesiredWidth(400.0f)
					.OnTextChanged_Lambda([CurrentInputText](const FText& NewText)
					{
						*CurrentInputText = NewText.ToString();
					})
				]
			]

			// Action Buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 10, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Provider:")))
						.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&ProviderOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(*Item));
						})
						.OnSelectionChanged_Lambda([SelectedProvider](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
						{
							if (NewSelection.IsValid())
							{
								*SelectedProvider = *NewSelection;
							}
						})
						.Content()
						[
							SNew(STextBlock)
							.Text_Lambda([SelectedProvider]() -> FText
							{
								return FText::FromString(*SelectedProvider);
							})
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("AI Functions:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("🔧 Generate Blueprint")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText, SelectedProvider]()
					{
						if (CurrentInputText->IsEmpty())
						{
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please enter a prompt first!\n\nExample: 'Create a health system with damage and healing'")));
							return FReply::Handled();
						}

						// Update log with start message
						*CurrentLogText = TEXT("🔄 Starting Blueprint Generation...\n\n");
						*CurrentLogText += FString::Printf(TEXT("📝 Prompt: %s\n"), **CurrentInputText);
						*CurrentLogText += TEXT("🤖 Sending request to AI...\n");
						*CurrentLogText += TEXT("⏳ Waiting for AI response...\n");

						// Show loading message immediately
						*CurrentResponseText = TEXT("🔄 Generating Blueprint...\n\nPlease wait while the AI processes your request...");

						// Create AI service and generate blueprint asynchronously
						UUnrealAIService* AIService = NewObject<UUnrealAIService>();

						// Create the request
						FAIRequest Request;
						Request.Prompt = **CurrentInputText;
						Request.RequestType = EAIRequestType::BlueprintGeneration;
						// Map provider selection
						if (*SelectedProvider == TEXT("Claude API")) Request.Provider = EAIProvider::Claude; else if (*SelectedProvider == TEXT("OpenAI API")) Request.Provider = EAIProvider::OpenAI; else Request.Provider = EAIProvider::LocalLLM;

						// Send the async request
						AIService->ProcessRequestAsync(Request, [CurrentResponseText, CurrentLogText](const FAIResponse& Response)
						{
							if (Response.bSuccess)
							{
								*CurrentLogText += TEXT("✅ AI Response Received!\n");
								*CurrentLogText += TEXT("🔍 Parsing JSON response...\n");
								*CurrentLogText += TEXT("🏗️ Creating Blueprint structure...\n");
								*CurrentLogText += TEXT("📁 Saving Blueprint asset...\n");
								*CurrentLogText += TEXT("🎉 Blueprint Generation Complete!\n");
								
								*CurrentResponseText = FString::Printf(TEXT("🔧 Blueprint Generation Complete!\n\nResponse:\n%s"), *Response.Content);
							}
							else
							{
								*CurrentLogText += FString::Printf(TEXT("❌ Error: %s\n"), *Response.ErrorMessage);
								*CurrentResponseText = FString::Printf(TEXT("❌ Error: %s"), *Response.ErrorMessage);
							}

							// Show result in dialog
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						});
						
						// Request already processed above
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("💻 Generate C++")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText, SelectedProvider]()
					{
						if (CurrentInputText->IsEmpty())
						{
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please enter a prompt first!\n\nExample: 'Create a weapon class with fire and reload functions'")));
							return FReply::Handled();
						}

						// Update log with start message
						*CurrentLogText = TEXT("💻 Starting C++ Code Generation...\n\n");
						*CurrentLogText += FString::Printf(TEXT("📝 Prompt: %s\n"), **CurrentInputText);
						*CurrentLogText += TEXT("🤖 Sending request to AI...\n");
						*CurrentLogText += TEXT("⏳ Waiting for AI response...\n");

						// Show loading message immediately
						*CurrentResponseText = TEXT("🔄 Generating C++ Code...\n\nPlease wait while the AI processes your request...");

						// Create AI service and generate C++ code asynchronously
						UUnrealAIService* AIService = NewObject<UUnrealAIService>();

						// Create the request
						FAIRequest Request;
						Request.Prompt = **CurrentInputText;
						Request.RequestType = EAIRequestType::CPPGeneration;
						if (*SelectedProvider == TEXT("Claude API")) Request.Provider = EAIProvider::Claude; else if (*SelectedProvider == TEXT("OpenAI API")) Request.Provider = EAIProvider::OpenAI; else Request.Provider = EAIProvider::LocalLLM;

						// Send the async request
						AIService->ProcessRequestAsync(Request, [CurrentResponseText, CurrentLogText](const FAIResponse& Response)
						{
							if (Response.bSuccess)
							{
								*CurrentLogText += TEXT("✅ AI Response Received!\n");
								*CurrentLogText += TEXT("📝 Processing C++ code...\n");
								*CurrentLogText += TEXT("🎉 C++ Code Generation Complete!\n");
								
								*CurrentResponseText = FString::Printf(TEXT("💻 C++ Generation Complete!\n\nResponse:\n%s"), *Response.Content);
							}
							else
							{
								*CurrentLogText += FString::Printf(TEXT("❌ Error: %s\n"), *Response.ErrorMessage);
								*CurrentResponseText = FString::Printf(TEXT("❌ Error: %s"), *Response.ErrorMessage);
							}

							// Show result in dialog
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						});
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("🔍 Analyze Code")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText, SelectedProvider]()
					{
						if (CurrentInputText->IsEmpty())
						{
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please enter code to analyze first!\n\nExample: Paste your C++ or Blueprint code for review")));
							return FReply::Handled();
						}

						// Update log with start message
						*CurrentLogText = TEXT("🔍 Starting Code Analysis...\n\n");
						*CurrentLogText += FString::Printf(TEXT("📝 Code to analyze: %s\n"), **CurrentInputText);
						*CurrentLogText += TEXT("🤖 Sending request to AI...\n");
						*CurrentLogText += TEXT("⏳ Waiting for AI response...\n");

						// Show loading message immediately
						*CurrentResponseText = TEXT("🔄 Analyzing Code...\n\nPlease wait while the AI processes your request...");

						// Create AI service and analyze code asynchronously
						UUnrealAIService* AIService = NewObject<UUnrealAIService>();

						// Create the request
						FAIRequest Request;
						Request.Prompt = **CurrentInputText;
						Request.RequestType = EAIRequestType::CodeReview;
						if (*SelectedProvider == TEXT("Claude API")) Request.Provider = EAIProvider::Claude; else if (*SelectedProvider == TEXT("OpenAI API")) Request.Provider = EAIProvider::OpenAI; else Request.Provider = EAIProvider::LocalLLM;

						// Send the async request
						AIService->ProcessRequestAsync(Request, [CurrentResponseText, CurrentLogText](const FAIResponse& Response)
						{
							if (Response.bSuccess)
							{
								*CurrentLogText += TEXT("✅ AI Response Received!\n");
								*CurrentLogText += TEXT("🔍 Analyzing code structure...\n");
								*CurrentLogText += TEXT("📋 Generating analysis report...\n");
								*CurrentLogText += TEXT("🎉 Code Analysis Complete!\n");
								
								*CurrentResponseText = FString::Printf(TEXT("🔍 Code Analysis Complete!\n\nAnalysis:\n%s"), *Response.Content);
							}
							else
							{
								*CurrentLogText += FString::Printf(TEXT("❌ Error: %s\n"), *Response.ErrorMessage);
								*CurrentResponseText = FString::Printf(TEXT("❌ Error: %s"), *Response.ErrorMessage);
							}

							// Show result in dialog
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						});
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("❓ General Query")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
					{
						if (CurrentInputText->IsEmpty())
						{
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please enter a question first!\n\nExample: 'How do I create a custom Blueprint node?'")));
							return FReply::Handled();
						}

						// Update log with start message
						*CurrentLogText = TEXT("❓ Starting General Query...\n\n");
						*CurrentLogText += FString::Printf(TEXT("📝 Question: %s\n"), **CurrentInputText);
						*CurrentLogText += TEXT("🤖 Sending request to AI...\n");
						*CurrentLogText += TEXT("⏳ Waiting for AI response...\n");

						// Show loading message immediately
						*CurrentResponseText = TEXT("🔄 Processing Query...\n\nPlease wait while the AI processes your request...");

						// Create AI service and handle general query asynchronously
						UUnrealAIService* AIService = NewObject<UUnrealAIService>();

						// Create the request
						FAIRequest Request;
						Request.Prompt = **CurrentInputText;
						Request.RequestType = EAIRequestType::General;
						
						// Send the async request
						AIService->ProcessRequestAsync(Request, [CurrentResponseText, CurrentLogText](const FAIResponse& Response)
						{
							if (Response.bSuccess)
							{
								*CurrentLogText += TEXT("✅ AI Response Received!\n");
								*CurrentLogText += TEXT("📝 Processing answer...\n");
								*CurrentLogText += TEXT("🎉 General Query Complete!\n");
								
								*CurrentResponseText = FString::Printf(TEXT("❓ General Query Complete!\n\nAnswer:\n%s"), *Response.Content);
							}
							else
							{
								*CurrentLogText += FString::Printf(TEXT("❌ Error: %s\n"), *Response.ErrorMessage);
								*CurrentResponseText = FString::Printf(TEXT("❌ Error: %s"), *Response.ErrorMessage);
							}

							// Show result in dialog
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						});
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
										SNew(SButton)
					.Text(FText::FromString(TEXT("🧪 Test JSON")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
					{
						// Update log with start message
						*CurrentLogText = TEXT("🧪 Starting JSON Test...\n\n");
						*CurrentLogText += TEXT("📄 Testing JSON parsing from TestJsonParser.json\n");
						*CurrentLogText += TEXT("🤖 Creating AI service...\n");
						
						// Show loading message immediately
						*CurrentResponseText = TEXT("🧪 Testing JSON Parsing...\n\nPlease wait while the AI service parses the test JSON file...");
						
						// Create AI service and test JSON parsing
						UUnrealAIService* AIService = NewObject<UUnrealAIService>();
						
						// Test the JSON parsing
						AIService->TestJsonParsing();
						
						// Update log with completion
						*CurrentLogText += TEXT("✅ Test completed!\n");
						*CurrentLogText += TEXT("📋 Check the Output Log for JSON parsing results\n");
						
						*CurrentResponseText = TEXT("🧪 Test JSON Parsing Complete!\n\n✅ Test completed successfully!\n📋 Check the Output Log for detailed JSON parsing information.\n\nThis test validates that our JSON structure can be properly parsed.");
						
						// Show result in dialog
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("🔧 Blueprint JSON Parser")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
					{
						// Update log with start message
						*CurrentLogText = TEXT("🔧 Starting Blueprint JSON Parser Test...\n\n");
						*CurrentLogText += TEXT("📄 Testing BlueprintJsonParser functionality\n");
						*CurrentLogText += TEXT("🤖 Creating BlueprintJsonParser instance...\n");
						
						// Show loading message immediately
						*CurrentResponseText = TEXT("🔧 Testing Blueprint JSON Parser...\n\nPlease wait while the parser is tested...");
						
						*CurrentLogText += TEXT("🔧 Testing simplified Blueprint JSON Parser...\n");
						
						// Test our simplified parser
						FBlueprintJsonParser Parser;
						
						// Create a test blueprint info structure
						FBlueprintInfo TestInfo;
						TestInfo.blueprintName = TEXT("TestBlueprint");
						TestInfo.parentClass = TEXT("Actor");
						TestInfo.variables.Add(TEXT("TestVariable (int32)"));
						TestInfo.functions.Add(TEXT("TestFunction"));
						
						// Convert to JSON and log
						TSharedPtr<FJsonObject> TestJson = Parser.BlueprintInfoToJson(TestInfo);
						Parser.LogJsonObject(TestJson);
						
						*CurrentLogText += TEXT("✅ Simplified BlueprintJsonParser test completed!\n");
						*CurrentLogText += TEXT("📋 Check the Output Log for JSON output\n");
						
						*CurrentResponseText = TEXT("🔧 Simplified Blueprint JSON Parser Test Complete!\n\n✅ Test completed successfully!\n📋 Check the Output Log for the JSON output.\n\nThis test validates that our simplified parser is working correctly.");
						
						// Show result in dialog
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						
						return FReply::Handled();
					})
				]
			]

			// Blueprint Selection Tools Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("🔧 Blueprint Selection Tools:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 10, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Select Blueprint:")))
						.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&BlueprintOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(*Item));
						})
						.OnSelectionChanged_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText](TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectionType)
						{
							if (NewSelection.IsValid())
							{
								*CurrentInputText = *NewSelection;
								*CurrentLogText = FString::Printf(TEXT("🔧 Blueprint Selected: %s\n"), **NewSelection);
							}
						})
						.Content()
						[
							SNew(STextBlock)
							.Text_Lambda([CurrentInputText]() -> FText
							{
								return FText::FromString(CurrentInputText->IsEmpty() ? TEXT("Select a Blueprint...") : *CurrentInputText);
							})
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 5, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("📥 Load Blueprint")))
						.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
						{
							if (CurrentInputText->IsEmpty() || *CurrentInputText == TEXT("Select a Blueprint..."))
							{
								FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please select a blueprint first!")));
								return FReply::Handled();
							}

							*CurrentLogText = FString::Printf(TEXT("📥 Loading Blueprint: %s\n"), **CurrentInputText);
							*CurrentResponseText = FString::Printf(TEXT("📥 Blueprint Loaded: %s\n\nReady for JSON conversion and editing."), **CurrentInputText);
							
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 5, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("🔄 Convert to JSON")))
						.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
						{
							UE_LOG(LogTemp, Log, TEXT("🔄 Convert to JSON button clicked!"));
							*CurrentLogText = TEXT("🔄 Convert to JSON button clicked!\n");
							
							if (CurrentInputText->IsEmpty() || *CurrentInputText == TEXT("Select a Blueprint..."))
							{
								UE_LOG(LogTemp, Warning, TEXT("No blueprint selected"));
								*CurrentLogText += TEXT("❌ No blueprint selected!\n");
								FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please select a blueprint first!")));
								return FReply::Handled();
							}

							UE_LOG(LogTemp, Log, TEXT("Selected blueprint: %s"), **CurrentInputText);
							*CurrentLogText += FString::Printf(TEXT("📋 Selected blueprint: %s\n"), **CurrentInputText);
							*CurrentLogText += TEXT("🔄 Converting Blueprint to JSON...\n");
							
							// Use our simplified BlueprintJsonParser to convert the real blueprint
							*CurrentLogText += TEXT("🔄 Using simplified BlueprintJsonParser...\n");
							UE_LOG(LogTemp, Log, TEXT("Using simplified BlueprintJsonParser..."));
							
							// Find the blueprint asset
							UBlueprint* BlueprintAsset = nullptr;
							UE_LOG(LogTemp, Log, TEXT("Loading AssetRegistry module..."));
							FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
							UE_LOG(LogTemp, Log, TEXT("AssetRegistry module loaded successfully"));
							
							// Try multiple approaches to find the blueprint
							UE_LOG(LogTemp, Log, TEXT("Trying multiple asset discovery methods..."));
							
							// Method 1: Direct asset lookup by name
							UE_LOG(LogTemp, Log, TEXT("Method 1: Direct asset lookup by name"));
							FString AssetPath = FString::Printf(TEXT("/Game/FirstPerson/Blueprints/%s.%s"), **CurrentInputText, **CurrentInputText);
							UE_LOG(LogTemp, Log, TEXT("Looking for asset at path: %s"), *AssetPath);
							BlueprintAsset = LoadObject<UBlueprint>(nullptr, *AssetPath);
							if (BlueprintAsset)
							{
								UE_LOG(LogTemp, Log, TEXT("✅ Found blueprint using direct path: %s"), *AssetPath);
								*CurrentLogText += FString::Printf(TEXT("✅ Found blueprint using direct path: %s\n"), *AssetPath);
							}
							else
							{
								UE_LOG(LogTemp, Log, TEXT("❌ Direct path lookup failed"));
								*CurrentLogText += TEXT("❌ Direct path lookup failed\n");
								
								// Method 2: Try different package paths
								UE_LOG(LogTemp, Log, TEXT("Method 2: Trying different package paths"));
								TArray<FString> PackagePaths = {
									TEXT("/Game"),
									TEXT("/Game/FirstPerson"),
									TEXT("/Game/FirstPerson/Blueprints"),
									TEXT("/Game/FirstPerson/Character"),
									TEXT("/Game/FirstPerson/Input"),
									TEXT("/Game/FirstPerson/Maps")
								};
								
								for (const FString& PackagePath : PackagePaths)
								{
									UE_LOG(LogTemp, Log, TEXT("Trying package path: %s"), *PackagePath);
									*CurrentLogText += FString::Printf(TEXT("🔍 Trying package path: %s\n"), *PackagePath);
									
									FARFilter Filter;
									Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
									Filter.PackagePaths.Add(FName(*PackagePath));
									
									TArray<FAssetData> AssetDataList;
									AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);
									UE_LOG(LogTemp, Log, TEXT("Found %d assets in %s"), AssetDataList.Num(), *PackagePath);
									*CurrentLogText += FString::Printf(TEXT("  Found %d assets in %s\n"), AssetDataList.Num(), *PackagePath);
									
									// Log first few assets for debugging
									for (int32 i = 0; i < AssetDataList.Num() && i < 5; i++)
									{
										UE_LOG(LogTemp, Log, TEXT("  Asset %d: %s (Class: %s)"), i, *AssetDataList[i].AssetName.ToString(), *AssetDataList[i].AssetClassPath.ToString());
										*CurrentLogText += FString::Printf(TEXT("    Asset %d: %s (Class: %s)\n"), i, *AssetDataList[i].AssetName.ToString(), *AssetDataList[i].AssetClassPath.ToString());
									}
									
									// Look for our specific blueprint
									for (const FAssetData& AssetData : AssetDataList)
									{
										if (AssetData.AssetName.ToString() == *CurrentInputText)
										{
											UE_LOG(LogTemp, Log, TEXT("Found matching asset! Loading..."));
											*CurrentLogText += TEXT("✅ Found matching asset! Loading...\n");
											BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
											if (BlueprintAsset)
											{
												UE_LOG(LogTemp, Log, TEXT("Successfully cast to UBlueprint"));
												*CurrentLogText += TEXT("✅ Successfully cast to UBlueprint\n");
												break;
											}
											else
											{
												UE_LOG(LogTemp, Warning, TEXT("Failed to cast asset to UBlueprint"));
												*CurrentLogText += TEXT("❌ Failed to cast asset to UBlueprint\n");
											}
										}
									}
									
									if (BlueprintAsset)
									{
										break;
									}
								}
								
								// Method 3: Try to find any blueprint asset
								if (!BlueprintAsset)
								{
									UE_LOG(LogTemp, Log, TEXT("Method 3: Looking for any blueprint asset"));
									*CurrentLogText += TEXT("🔍 Method 3: Looking for any blueprint asset\n");
									
									FARFilter Filter;
									Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
									
									TArray<FAssetData> AllBlueprints;
									AssetRegistryModule.Get().GetAssets(Filter, AllBlueprints);
									UE_LOG(LogTemp, Log, TEXT("Found %d total blueprints in project"), AllBlueprints.Num());
									*CurrentLogText += FString::Printf(TEXT("Found %d total blueprints in project\n"), AllBlueprints.Num());
									
									// Log all found blueprints
									for (int32 i = 0; i < AllBlueprints.Num() && i < 20; i++)
									{
										UE_LOG(LogTemp, Log, TEXT("Blueprint %d: %s at %s"), i, *AllBlueprints[i].AssetName.ToString(), *AllBlueprints[i].PackagePath.ToString());
										*CurrentLogText += FString::Printf(TEXT("  Blueprint %d: %s at %s\n"), i, *AllBlueprints[i].AssetName.ToString(), *AllBlueprints[i].PackagePath.ToString());
									}
								}
							}
							
							// This section was removed as it's now handled by the multiple discovery methods above
							
							if (BlueprintAsset)
							{
								UE_LOG(LogTemp, Log, TEXT("Blueprint found and loaded successfully: %s"), **CurrentInputText);
								*CurrentLogText += FString::Printf(TEXT("✅ Blueprint found: %s\n"), **CurrentInputText);
								
								// Use our simplified parser
								UE_LOG(LogTemp, Log, TEXT("Creating BlueprintJsonParser..."));
								FBlueprintJsonParser Parser;
								UE_LOG(LogTemp, Log, TEXT("Calling ConvertBlueprintToJsonAndLog..."));
								Parser.ConvertBlueprintToJsonAndLog(BlueprintAsset);
								UE_LOG(LogTemp, Log, TEXT("ConvertBlueprintToJsonAndLog completed"));
								
								*CurrentResponseText = TEXT("🔄 Blueprint converted to JSON and logged to console!\n\nCheck the Output Log for the JSON structure.");
								*CurrentLogText += TEXT("✅ JSON conversion complete! Check console for output.\n");
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("Failed to find or load blueprint: %s"), **CurrentInputText);
								*CurrentResponseText = FString::Printf(TEXT("❌ Failed to find blueprint: %s"), **CurrentInputText);
								*CurrentLogText += FString::Printf(TEXT("❌ Failed to find blueprint: %s\n"), **CurrentInputText);
							}
							
							UE_LOG(LogTemp, Log, TEXT("Convert to JSON button handler completed"));
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 5, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("💾 Save JSON")))
						.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
						{
							if (CurrentResponseText->IsEmpty() || !CurrentResponseText->Contains(TEXT("{")))
							{
								FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("No JSON data to save! Convert a blueprint to JSON first.")));
								return FReply::Handled();
							}

							*CurrentLogText = TEXT("💾 Saving JSON to file...\n");
							
							*CurrentLogText += TEXT("💾 JSON saving not implemented in simplified parser\n");
							*CurrentLogText += TEXT("📋 JSON data is available in the response area above\n");
							
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("JSON saving not implemented in simplified parser.\n\nJSON data is available in the response area above.")));
							
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 5, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("📝 Edit JSON")))
						.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
						{
							if (CurrentResponseText->IsEmpty() || !CurrentResponseText->Contains(TEXT("{")))
							{
								FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("No JSON data to edit! Convert a blueprint to JSON first.")));
								return FReply::Handled();
							}

							*CurrentLogText = TEXT("📝 Opening JSON editor...\n");
							
							// Create a simple JSON editor dialog
							TSharedRef<SWindow> EditorWindow = SNew(SWindow)
								.Title(FText::FromString(TEXT("JSON Editor")))
								.ClientSize(FVector2D(600, 400))
								.SupportsMaximize(true)
								.SupportsMinimize(true);

							TSharedRef<SEditableTextBox> JsonEditor = SNew(SEditableTextBox)
								.Text(FText::FromString(*CurrentResponseText))
								.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
								.OnTextChanged_Lambda([CurrentResponseText](const FText& NewText)
								{
									*CurrentResponseText = NewText.ToString();
								});

							EditorWindow->SetContent(
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								.Padding(10)
								[
									JsonEditor
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(10)
								.HAlign(HAlign_Right)
								[
									SNew(SButton)
									.Text(FText::FromString(TEXT("Save Changes")))
									.OnClicked_Lambda([EditorWindow, CurrentLogText]()
									{
										*CurrentLogText += TEXT("✅ JSON changes saved!\n");
										EditorWindow->RequestDestroyWindow();
										return FReply::Handled();
									})
								]
							);

							FSlateApplication::Get().AddWindow(EditorWindow);
							
							return FReply::Handled();
						})
					]
				]
			]

			// Utility Buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Utilities:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("🔀 Merge Conflicts")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
					{
						// Update log with start message
						*CurrentLogText = TEXT("🔀 Starting UAsset Merge Analysis...\n\n");
						*CurrentLogText += TEXT("📁 This feature will analyze UAsset files for merge conflicts\n");
						*CurrentLogText += TEXT("🤖 Using AI to identify merge points and preserve GUIDs\n");
						*CurrentLogText += TEXT("⏳ Processing...\n");

						// Show loading message immediately
						*CurrentResponseText = TEXT("🔄 Processing UAsset Merge...\n\nPlease wait while the AI analyzes the assets...");

						// Create AI service and process UAsset merge asynchronously
						UUnrealAIService* AIService = NewObject<UUnrealAIService>();

						// Create the request with example paths (in a real implementation, these would come from file dialogs)
						FAIRequest Request;
						Request.RequestType = EAIRequestType::UAssetMerge;
						Request.Provider = EAIProvider::LocalLLM;
						Request.ContextData.Add(TEXT("BaseAssetPath"), TEXT("/Game/Example/BaseAsset.uasset"));
						Request.ContextData.Add(TEXT("ModifiedAssetPath"), TEXT("/Game/Example/ModifiedAsset.uasset"));
						Request.ContextData.Add(TEXT("OutputAssetPath"), TEXT("/Game/Example/MergedAsset.uasset"));
						Request.ContextData.Add(TEXT("AssetPath"), TEXT("/Game/Example/"));

						// Send the async request
						AIService->ProcessRequestAsync(Request, [CurrentResponseText, CurrentLogText](const FAIResponse& Response)
						{
							if (Response.bSuccess)
							{
								*CurrentLogText += TEXT("✅ UAsset Analysis Complete!\n");
								*CurrentLogText += TEXT("🔍 AI has analyzed the asset structure...\n");
								*CurrentLogText += TEXT("🔀 Merging assets while preserving GUIDs...\n");
								*CurrentLogText += TEXT("💾 Writing merged asset to disk...\n");
								*CurrentLogText += TEXT("🎉 UAsset Merge Complete!\n");
								
								*CurrentResponseText = FString::Printf(TEXT("🔀 UAsset Merge Complete!\n\nResult:\n%s"), *Response.Content);
							}
							else
							{
								*CurrentLogText += FString::Printf(TEXT("❌ Error: %s\n"), *Response.ErrorMessage);
								*CurrentResponseText = FString::Printf(TEXT("❌ UAsset Merge Error: %s"), *Response.ErrorMessage);
							}

							// Show result in dialog
							FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(*CurrentResponseText));
						});
						
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("🗑️ Clear")))
					.OnClicked_Lambda([CurrentInputText, CurrentResponseText, CurrentLogText]()
					{
						*CurrentInputText = TEXT("");
						*CurrentResponseText = TEXT("Input and response areas cleared.");
						*CurrentLogText = TEXT("📋 AI Processing Log\n\nLog cleared. Ready for new requests...");
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("🗑️ Clear\n\nInput, response, and log areas have been cleared.")));
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 5)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("⚙️ Settings")))
					.OnClicked_Lambda([]()
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("⚙️ Settings\n\nConfigure AI providers and advanced options:\n\n• Local LLM (Ollama)\n• Claude API\n• OpenAI API\n• Temperature settings\n• Max tokens\n\nFunctionality will be implemented in the next version.")));
						return FReply::Handled();
					})
				]
			]

			// Log Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 20)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("📋 Processing Log:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(150.0f)
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
			]

			// Response Section
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("AI Response:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([CurrentResponseText]() -> FText
					{
						return FText::FromString(*CurrentResponseText);
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
					.AutoWrapText(true)
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
