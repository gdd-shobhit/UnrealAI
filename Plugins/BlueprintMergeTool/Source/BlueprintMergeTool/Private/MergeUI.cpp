#include "../Public/MergeUI.h"
#include "../Public/PerforceAdapter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Misc/MessageDialog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Editor.h"
#include "Styling/CoreStyle.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Input/STextComboBox.h"
#include "UObject/SavePackage.h"

void SMergeUI::Construct(const FArguments& InArgs)
{
	// Initialize shared text storage
	StatusMessage = MakeShareable(new FString(TEXT("Ready to merge Blueprints")));
	PreviewText = MakeShareable(new FString(TEXT("Select Blueprints to see merge preview...")));
	ConflictSummaryText = MakeShareable(new FString(TEXT("No conflicts detected")));
	OperationSummaryText = MakeShareable(new FString(TEXT("No operations to apply")));
	
	BasePathText = MakeShareable(new FString(TEXT("No base Blueprint selected")));
	LocalPathText = MakeShareable(new FString(TEXT("No local Blueprint selected")));
	RemotePathText = MakeShareable(new FString(TEXT("No remote Blueprint selected")));

	// Initialize combo box options
	BlueprintOptions.Empty();
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("Select a Blueprint..."))));
	SelectedBaseBlueprint = BlueprintOptions[0];
	SelectedLocalBlueprint = BlueprintOptions[0];
	SelectedRemoteBlueprint = BlueprintOptions[0];
	PopulateBlueprintOptions();

	// Initialize state
	bDiffPerformed = false;
	bMergePlanCreated = false;
	bHasUnresolvedConflicts = false;
	
	// Initialize Perforce state
	ConflictedBlueprints.Empty();
	SelectedConflictedBlueprint.Empty();

	// Initialize merge config with sensible defaults
	MergeConfig.DefaultStrategy = EResolutionStrategy::NonDestructive;
	MergeConfig.bPreferLocalForVariables = true;
	MergeConfig.bPreferRemoteForNodes = false;
	MergeConfig.bAutoResolvePositionConflicts = true;
	MergeConfig.bEnableLLMResolution = false; // Disable LLM for now
	MergeConfig.ConflictThresholdForManualReview = 0.7f;

	ChildSlot
	[
		SNew(SVerticalBox)
		
		// Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("🔀 Blueprint Merge Tool")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
			.ColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.4f, 1.0f))
		]

		// Status
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20, 0, 20, 20)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				return GetStatusText();
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
			.ColorAndOpacity_Lambda([this]() -> FSlateColor
			{
				return GetStatusColor();
			})
		]

		// Main Content
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(20)
		[
			SNew(SScrollBox)
			
			// Perforce Integration Section (if available)
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				CreatePerforceSection()
			]

			// Separator
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				SNew(SSeparator)
			]

			// File Selection Section
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				CreateFileSelectionSection()
			]

			// Separator
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				SNew(SSeparator)
			]

			// Conflict Resolution Section
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				CreateConflictResolutionSection()
			]

			// Separator
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				SNew(SSeparator)
			]

			// Preview Section
			+ SScrollBox::Slot()
			.Padding(0, 0, 0, 20)
			[
				CreatePreviewSection()
			]

			// Action Buttons
			+ SScrollBox::Slot()
			[
				CreateActionButtonsSection()
			]
		]
	];
}

TSharedRef<SWidget> SMergeUI::CreateFileSelectionSection()
{
	return SNew(SVerticalBox)
		
		// Section Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("📁 Blueprint Selection")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]

		// Base Blueprint
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Base:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.MinDesiredWidth(60.0f)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextComboBox)
				.OptionsSource(&BlueprintOptions)
				.InitiallySelectedItem(SelectedBaseBlueprint)
				.OnSelectionChanged(this, &SMergeUI::OnBaseBlueprintSelected)
			]
		]

		// Local Blueprint
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Local:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.MinDesiredWidth(60.0f)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextComboBox)
				.OptionsSource(&BlueprintOptions)
				.InitiallySelectedItem(SelectedLocalBlueprint)
				.OnSelectionChanged(this, &SMergeUI::OnLocalBlueprintSelected)
			]
		]

		// Remote Blueprint
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Remote:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.MinDesiredWidth(60.0f)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextComboBox)
				.OptionsSource(&BlueprintOptions)
				.InitiallySelectedItem(SelectedRemoteBlueprint)
				.OnSelectionChanged(this, &SMergeUI::OnRemoteBlueprintSelected)
			]
		]

		// Action buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 15, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🔍 Perform Diff")))
				.OnClicked(this, &SMergeUI::OnPerformDiff)
				.IsEnabled_Lambda([this]() -> bool
				{
					return BaseBlueprint.IsValid() && LocalBlueprint.IsValid() && RemoteBlueprint.IsValid();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("📋 Create Plan")))
				.OnClicked(this, &SMergeUI::OnCreateMergePlan)
				.IsEnabled_Lambda([this]() -> bool
				{
					return bDiffPerformed;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("💾 Export Snapshots")))
				.OnClicked(this, &SMergeUI::OnExportSnapshots)
				.IsEnabled_Lambda([this]() -> bool
				{
					return BaseSnapshot.IsValid() || LocalSnapshot.IsValid() || RemoteSnapshot.IsValid();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🗑️ Clear All")))
				.OnClicked(this, &SMergeUI::OnClearAll)
			]
		];
}

