#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "DiffEngine.h"
#include "MergePlanner.h"
#include "ApplyEngine.h"
#include "SnapshotManager.h"
#include "Engine/Blueprint.h"

/**
 * Main UI widget for Blueprint merging operations
 * Provides interface for:
 * - Loading and comparing Blueprints
 * - Showing conflicts and preview
 * - Manual conflict resolution
 * - Applying merge operations
 */
class BLUEPRINTMERGETOOL_API SMergeUI : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMergeUI) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// File selection
	TSharedRef<SWidget> CreateFileSelectionSection();
	TSharedRef<SWidget> CreateConflictResolutionSection();
	TSharedRef<SWidget> CreatePreviewSection();
	TSharedRef<SWidget> CreateActionButtonsSection();

	// Event handlers
	FReply OnLoadBaseBlueprint();
	FReply OnLoadLocalBlueprint();
	FReply OnLoadRemoteBlueprint();
	FReply OnPerformDiff();
	FReply OnCreateMergePlan();
	FReply OnApplyMerge();
	FReply OnClearAll();
	FReply OnExportSnapshots();
	FReply OnImportSnapshots();

	// Conflict resolution
	FReply OnResolveConflict(int32 ConflictIndex, const FString& Resolution);
	FReply OnResolveAllConflicts(const FString& Strategy);

	// Preview functions
	void UpdatePreview();
	FText GetPreviewText() const;
	FText GetConflictSummaryText() const;
	FText GetOperationSummaryText() const;

	// Utility functions
	FString GetBlueprintDisplayName(UBlueprint* Blueprint) const;
	FSlateColor GetConflictSeverityColor(EConflictSeverity Severity) const;
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;

	// File dialog helpers
	bool ShowBlueprintSelectionDialog(FString& OutSelectedPath);
	UBlueprint* LoadBlueprintFromPath(const FString& Path);

private:
	// Blueprint references
	TWeakObjectPtr<UBlueprint> BaseBlueprint;
	TWeakObjectPtr<UBlueprint> LocalBlueprint;
	TWeakObjectPtr<UBlueprint> RemoteBlueprint;

	// Snapshots
	TSharedPtr<FJsonObject> BaseSnapshot;
	TSharedPtr<FJsonObject> LocalSnapshot;
	TSharedPtr<FJsonObject> RemoteSnapshot;

	// Diff and merge data
	FDiffResult CurrentDiffResult;
	FMergePlan CurrentMergePlan;
	FMergePlannerConfig MergeConfig;

	// UI state
	TSharedPtr<FString> StatusMessage;
	TSharedPtr<FString> PreviewText;
	TSharedPtr<FString> ConflictSummaryText;
	TSharedPtr<FString> OperationSummaryText;

	// Selected paths
	TSharedPtr<FString> BasePathText;
	TSharedPtr<FString> LocalPathText;
	TSharedPtr<FString> RemotePathText;

	// UI flags
	bool bDiffPerformed;
	bool bMergePlanCreated;
	bool bHasUnresolvedConflicts;
};
