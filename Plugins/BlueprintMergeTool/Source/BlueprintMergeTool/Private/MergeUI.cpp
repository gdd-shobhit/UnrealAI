#include "../Public/MergeUI.h"
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

	// Initialize state
	bDiffPerformed = false;
	bMergePlanCreated = false;
	bHasUnresolvedConflicts = false;

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
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(*BasePathText);
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Select Base")))
				.OnClicked(this, &SMergeUI::OnLoadBaseBlueprint)
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
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(*LocalPathText);
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Select Local")))
				.OnClicked(this, &SMergeUI::OnLoadLocalBlueprint)
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
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(*RemotePathText);
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Select Remote")))
				.OnClicked(this, &SMergeUI::OnLoadRemoteBlueprint)
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
				.ToolTipText(FText::FromString(TEXT("Resolve conflicts with non-destructive strategy")))
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
				.Text(FText::FromString(TEXT("✅ Apply Merge")))
				.OnClicked(this, &SMergeUI::OnApplyMerge)
				.IsEnabled_Lambda([this]() -> bool
				{
					return bMergePlanCreated && !bHasUnresolvedConflicts;
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
		if (Blueprint)
		{
			BaseBlueprint = Blueprint;
			*BasePathText = FString::Printf(TEXT("Base: %s"), *GetBlueprintDisplayName(Blueprint));
			*StatusMessage = TEXT("Base Blueprint loaded");
			
			// Create snapshot
			if (FSnapshotManager::CreateSnapshot(Blueprint, BaseSnapshot))
			{
				UE_LOG(LogTemp, Log, TEXT("Base Blueprint snapshot created"));
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
		if (Blueprint)
		{
			LocalBlueprint = Blueprint;
			*LocalPathText = FString::Printf(TEXT("Local: %s"), *GetBlueprintDisplayName(Blueprint));
			*StatusMessage = TEXT("Local Blueprint loaded");

			// Create snapshot
			if (FSnapshotManager::CreateSnapshot(Blueprint, LocalSnapshot))
			{
				UE_LOG(LogTemp, Log, TEXT("Local Blueprint snapshot created"));
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
		if (Blueprint)
		{
			RemoteBlueprint = Blueprint;
			*RemotePathText = FString::Printf(TEXT("Remote: %s"), *GetBlueprintDisplayName(Blueprint));
			*StatusMessage = TEXT("Remote Blueprint loaded");

			// Create snapshot
			if (FSnapshotManager::CreateSnapshot(Blueprint, RemoteSnapshot))
			{
				UE_LOG(LogTemp, Log, TEXT("Remote Blueprint snapshot created"));
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
	if (!BaseBlueprint.IsValid() || !LocalBlueprint.IsValid() || !RemoteBlueprint.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please load all three Blueprints first!")));
		return FReply::Handled();
	}

	*StatusMessage = TEXT("Reloading Blueprints and performing three-way diff...");

	// Reload Blueprints from disk to get latest changes
	FString BasePath = BaseBlueprint->GetPathName();
	FString LocalPath = LocalBlueprint->GetPathName();
	FString RemotePath = RemoteBlueprint->GetPathName();

	UE_LOG(LogTemp, Log, TEXT("Reloading Blueprints from disk:"));
	UE_LOG(LogTemp, Log, TEXT("  Base: %s"), *BasePath);
	UE_LOG(LogTemp, Log, TEXT("  Local: %s"), *LocalPath);
	UE_LOG(LogTemp, Log, TEXT("  Remote: %s"), *RemotePath);

	// Force reload from disk by clearing the object cache and reloading
	UBlueprint* ReloadedBase = ReloadBlueprintFromDisk(BasePath);
	UBlueprint* ReloadedLocal = ReloadBlueprintFromDisk(LocalPath);
	UBlueprint* ReloadedRemote = ReloadBlueprintFromDisk(RemotePath);

	if (!ReloadedBase || !ReloadedLocal || !ReloadedRemote)
	{
		*StatusMessage = TEXT("Failed to reload Blueprints from disk");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to reload Blueprints from disk. Check the log for details.")));
		return FReply::Handled();
	}

	// Update our Blueprint references
	BaseBlueprint = ReloadedBase;
	LocalBlueprint = ReloadedLocal;
	RemoteBlueprint = ReloadedRemote;

	// Create fresh snapshots from the reloaded Blueprints
	UE_LOG(LogTemp, Log, TEXT("Creating fresh snapshots from reloaded Blueprints"));
	if (!FSnapshotManager::CreateSnapshot(ReloadedBase, BaseSnapshot) ||
		!FSnapshotManager::CreateSnapshot(ReloadedLocal, LocalSnapshot) ||
		!FSnapshotManager::CreateSnapshot(ReloadedRemote, RemoteSnapshot))
	{
		*StatusMessage = TEXT("Failed to create snapshots from reloaded Blueprints");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to create snapshots from reloaded Blueprints. Check the log for details.")));
		return FReply::Handled();
	}

	// Perform the diff with fresh snapshots
	if (FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, CurrentDiffResult))
	{
		bDiffPerformed = true;
		*StatusMessage = FString::Printf(TEXT("Diff completed with fresh data: %d operations, %d conflicts"), 
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
	EAppReturnType::Type Target = FMessageDialog::Open(EAppMsgType::YesNoCancel,
		FText::FromString(TEXT("Which Blueprint should receive the merged changes?\n\nYes = Local Blueprint\nNo = Remote Blueprint\nCancel = Abort")));

	UBlueprint* TargetBlueprint = nullptr;
	if (Target == EAppReturnType::Yes)
	{
		TargetBlueprint = LocalBlueprint.Get();
	}
	else if (Target == EAppReturnType::No)
	{
		TargetBlueprint = RemoteBlueprint.Get();
	}
	else
	{
		return FReply::Handled(); // Cancelled
	}

	if (!TargetBlueprint)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Target Blueprint is no longer valid!")));
		return FReply::Handled();
	}

	*StatusMessage = TEXT("Applying merge operations...");

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
	BaseBlueprint.Reset();
	LocalBlueprint.Reset();
	RemoteBlueprint.Reset();
	
	BaseSnapshot.Reset();
	LocalSnapshot.Reset();
	RemoteSnapshot.Reset();

	CurrentDiffResult = FDiffResult();
	CurrentMergePlan = FMergePlan();

	*BasePathText = TEXT("No base Blueprint selected");
	*LocalPathText = TEXT("No local Blueprint selected");
	*RemotePathText = TEXT("No remote Blueprint selected");
	*StatusMessage = TEXT("All data cleared");
	*PreviewText = TEXT("Select Blueprints to see merge preview...");
	*ConflictSummaryText = TEXT("No conflicts detected");
	*OperationSummaryText = TEXT("No operations to apply");

	bDiffPerformed = false;
	bMergePlanCreated = false;
	bHasUnresolvedConflicts = false;

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
	// Path should now be a proper asset path from Content Browser (e.g., "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
	
	if (Blueprint)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully loaded Blueprint: %s from path: %s"), *Blueprint->GetName(), *Path);
		return Blueprint;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Failed to load Blueprint from path: %s"), *Path);
	return nullptr;
}

UBlueprint* SMergeUI::ReloadBlueprintFromDisk(const FString& Path)
{
	UE_LOG(LogTemp, Log, TEXT("Reloading Blueprint from disk: %s"), *Path);
	
	// First, try to find the existing object and mark it for garbage collection
	UBlueprint* ExistingBlueprint = FindObject<UBlueprint>(nullptr, *Path);
	if (ExistingBlueprint)
	{
		UE_LOG(LogTemp, Log, TEXT("Found existing Blueprint object, forcing reload from disk"));
		
		// Mark the existing object for garbage collection
		ExistingBlueprint->MarkAsGarbage();
		
		// Force garbage collection to clear the object from memory
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	
	// Now reload from disk
	UBlueprint* ReloadedBlueprint = LoadObject<UBlueprint>(nullptr, *Path);
	
	if (ReloadedBlueprint)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully reloaded Blueprint from disk: %s"), *ReloadedBlueprint->GetName());
		
		// Compile the Blueprint to ensure it's in a valid state
		FKismetEditorUtilities::CompileBlueprint(ReloadedBlueprint, EBlueprintCompileOptions::None);
		
		return ReloadedBlueprint;
	}
	
	UE_LOG(LogTemp, Error, TEXT("Failed to reload Blueprint from disk: %s"), *Path);
	return nullptr;
}

FReply SMergeUI::OnResolveConflict(int32 ConflictIndex, const FString& Resolution)
{
	// TODO: Implement individual conflict resolution
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Individual conflict resolution will be implemented in a future version.")));
	return FReply::Handled();
}