TSharedRef<SWidget> SMergeUI::CreatePerforceSection()
{
	return SNew(SVerticalBox)
		
		// Section Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("🔧 Perforce Integration")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]

		// Perforce status
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				if (FPerforceAdapter::IsPerforceAvailable())
				{
					return FText::FromString(TEXT("✓ Perforce is available"));
				}
				else
				{
					return FText::FromString(TEXT("⚠ Perforce is not configured. Manual Blueprint selection is still available below."));
				}
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
			.ColorAndOpacity_Lambda([this]() -> FSlateColor
			{
				if (FPerforceAdapter::IsPerforceAvailable())
				{
					return FSlateColor(FLinearColor(0.2f, 0.8f, 0.4f, 1.0f));
				}
				else
				{
					return FSlateColor(FLinearColor(0.8f, 0.6f, 0.2f, 1.0f));
				}
			})
		]

		// Perforce buttons (only shown if Perforce is available)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return GetPerforceSectionVisibility(); })
			[
				SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🔍 Detect Conflicts")))
				.ToolTipText(FText::FromString(TEXT("Scan for Blueprints with Perforce conflicts")))
				.OnClicked(this, &SMergeUI::OnDetectPerforceConflicts)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("📥 Load from Perforce")))
				.ToolTipText(FText::FromString(TEXT("Load BASE, LOCAL, and REMOTE versions for selected conflicted Blueprint")))
				.OnClicked(this, &SMergeUI::OnLoadFromPerforce)
				.IsEnabled_Lambda([this]() -> bool
				{
					return !SelectedConflictedBlueprint.IsEmpty();
				})
			]
			]
		]

		// Selected conflicted Blueprint display
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return GetSelectedBlueprintVisibility(); })
			[
				SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.2f, 0.4f, 0.2f, 1.0f))
			.Padding(10)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(FString::Printf(TEXT("Selected Blueprint: %s"), *SelectedConflictedBlueprint));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
				.AutoWrapText(true)
			]
			]
		]

		// Conflict count display
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return GetConflictCountVisibility(); })
			[
				SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				return FText::FromString(FString::Printf(TEXT("Conflicts detected: %d Blueprint(s)"), ConflictedBlueprints.Num()));
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
			.ColorAndOpacity(FLinearColor(0.8f, 0.6f, 0.2f, 1.0f))
			]
		]

		// Post-merge Perforce actions (only shown after merge is applied)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return GetPostMergeActionsVisibility(); })
			[
				SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("✅ Resolve in Perforce")))
				.ToolTipText(FText::FromString(TEXT("Mark the conflict as resolved in Perforce (accept merge)")))
				.OnClicked(this, &SMergeUI::OnResolveInPerforce)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("📤 Submit to Perforce")))
				.ToolTipText(FText::FromString(TEXT("Submit the merged Blueprint to Perforce")))
				.OnClicked(this, &SMergeUI::OnSubmitToPerforce)
			]
			]
		];
}

TSharedRef<SWidget> SMergeUI::CreateConflictResolutionSection()
{
	return SNew(SVerticalBox)
		
		// Section Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("⚠️ Conflict Resolution")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]

		// Conflict Summary
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				return GetConflictSummaryText();
			})
			.Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
			.AutoWrapText(true)
		]

		// Auto-resolution buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("📍 Use Local")))
				.ToolTipText(FText::FromString(TEXT("Resolve all conflicts by preferring local changes")))
				.OnClicked_Lambda([this]() -> FReply
				{
					return OnResolveAllConflicts(FString(TEXT("UseLocal")));
				})
				.IsEnabled_Lambda([this]() -> bool
				{
					return bDiffPerformed && CurrentDiffResult.bHasConflicts;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🌐 Use Remote")))
				.ToolTipText(FText::FromString(TEXT("Resolve all conflicts by preferring remote changes")))
				.OnClicked_Lambda([this]() -> FReply
				{
					return OnResolveAllConflicts(FString(TEXT("UseRemote")));
				})
				.IsEnabled_Lambda([this]() -> bool
				{
					return bDiffPerformed && CurrentDiffResult.bHasConflicts;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🔄 Smart Merge")))
				.ToolTipText(FText::FromString(TEXT("Attempt intelligent conflict resolution")))
				.OnClicked_Lambda([this]() -> FReply
				{
					return OnResolveAllConflicts(FString(TEXT("SmartMerge")));
				})
				.IsEnabled_Lambda([this]() -> bool
				{
					return bDiffPerformed && CurrentDiffResult.bHasConflicts;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🛡️ Non-Destructive")))
				.ToolTipText(FText::FromString(TEXT("Resolve conflicts with non-destructive strategy - keep both conflicting changes when possible")))
				.OnClicked_Lambda([this]() -> FReply
				{
					return OnResolveAllConflicts(FString(TEXT("NonDestructive")));
				})
				.IsEnabled_Lambda([this]() -> bool
				{
					return bDiffPerformed && CurrentDiffResult.bHasConflicts;
				})
			]
		];
}

TSharedRef<SWidget> SMergeUI::CreatePreviewSection()
{
	return SNew(SVerticalBox)
		
		// Section Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("👁️ Merge Preview")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]

		// Operation Summary
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Operations Summary:")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(100.0f)
		.Padding(0, 0, 0, 15)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(10.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return GetOperationSummaryText();
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
				.AutoWrapText(true)
			]
		]

		// Full Preview
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Detailed Preview:")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.MaxHeight(300.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(10.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						return GetPreviewText();
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
					.AutoWrapText(true)
				]
			]
		];
}

TSharedRef<SWidget> SMergeUI::CreateActionButtonsSection()
{
	return SNew(SVerticalBox)
		
		// Section Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 15)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("🚀 Actions")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]

		// Action buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 15, 0)
			[
				SNew(SButton)
				.Text_Lambda([this]() -> FText
				{
					if (bHasUnresolvedConflicts)
					{
						return FText::FromString(TEXT("⚠️ Apply Merge (Has Conflicts)"));
					}
					return FText::FromString(TEXT("✅ Apply Merge"));
				})
				.OnClicked(this, &SMergeUI::OnApplyMerge)
				.IsEnabled_Lambda([this]() -> bool
				{
					return bMergePlanCreated; // Allow applying even with conflicts (user will be warned)
				})
				.ToolTipText_Lambda([this]() -> FText
				{
					if (bHasUnresolvedConflicts)
					{
						return FText::FromString(TEXT("Apply merge with unresolved conflicts. You will be warned before proceeding."));
					}
					return FText::FromString(TEXT("Apply the merge plan to the selected Blueprint"));
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 15, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("🔄 Refresh Diff")))
				.OnClicked(this, &SMergeUI::OnPerformDiff)
				.IsEnabled_Lambda([this]() -> bool
				{
					return BaseBlueprint.IsValid() && LocalBlueprint.IsValid() && RemoteBlueprint.IsValid();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 15, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("📥 Import Snapshots")))
				.OnClicked(this, &SMergeUI::OnImportSnapshots)
			]
		];
}

FReply SMergeUI::OnLoadBaseBlueprint()
{
	FString SelectedPath;
	if (ShowBlueprintSelectionDialog(SelectedPath))
	{
		UBlueprint* Blueprint = LoadBlueprintFromPath(SelectedPath);
		if (Blueprint && IsValid(Blueprint))
		{
			// Clear any existing reference first
			BaseBlueprint.Reset();
			
			BaseBlueprint = Blueprint;
			*BasePathText = FString::Printf(TEXT("Base: %s"), *GetBlueprintDisplayName(Blueprint));
			*StatusMessage = TEXT("Base Blueprint loaded");
			
			// Create snapshot with error handling
			try
			{
				if (FSnapshotManager::CreateSnapshot(Blueprint, BaseSnapshot))
				{
					UE_LOG(LogTemp, Log, TEXT("Base Blueprint snapshot created"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to create Base Blueprint snapshot"));
					*StatusMessage = TEXT("Base Blueprint loaded but snapshot failed");
				}
			}
			catch (...)
			{
				UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating Base Blueprint snapshot"));
				*StatusMessage = TEXT("Base Blueprint loaded but snapshot failed");
			}
		}
		else
		{
			*StatusMessage = TEXT("Failed to load Base Blueprint");
		}
	}

	return FReply::Handled();
}

FReply SMergeUI::OnLoadLocalBlueprint()
{
	FString SelectedPath;
	if (ShowBlueprintSelectionDialog(SelectedPath))
	{
		UBlueprint* Blueprint = LoadBlueprintFromPath(SelectedPath);
		if (Blueprint && IsValid(Blueprint))
		{
			// Clear any existing reference first
			LocalBlueprint.Reset();
			
			LocalBlueprint = Blueprint;
			*LocalPathText = FString::Printf(TEXT("Local: %s"), *GetBlueprintDisplayName(Blueprint));
			*StatusMessage = TEXT("Local Blueprint loaded");

			// Create snapshot with error handling
			try
			{
				if (FSnapshotManager::CreateSnapshot(Blueprint, LocalSnapshot))
				{
					UE_LOG(LogTemp, Log, TEXT("Local Blueprint snapshot created"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to create Local Blueprint snapshot"));
					*StatusMessage = TEXT("Local Blueprint loaded but snapshot failed");
				}
			}
			catch (...)
			{
				UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating Local Blueprint snapshot"));
				*StatusMessage = TEXT("Local Blueprint loaded but snapshot failed");
			}
		}
		else
		{
			*StatusMessage = TEXT("Failed to load Local Blueprint");
		}
	}

	return FReply::Handled();
}

FReply SMergeUI::OnLoadRemoteBlueprint()
{
	FString SelectedPath;
	if (ShowBlueprintSelectionDialog(SelectedPath))
	{
		UBlueprint* Blueprint = LoadBlueprintFromPath(SelectedPath);
		if (Blueprint && IsValid(Blueprint))
		{
			// Clear any existing reference first
			RemoteBlueprint.Reset();
			
			RemoteBlueprint = Blueprint;
			*RemotePathText = FString::Printf(TEXT("Remote: %s"), *GetBlueprintDisplayName(Blueprint));
			*StatusMessage = TEXT("Remote Blueprint loaded");

			// Create snapshot with error handling
			try
			{
				if (FSnapshotManager::CreateSnapshot(Blueprint, RemoteSnapshot))
				{
					UE_LOG(LogTemp, Log, TEXT("Remote Blueprint snapshot created"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to create Remote Blueprint snapshot"));
					*StatusMessage = TEXT("Remote Blueprint loaded but snapshot failed");
				}
			}
			catch (...)
			{
				UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating Remote Blueprint snapshot"));
				*StatusMessage = TEXT("Remote Blueprint loaded but snapshot failed");
			}
		}
		else
		{
			*StatusMessage = TEXT("Failed to load Remote Blueprint");
		}
	}

	return FReply::Handled();
}

FReply SMergeUI::OnPerformDiff()
{
	// Check if Blueprint references are valid
	if (!BaseBlueprint.IsValid() || !LocalBlueprint.IsValid() || !RemoteBlueprint.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please load all three Blueprints first!")));
		return FReply::Handled();
	}

	// Additional validation - check if the objects are still valid
	UBlueprint* BaseBP = BaseBlueprint.Get();
	UBlueprint* LocalBP = LocalBlueprint.Get();
	UBlueprint* RemoteBP = RemoteBlueprint.Get();

	if (!IsValid(BaseBP) || !IsValid(LocalBP) || !IsValid(RemoteBP))
	{
		UE_LOG(LogTemp, Warning, TEXT("Blueprint references are invalid (possibly due to renaming). Clearing all references."));
		ClearAllBlueprintReferences();
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Blueprint references are invalid (possibly due to renaming). Please reload all Blueprints.")));
		return FReply::Handled();
	}

	*StatusMessage = TEXT("Performing three-way diff...");

	// Create snapshots from the currently loaded Blueprints with error handling
	try
	{
		if (!FSnapshotManager::CreateSnapshot(BaseBP, BaseSnapshot) ||
			!FSnapshotManager::CreateSnapshot(LocalBP, LocalSnapshot) ||
			!FSnapshotManager::CreateSnapshot(RemoteBP, RemoteSnapshot))
		{
			*StatusMessage = TEXT("Failed to create snapshots");
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to create snapshots. Check the log for details.")));
			return FReply::Handled();
		}
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating snapshots"));
		*StatusMessage = TEXT("Exception occurred while creating snapshots");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Exception occurred while creating snapshots. Check the log for details.")));
		return FReply::Handled();
	}

	// Perform the diff
	if (FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, CurrentDiffResult))
	{
		bDiffPerformed = true;
		*StatusMessage = FString::Printf(TEXT("Diff completed: %d operations, %d conflicts"), 
			CurrentDiffResult.Operations.Num(), CurrentDiffResult.Conflicts.Num());

		UpdatePreview();
	}
	else
	{
		*StatusMessage = TEXT("Failed to perform diff");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to perform three-way diff. Check the log for details.")));
	}

	return FReply::Handled();
}

FReply SMergeUI::OnCreateMergePlan()
{
	if (!bDiffPerformed)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please perform diff first!")));
		return FReply::Handled();
	}

	*StatusMessage = TEXT("Creating merge plan...");

	// Create the merge plan
	UE_LOG(LogTemp, Log, TEXT("MergeUI: Creating merge plan with %d operations and %d conflicts"), 
		CurrentDiffResult.Operations.Num(), CurrentDiffResult.Conflicts.Num());
	
	if (FMergePlanner::CreateMergePlan(CurrentDiffResult, MergeConfig, CurrentMergePlan))
	{
		bMergePlanCreated = true;
		bHasUnresolvedConflicts = CurrentMergePlan.bRequiresManualReview || CurrentMergePlan.UnresolvedConflicts.Num() > 0;
		
		UE_LOG(LogTemp, Log, TEXT("MergeUI: Merge plan created successfully - %d auto-resolved, %d manual review"), 
			CurrentMergePlan.AutoResolvedOperations.Num(), CurrentMergePlan.ManualReviewRequired.Num());
		
		*StatusMessage = FString::Printf(TEXT("Merge plan created: %d auto-resolved, %d need manual review"), 
			CurrentMergePlan.AutoResolvedOperations.Num(), CurrentMergePlan.ManualReviewRequired.Num());

		UpdatePreview();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MergeUI: Failed to create merge plan"));
		*StatusMessage = TEXT("Failed to create merge plan");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to create merge plan. Check the log for details.")));
	}

	return FReply::Handled();
}

FReply SMergeUI::OnApplyMerge()
{
	if (!bMergePlanCreated)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please create merge plan first!")));
		return FReply::Handled();
	}

	if (bHasUnresolvedConflicts)
	{
		EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, 
			FText::FromString(TEXT("There are unresolved conflicts. Applying the merge may cause issues.\n\nDo you want to continue anyway?")));
		
		if (Result != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}
	}

	// Ask which Blueprint to apply to
	// NOTE: Base should NEVER be modified - it's the common ancestor for 3-way merge
	EAppReturnType::Type Target = FMessageDialog::Open(EAppMsgType::YesNoCancel,
		FText::FromString(TEXT("Which Blueprint should receive the merged changes?\n\nYes = Local Blueprint (recommended)\nNo = Remote Blueprint\nCancel = Abort\n\n⚠ Base Blueprint will NEVER be modified")));

	UBlueprint* TargetBlueprint = nullptr;
	FString TargetBlueprintName;
	if (Target == EAppReturnType::Yes)
	{
		TargetBlueprint = LocalBlueprint.Get();
		TargetBlueprintName = TEXT("Local");
		UE_LOG(LogTemp, Log, TEXT("MergeUI: User selected LOCAL Blueprint as merge target: %s"), 
			TargetBlueprint ? *TargetBlueprint->GetName() : TEXT("NULL"));
	}
	else if (Target == EAppReturnType::No)
	{
		TargetBlueprint = RemoteBlueprint.Get();
		TargetBlueprintName = TEXT("Remote");
		UE_LOG(LogTemp, Log, TEXT("MergeUI: User selected REMOTE Blueprint as merge target: %s"), 
			TargetBlueprint ? *TargetBlueprint->GetName() : TEXT("NULL"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("MergeUI: User cancelled merge application"));
		return FReply::Handled(); // Cancelled
	}

	if (!TargetBlueprint)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Target Blueprint is no longer valid!")));
		return FReply::Handled();
	}

	// Safety check: NEVER allow Base to be modified
	if (BaseBlueprint.IsValid() && TargetBlueprint == BaseBlueprint.Get())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("ERROR: Cannot modify Base Blueprint!\n\nBase is the common ancestor and must remain unchanged.\nPlease select Local or Remote instead.")));
		UE_LOG(LogTemp, Error, TEXT("MergeUI: BLOCKED attempt to modify Base Blueprint!"));
		return FReply::Handled();
	}

	UE_LOG(LogTemp, Log, TEXT("MergeUI: Applying merge to %s Blueprint: %s (Path: %s)"), 
		*TargetBlueprintName, 
		*TargetBlueprint->GetName(),
		*TargetBlueprint->GetPathName());

	*StatusMessage = FString::Printf(TEXT("Applying merge operations to %s Blueprint..."), *TargetBlueprintName);

	// Apply the merge
	FApplyResult ApplyResult;
	if (FApplyEngine::ApplyMergePlan(TargetBlueprint, CurrentMergePlan, ApplyResult))
	{
		*StatusMessage = FString::Printf(TEXT("Merge applied successfully: %d operations applied"), 
			ApplyResult.AppliedOperations.Num());

		FString ResultMessage = FString::Printf(TEXT("Merge completed successfully!\n\nApplied: %d operations\nWarnings: %d\nCompiled: %s\nSaved: %s"),
			ApplyResult.AppliedOperations.Num(),
			ApplyResult.Warnings.Num(),
			ApplyResult.bBlueprintCompiled ? TEXT("Yes") : TEXT("No"),
			ApplyResult.bBlueprintSaved ? TEXT("Yes") : TEXT("No"));

		if (ApplyResult.Warnings.Num() > 0)
		{
			ResultMessage += TEXT("\n\nWarnings:\n");
			for (const FString& Warning : ApplyResult.Warnings)
			{
				ResultMessage += FString::Printf(TEXT("- %s\n"), *Warning);
			}
		}

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ResultMessage));
	}
	else
	{
		*StatusMessage = FString::Printf(TEXT("Merge failed: %s"), *ApplyResult.ErrorMessage);
		
		FString ErrorMessage = FString::Printf(TEXT("Merge failed!\n\nError: %s\n\nApplied: %d\nFailed: %d"),
			*ApplyResult.ErrorMessage,
			ApplyResult.AppliedOperations.Num(),
			ApplyResult.FailedOperations.Num());

		if (ApplyResult.FailedOperations.Num() > 0)
		{
			ErrorMessage += TEXT("\n\nFailed Operations:\n");
			for (const FString& FailedOp : ApplyResult.FailedOperations)
			{
				ErrorMessage += FString::Printf(TEXT("- %s\n"), *FailedOp);
			}
		}

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage));
	}

	return FReply::Handled();
}

FReply SMergeUI::OnClearAll()
{
	// Use the centralized function to clear all references safely
	ClearAllBlueprintReferences();
	
	// Update additional UI elements
	*PreviewText = TEXT("Select Blueprints to see merge preview...");
	*ConflictSummaryText = TEXT("No conflicts detected");
	*OperationSummaryText = TEXT("No operations to apply");

	return FReply::Handled();
}

FReply SMergeUI::OnExportSnapshots()
{
	// TODO: Implement snapshot export functionality
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Snapshot export functionality will be implemented in a future version.")));
	return FReply::Handled();
}

FReply SMergeUI::OnImportSnapshots()
{
	// TODO: Implement snapshot import functionality
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Snapshot import functionality will be implemented in a future version.")));
	return FReply::Handled();
}

FReply SMergeUI::OnResolveAllConflicts(const FString& Strategy)
{
	if (!bDiffPerformed)
	{
		return FReply::Handled();
	}

	// Update config based on strategy
	if (Strategy == TEXT("UseLocal"))
	{
		MergeConfig.DefaultStrategy = EResolutionStrategy::UseLocal;
	}
	else if (Strategy == TEXT("UseRemote"))
	{
		MergeConfig.DefaultStrategy = EResolutionStrategy::UseRemote;
	}
	else if (Strategy == TEXT("SmartMerge"))
	{
		MergeConfig.DefaultStrategy = EResolutionStrategy::SmartMerge;
	}
	else if (Strategy == TEXT("NonDestructive"))
	{
		MergeConfig.DefaultStrategy = EResolutionStrategy::NonDestructive;
		MergeConfig.bKeepBothConflictingNodes = true; // Enable KeepBoth logic for NonDestructive strategy
	}

	// Recreate merge plan with new strategy
	OnCreateMergePlan();

	return FReply::Handled();
}

void SMergeUI::UpdatePreview()
{
	FString Preview = TEXT("=== MERGE PREVIEW ===\n\n");

	if (bDiffPerformed)
	{
		Preview += FString::Printf(TEXT("Diff Results:\n"));
		Preview += FString::Printf(TEXT("  Operations: %d\n"), CurrentDiffResult.Operations.Num());
		Preview += FString::Printf(TEXT("  Conflicts: %d\n"), CurrentDiffResult.Conflicts.Num());
		Preview += FString::Printf(TEXT("  Has Conflicts: %s\n\n"), CurrentDiffResult.bHasConflicts ? TEXT("Yes") : TEXT("No"));

		if (CurrentDiffResult.Operations.Num() > 0)
		{
			Preview += TEXT("Operations:\n");
			for (int32 i = 0; i < FMath::Min(10, CurrentDiffResult.Operations.Num()); i++)
			{
				const FMergeOperation& Op = CurrentDiffResult.Operations[i];
				Preview += FString::Printf(TEXT("  %d. %s on %s\n"), i + 1, 
					*UEnum::GetValueAsString(Op.OperationType), *Op.TargetId);
			}
			if (CurrentDiffResult.Operations.Num() > 10)
			{
				Preview += FString::Printf(TEXT("  ... and %d more\n"), CurrentDiffResult.Operations.Num() - 10);
			}
			Preview += TEXT("\n");
		}

		if (CurrentDiffResult.Conflicts.Num() > 0)
		{
			Preview += TEXT("Conflicts:\n");
			for (int32 i = 0; i < FMath::Min(5, CurrentDiffResult.Conflicts.Num()); i++)
			{
				const FMergeConflict& Conflict = CurrentDiffResult.Conflicts[i];
				FString SeverityStr = Conflict.Severity == EConflictSeverity::Low ? TEXT("Low") :
									  Conflict.Severity == EConflictSeverity::Medium ? TEXT("Medium") :
									  Conflict.Severity == EConflictSeverity::High ? TEXT("High") : TEXT("Critical");
				Preview += FString::Printf(TEXT("  %d. %s (%s) - %s severity\n"), i + 1, 
					*Conflict.ElementName, *Conflict.ConflictType, *SeverityStr);
			}
			if (CurrentDiffResult.Conflicts.Num() > 5)
			{
				Preview += FString::Printf(TEXT("  ... and %d more\n"), CurrentDiffResult.Conflicts.Num() - 5);
			}
			Preview += TEXT("\n");
		}
	}

	if (bMergePlanCreated)
	{
		Preview += FString::Printf(TEXT("Merge Plan:\n"));
		Preview += FString::Printf(TEXT("  Auto-resolved: %d operations\n"), CurrentMergePlan.AutoResolvedOperations.Num());
		Preview += FString::Printf(TEXT("  Manual review: %d conflicts\n"), CurrentMergePlan.ManualReviewRequired.Num());
		Preview += FString::Printf(TEXT("  Unresolved: %d conflicts\n"), CurrentMergePlan.UnresolvedConflicts.Num());
		Preview += FString::Printf(TEXT("  Requires manual review: %s\n"), CurrentMergePlan.bRequiresManualReview ? TEXT("Yes") : TEXT("No"));
		Preview += TEXT("\n");

		Preview += CurrentMergePlan.PlanSummary;
	}

	*PreviewText = Preview;

	// Update summaries
	if (bDiffPerformed)
	{
		*ConflictSummaryText = FString::Printf(TEXT("Conflicts: %d total"), CurrentDiffResult.Conflicts.Num());
		if (CurrentDiffResult.Conflicts.Num() > 0)
		{
			TMap<EConflictSeverity, int32> SeverityCounts;
			for (const FMergeConflict& Conflict : CurrentDiffResult.Conflicts)
			{
				SeverityCounts.FindOrAdd(Conflict.Severity)++;
			}

			*ConflictSummaryText += TEXT(" (");
			TArray<FString> SeverityStrings;
			for (const auto& Count : SeverityCounts)
			{
				FString SeverityName = Count.Key == EConflictSeverity::Low ? TEXT("Low") :
									   Count.Key == EConflictSeverity::Medium ? TEXT("Medium") :
									   Count.Key == EConflictSeverity::High ? TEXT("High") : TEXT("Critical");
				SeverityStrings.Add(FString::Printf(TEXT("%d %s"), Count.Value, *SeverityName));
			}
			*ConflictSummaryText += FString::Join(SeverityStrings, TEXT(", ")) + TEXT(")");
		}
	}

	if (bMergePlanCreated)
	{
		UE_LOG(LogTemp, Log, TEXT("MergeUI: Updating operation summary - %d auto-resolved, %d manual"), 
			CurrentMergePlan.AutoResolvedOperations.Num(), CurrentMergePlan.ManualReviewRequired.Num());
		
		*OperationSummaryText = FString::Printf(TEXT("Operations: %d auto-resolved, %d manual"), 
			CurrentMergePlan.AutoResolvedOperations.Num(), CurrentMergePlan.ManualReviewRequired.Num());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("MergeUI: Merge plan not created yet, keeping default operation summary"));
	}
}

FText SMergeUI::GetPreviewText() const
{
	return FText::FromString(*PreviewText);
}

FText SMergeUI::GetConflictSummaryText() const
{
	return FText::FromString(*ConflictSummaryText);
}

FText SMergeUI::GetOperationSummaryText() const
{
	return FText::FromString(*OperationSummaryText);
}

FString SMergeUI::GetBlueprintDisplayName(UBlueprint* Blueprint) const
{
	if (!Blueprint)
	{
		return TEXT("Invalid Blueprint");
	}

	return FString::Printf(TEXT("%s (%s)"), *Blueprint->GetName(), *Blueprint->GetPathName());
}

FSlateColor SMergeUI::GetConflictSeverityColor(EConflictSeverity Severity) const
{
	switch (Severity)
	{
	case EConflictSeverity::Low:
		return FLinearColor::Yellow;
	case EConflictSeverity::Medium:
		return FLinearColor(1.0f, 0.5f, 0.0f); // Orange
	case EConflictSeverity::High:
		return FLinearColor::Red;
	case EConflictSeverity::Critical:
		return FLinearColor(0.8f, 0.0f, 0.0f); // Dark Red
	default:
		return FLinearColor::White;
	}
}

FText SMergeUI::GetStatusText() const
{
	return FText::FromString(*StatusMessage);
}

FSlateColor SMergeUI::GetStatusColor() const
{
	if (StatusMessage->Contains(TEXT("Failed")) || StatusMessage->Contains(TEXT("Error")))
	{
		return FLinearColor::Red;
	}
	else if (StatusMessage->Contains(TEXT("Warning")) || StatusMessage->Contains(TEXT("conflicts")))
	{
		return FLinearColor::Yellow;
	}
	else if (StatusMessage->Contains(TEXT("completed")) || StatusMessage->Contains(TEXT("successful")))
	{
		return FLinearColor::Green;
	}
	else
	{
		return FLinearColor::White;
	}
}

EVisibility SMergeUI::GetPerforceSectionVisibility() const
{
	return FPerforceAdapter::IsPerforceAvailable() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMergeUI::GetSelectedBlueprintVisibility() const
{
	return (!SelectedConflictedBlueprint.IsEmpty() && FPerforceAdapter::IsPerforceAvailable()) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMergeUI::GetConflictCountVisibility() const
{
	return (ConflictedBlueprints.Num() > 0 && FPerforceAdapter::IsPerforceAvailable()) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMergeUI::GetPostMergeActionsVisibility() const
{
	return (bMergePlanCreated && !bHasUnresolvedConflicts && FPerforceAdapter::IsPerforceAvailable() && !SelectedConflictedBlueprint.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SMergeUI::ShowBlueprintSelectionDialog(FString& OutSelectedPath)
{
	// Create a custom asset picker dialog
	TSharedRef<SWindow> AssetPickerWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Select Blueprint")))
		.ClientSize(FVector2D(800, 600))
		.SupportsMaximize(true)
		.SupportsMinimize(false)
		.IsTopmostWindow(true);

	FString SelectedAssetPath;
	bool bAssetSelected = false;

	// Create the asset picker content
	TSharedRef<SWidget> AssetPickerContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Select a Blueprint asset:")))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(8.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(5)
			[
				// Create a simple list of available Blueprint assets
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					CreateBlueprintAssetList(SelectedAssetPath, bAssetSelected)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 5, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("OK")))
				.IsEnabled_Lambda([&bAssetSelected]() { return bAssetSelected; })
				.OnClicked_Lambda([&SelectedAssetPath, AssetPickerWindow]() -> FReply
				{
					AssetPickerWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Cancel")))
				.OnClicked_Lambda([AssetPickerWindow]() -> FReply
				{
					AssetPickerWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		];

	AssetPickerWindow->SetContent(AssetPickerContent);
	FSlateApplication::Get().AddModalWindow(AssetPickerWindow, nullptr);

	if (bAssetSelected && !SelectedAssetPath.IsEmpty())
	{
		OutSelectedPath = SelectedAssetPath;
		return true;
	}

	return false;
}

TSharedRef<SWidget> SMergeUI::CreateBlueprintAssetList(FString& OutSelectedPath, bool& bAssetSelected)
{
	// Get all Blueprint assets from the project
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	
	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssets(Filter, BlueprintAssets);
	
	// Create a vertical box to hold all the asset buttons
	TSharedRef<SVerticalBox> AssetListBox = SNew(SVerticalBox);
	
	for (const FAssetData& AssetData : BlueprintAssets)
	{
		FString AssetPath = AssetData.GetObjectPathString();
		FString AssetName = AssetData.AssetName.ToString();
		
		AssetListBox->AddSlot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(AssetName))
			.ToolTipText(FText::FromString(AssetPath))
			.OnClicked_Lambda([&OutSelectedPath, &bAssetSelected, AssetPath]() -> FReply
			{
				OutSelectedPath = AssetPath;
				bAssetSelected = true;
				return FReply::Handled();
			})
		];
	}
	
	// If no assets found, show a message
	if (BlueprintAssets.Num() == 0)
	{
		AssetListBox->AddSlot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No Blueprint assets found in the project.")))
			.ColorAndOpacity(FLinearColor::Red)
		];
	}
	
	return AssetListBox;
}

UBlueprint* SMergeUI::LoadBlueprintFromPath(const FString& Path)
{
	// Validate the path
	if (Path.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot load Blueprint: empty path provided"));
		return nullptr;
	}
	
	// Ensure we're on the game thread
	if (!IsInGameThread())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadBlueprintFromPath must be called from game thread"));
		return nullptr;
	}
	
	// Path should now be a proper asset path from Content Browser (e.g., "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
	UBlueprint* Blueprint = nullptr;
	
	try
	{
		// Use LoadObject with LOAD_NoWarn to avoid warnings about existing objects
		Blueprint = LoadObject<UBlueprint>(nullptr, *Path, nullptr, LOAD_NoWarn);
		
		// Validate the loaded object
		if (Blueprint && IsValid(Blueprint))
		{
			// Check if the Blueprint is in a valid state
			if (Blueprint->IsValidLowLevel() && !Blueprint->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
			{
				UE_LOG(LogTemp, Log, TEXT("Successfully loaded Blueprint: %s from path: %s"), *Blueprint->GetName(), *Path);
				return Blueprint;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Blueprint loaded but is in invalid state: %s"), *Path);
				Blueprint = nullptr;
			}
		}
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception occurred while loading Blueprint from path: %s"), *Path);
		Blueprint = nullptr;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Failed to load Blueprint from path: %s"), *Path);
	return nullptr;
}

void SMergeUI::PopulateBlueprintOptions()
{
	// Clear existing options (except the placeholder)
	BlueprintOptions.Empty();
	BlueprintOptions.Add(MakeShareable(new FString(TEXT("Select a Blueprint..."))));
	BlueprintNameToPathMap.Empty();
	
	// Get all Blueprint assets from the project
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	
	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssets(Filter, BlueprintAssets);
	
	// Add all found blueprints to the options
	for (const FAssetData& AssetData : BlueprintAssets)
	{
		FString AssetName = AssetData.AssetName.ToString();
		FString AssetPath = AssetData.GetObjectPathString();
		
		// Store the name in the combo box options
		BlueprintOptions.Add(MakeShareable(new FString(AssetName)));
		// Store the mapping from name to path
		BlueprintNameToPathMap.Add(AssetName, AssetPath);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Populated %d Blueprint options"), BlueprintOptions.Num() - 1);
}

void SMergeUI::OnBaseBlueprintSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid() || NewSelection == BlueprintOptions[0])
	{
		return; // Don't process the placeholder option
	}
	
	SelectedBaseBlueprint = NewSelection;
	
	// Get the asset path from the name
	FString AssetName = *NewSelection;
	FString* AssetPathPtr = BlueprintNameToPathMap.Find(AssetName);
	if (!AssetPathPtr)
	{
		*StatusMessage = TEXT("Failed to find Blueprint path");
		SelectedBaseBlueprint = BlueprintOptions[0]; // Reset to placeholder
		return;
	}
	
	FString AssetPath = *AssetPathPtr;
	UBlueprint* Blueprint = LoadBlueprintFromPath(AssetPath);
	if (Blueprint && IsValid(Blueprint))
	{
		// Clear any existing reference first
		BaseBlueprint.Reset();
		
		BaseBlueprint = Blueprint;
		*BasePathText = FString::Printf(TEXT("Base: %s"), *GetBlueprintDisplayName(Blueprint));
		*StatusMessage = TEXT("Base Blueprint loaded");
		
		// Create snapshot with error handling
		try
		{
			if (FSnapshotManager::CreateSnapshot(Blueprint, BaseSnapshot))
			{
				UE_LOG(LogTemp, Log, TEXT("Base Blueprint snapshot created"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create Base Blueprint snapshot"));
				*StatusMessage = TEXT("Base Blueprint loaded but snapshot failed");
			}
		}
		catch (...)
		{
			UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating Base Blueprint snapshot"));
			*StatusMessage = TEXT("Base Blueprint loaded but snapshot failed");
		}
	}
	else
	{
		*StatusMessage = TEXT("Failed to load Base Blueprint");
		SelectedBaseBlueprint = BlueprintOptions[0]; // Reset to placeholder
	}
}

void SMergeUI::OnLocalBlueprintSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid() || NewSelection == BlueprintOptions[0])
	{
		return; // Don't process the placeholder option
	}
	
	SelectedLocalBlueprint = NewSelection;
	
	// Get the asset path from the name
	FString AssetName = *NewSelection;
	FString* AssetPathPtr = BlueprintNameToPathMap.Find(AssetName);
	if (!AssetPathPtr)
	{
		*StatusMessage = TEXT("Failed to find Blueprint path");
		SelectedLocalBlueprint = BlueprintOptions[0]; // Reset to placeholder
		return;
	}
	
	FString AssetPath = *AssetPathPtr;
	UBlueprint* Blueprint = LoadBlueprintFromPath(AssetPath);
	if (Blueprint && IsValid(Blueprint))
	{
		// Clear any existing reference first
		LocalBlueprint.Reset();
		
		LocalBlueprint = Blueprint;
		*LocalPathText = FString::Printf(TEXT("Local: %s"), *GetBlueprintDisplayName(Blueprint));
		*StatusMessage = TEXT("Local Blueprint loaded");

		// Create snapshot with error handling
		try
		{
			if (FSnapshotManager::CreateSnapshot(Blueprint, LocalSnapshot))
			{
				UE_LOG(LogTemp, Log, TEXT("Local Blueprint snapshot created"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create Local Blueprint snapshot"));
				*StatusMessage = TEXT("Local Blueprint loaded but snapshot failed");
			}
		}
		catch (...)
		{
			UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating Local Blueprint snapshot"));
			*StatusMessage = TEXT("Local Blueprint loaded but snapshot failed");
		}
	}
	else
	{
		*StatusMessage = TEXT("Failed to load Local Blueprint");
		SelectedLocalBlueprint = BlueprintOptions[0]; // Reset to placeholder
	}
}

void SMergeUI::OnRemoteBlueprintSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid() || NewSelection == BlueprintOptions[0])
	{
		return; // Don't process the placeholder option
	}
	
	SelectedRemoteBlueprint = NewSelection;
	
	// Get the asset path from the name
	FString AssetName = *NewSelection;
	FString* AssetPathPtr = BlueprintNameToPathMap.Find(AssetName);
	if (!AssetPathPtr)
	{
		*StatusMessage = TEXT("Failed to find Blueprint path");
		SelectedRemoteBlueprint = BlueprintOptions[0]; // Reset to placeholder
		return;
	}
	
	FString AssetPath = *AssetPathPtr;
	UBlueprint* Blueprint = LoadBlueprintFromPath(AssetPath);
	if (Blueprint && IsValid(Blueprint))
	{
		// Clear any existing reference first
		RemoteBlueprint.Reset();
		
		RemoteBlueprint = Blueprint;
		*RemotePathText = FString::Printf(TEXT("Remote: %s"), *GetBlueprintDisplayName(Blueprint));
		*StatusMessage = TEXT("Remote Blueprint loaded");

		// Create snapshot with error handling
		try
		{
			if (FSnapshotManager::CreateSnapshot(Blueprint, RemoteSnapshot))
			{
				UE_LOG(LogTemp, Log, TEXT("Remote Blueprint snapshot created"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create Remote Blueprint snapshot"));
				*StatusMessage = TEXT("Remote Blueprint loaded but snapshot failed");
			}
		}
		catch (...)
		{
			UE_LOG(LogTemp, Error, TEXT("Exception occurred while creating Remote Blueprint snapshot"));
			*StatusMessage = TEXT("Remote Blueprint loaded but snapshot failed");
		}
	}
	else
	{
		*StatusMessage = TEXT("Failed to load Remote Blueprint");
		SelectedRemoteBlueprint = BlueprintOptions[0]; // Reset to placeholder
	}
}

void SMergeUI::ClearAllBlueprintReferences()
{
	UE_LOG(LogTemp, Log, TEXT("Clearing all Blueprint references to prevent crashes after renaming"));
	
	// Clear all Blueprint references
	BaseBlueprint.Reset();
	LocalBlueprint.Reset();
	RemoteBlueprint.Reset();
	
	// Clear all snapshots
	BaseSnapshot.Reset();
	LocalSnapshot.Reset();
	RemoteSnapshot.Reset();
	
	// Reset UI state
	bDiffPerformed = false;
	bMergePlanCreated = false;
	bHasUnresolvedConflicts = false;
	
	// Clear path text
	*BasePathText = TEXT("No base Blueprint selected");
	*LocalPathText = TEXT("No local Blueprint selected");
	*RemotePathText = TEXT("No remote Blueprint selected");
	
	// Reset combo box selections
	if (BlueprintOptions.Num() > 0)
	{
		SelectedBaseBlueprint = BlueprintOptions[0];
		SelectedLocalBlueprint = BlueprintOptions[0];
		SelectedRemoteBlueprint = BlueprintOptions[0];
	}
	
	// Clear status
	*StatusMessage = TEXT("All references cleared. Please reload Blueprints after renaming.");
	
	// Clear diff and merge data
	CurrentDiffResult = FDiffResult();
	CurrentMergePlan = FMergePlan();
	
	UE_LOG(LogTemp, Log, TEXT("All Blueprint references cleared successfully"));
}

FReply SMergeUI::OnResolveConflict(int32 ConflictIndex, const FString& Resolution)
{
	// TODO: Implement individual conflict resolution
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Individual conflict resolution will be implemented in a future version.")));
	return FReply::Handled();
}

FReply SMergeUI::OnDetectPerforceConflicts()
{
	if (!FPerforceAdapter::IsPerforceAvailable())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("Perforce is not configured. Please set up Perforce in Editor Preferences → Source Control.")));
		return FReply::Handled();
	}

	*StatusMessage = TEXT("Scanning for conflicted Blueprints...");
	
	ConflictedBlueprints.Empty();
	int32 ConflictCount = FPerforceAdapter::DetectConflictedBlueprints(ConflictedBlueprints);

	if (ConflictCount > 0)
	{
		*StatusMessage = FString::Printf(TEXT("Found %d conflicted Blueprint(s)"), ConflictCount);
		
		// Show dialog to select a conflicted Blueprint
		FString SelectedBlueprint;
		bool bBlueprintSelected = false;
		
		// Create a dialog window for selecting conflicted Blueprint
		TSharedRef<SWindow> ConflictPickerWindow = SNew(SWindow)
			.Title(FText::FromString(TEXT("Select Conflicted Blueprint")))
			.ClientSize(FVector2D(600, 400))
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		TSharedRef<SVerticalBox> ConflictListBox = SNew(SVerticalBox);
		
		for (const FString& BlueprintPath : ConflictedBlueprints)
		{
			ConflictListBox->AddSlot()
				.AutoHeight()
				.Padding(2)
				[
					SNew(SButton)
					.Text(FText::FromString(BlueprintPath))
					.HAlign(HAlign_Left)
					.OnClicked_Lambda([&SelectedBlueprint, &bBlueprintSelected, BlueprintPath, ConflictPickerWindow]() -> FReply
					{
						SelectedBlueprint = BlueprintPath;
						bBlueprintSelected = true;
						ConflictPickerWindow->RequestDestroyWindow();
						return FReply::Handled();
					})
				];
		}
		
		TSharedRef<SWidget> ConflictPickerContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(10)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(5)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						ConflictListBox
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Cancel")))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([ConflictPickerWindow]() -> FReply
				{
					ConflictPickerWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			];

		ConflictPickerWindow->SetContent(ConflictPickerContent);
		FSlateApplication::Get().AddModalWindow(ConflictPickerWindow, nullptr);

		if (bBlueprintSelected && !SelectedBlueprint.IsEmpty())
		{
			OnConflictedBlueprintSelected(SelectedBlueprint);
		}
		
		UE_LOG(LogTemp, Log, TEXT("Found %d conflicted Blueprints"), ConflictCount);
	}
	else
	{
		*StatusMessage = TEXT("No conflicted Blueprints found");
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("No conflicted Blueprints found. You can still use manual Blueprint selection below.")));
	}

	return FReply::Handled();
}

FReply SMergeUI::OnLoadFromPerforce()
{
	if (SelectedConflictedBlueprint.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("Please select a conflicted Blueprint first by clicking 'Detect Conflicts' and selecting one from the list.")));
		return FReply::Handled();
	}

	*StatusMessage = TEXT("Loading Blueprint versions from Perforce...");

	UBlueprint* BaseBP = nullptr;
	UBlueprint* LocalBP = nullptr;
	UBlueprint* RemoteBP = nullptr;
	TSharedPtr<FJsonObject> BaseSnap, LocalSnap, RemoteSnap;
	FString Error;

	if (FPerforceAdapter::LoadAllVersions(SelectedConflictedBlueprint, BaseBP, LocalBP, RemoteBP, BaseSnap, LocalSnap, RemoteSnap, Error))
	{
		// Clear existing references
		ClearAllBlueprintReferences();

		// Set new references
		BaseBlueprint = BaseBP;
		LocalBlueprint = LocalBP;
		RemoteBlueprint = RemoteBP;
		
		BaseSnapshot = BaseSnap;
		LocalSnapshot = LocalSnap;
		RemoteSnapshot = RemoteSnap;

		// Update UI text
		*BasePathText = FString::Printf(TEXT("Base: %s (from Perforce)"), *GetBlueprintDisplayName(BaseBP));
		*LocalPathText = FString::Printf(TEXT("Local: %s (from Perforce)"), *GetBlueprintDisplayName(LocalBP));
		*RemotePathText = FString::Printf(TEXT("Remote: %s (from Perforce)"), *GetBlueprintDisplayName(RemoteBP));

		*StatusMessage = TEXT("Successfully loaded all versions from Perforce. Click 'Perform Diff' to continue.");
		
		UE_LOG(LogTemp, Log, TEXT("Successfully loaded all versions from Perforce for: %s"), *SelectedConflictedBlueprint);
	}
	else
	{
		*StatusMessage = FString::Printf(TEXT("Failed to load versions: %s"), *Error);
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(FString::Printf(TEXT("Failed to load Blueprint versions from Perforce:\n\n%s"), *Error)));
	}

	return FReply::Handled();
}

void SMergeUI::OnConflictedBlueprintSelected(FString BlueprintPath)
{
	SelectedConflictedBlueprint = BlueprintPath;
	*StatusMessage = FString::Printf(TEXT("Selected: %s - Click 'Load from Perforce' to load versions"), *BlueprintPath);
	UE_LOG(LogTemp, Log, TEXT("Selected conflicted Blueprint: %s"), *BlueprintPath);
}

FReply SMergeUI::OnResolveInPerforce()
{
	if (SelectedConflictedBlueprint.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("No Blueprint selected. Please load a Blueprint from Perforce first.")));
		return FReply::Handled();
	}

	FString FilePath = FPerforceAdapter::AssetPathToFilePath(SelectedConflictedBlueprint);
	if (FilePath.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("Could not convert asset path to file path.")));
		return FReply::Handled();
	}

	FString Error;
	if (FPerforceAdapter::ResolveConflict(FilePath, EPerforceResolveMethod::AcceptMerge, Error))
	{
		*StatusMessage = TEXT("Conflict resolved in Perforce successfully!");
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("Conflict has been marked as resolved in Perforce. You can now submit your changes.")));
		
		UE_LOG(LogTemp, Log, TEXT("Resolved conflict in Perforce for: %s"), *SelectedConflictedBlueprint);
	}
	else
	{
		*StatusMessage = FString::Printf(TEXT("Failed to resolve: %s"), *Error);
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(FString::Printf(TEXT("Failed to resolve conflict in Perforce:\n\n%s"), *Error)));
	}

	return FReply::Handled();
}

FReply SMergeUI::OnSubmitToPerforce()
{
	if (SelectedConflictedBlueprint.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("No Blueprint selected. Please load a Blueprint from Perforce first.")));
		return FReply::Handled();
	}

	FString FilePath = FPerforceAdapter::AssetPathToFilePath(SelectedConflictedBlueprint);
	if (FilePath.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("Could not convert asset path to file path.")));
		return FReply::Handled();
	}

	// Save the Blueprint before submitting
	if (LocalBlueprint.IsValid())
	{
		UPackage* Package = LocalBlueprint->GetOutermost();
		if (Package)
		{
			FString PackageName = Package->GetName();
			FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.SaveFlags = RF_Standalone;
			UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);
		}
	}

	TArray<FString> FilesToSubmit;
	FilesToSubmit.Add(FilePath);

	FString Description = FString::Printf(TEXT("Resolved merge conflicts for %s using Blueprint Merge Tool"), *SelectedConflictedBlueprint);
	
	FString Error;
	if (FPerforceAdapter::SubmitFiles(FilesToSubmit, Description, Error))
	{
		*StatusMessage = TEXT("Blueprint submitted to Perforce successfully!");
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(TEXT("Blueprint has been submitted to Perforce successfully!")));
		
		// Clear the selection after successful submit
		SelectedConflictedBlueprint.Empty();
		
		UE_LOG(LogTemp, Log, TEXT("Submitted to Perforce: %s"), *SelectedConflictedBlueprint);
	}
	else
	{
		*StatusMessage = FString::Printf(TEXT("Failed to submit: %s"), *Error);
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(FString::Printf(TEXT("Failed to submit to Perforce:\n\n%s"), *Error)));
	}

	return FReply::Handled();
}
