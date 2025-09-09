/**
 * Integration example showing how to use Blueprint Merge Tool with UnrealAI
 */

#include "CoreMinimal.h"

// Blueprint Merge Tool includes
#include "../Source/BlueprintMergeTool/Public/SnapshotManager.h"
#include "../Source/BlueprintMergeTool/Public/DiffEngine.h"
#include "../Source/BlueprintMergeTool/Public/MergePlanner.h"
#include "../Source/BlueprintMergeTool/Public/UnrealAILLMAdapter.h"

// UnrealAI includes (if available)
// #include "../../UnrealAI/Source/UnrealAI/UnrealAIService.h"

/**
 * Example showing AI-powered Blueprint merging
 */
void RunAIAssistedMergeExample()
{
    UE_LOG(LogTemp, Log, TEXT("=== AI-Assisted Blueprint Merge Example ==="));

    // Create and configure the AI adapter
    TSharedPtr<FUnrealAILLMAdapter> AIAdapter = MakeShareable(new FUnrealAILLMAdapter());
    AIAdapter->SetAIEndpoint(TEXT("http://localhost:11434/api/generate"));
    AIAdapter->SetEnabled(true);

    // Set the adapter for the merge planner
    FMergePlanner::SetLLMAdapter(AIAdapter);

    // Configure merge planner to use AI for complex conflicts
    FMergePlannerConfig Config;
    Config.DefaultStrategy = EResolutionStrategy::SmartMerge;
    Config.bEnableLLMResolution = true;
    Config.ConflictThresholdForManualReview = 0.8f; // Only highest severity conflicts need manual review

    UE_LOG(LogTemp, Log, TEXT("✅ AI adapter configured and ready"));

    // Example workflow would continue with normal merge process...
    // The AI will automatically be consulted for unresolved conflicts
}

/**
 * Example showing how to extend the system with custom resolution rules
 */
class FCustomMergeResolver
{
public:
    /**
     * Custom resolution for specific Blueprint types
     */
    static bool ResolveCustomConflicts(
        const TArray<FMergeConflict>& Conflicts,
        const FString& BlueprintType,
        TArray<FMergeOperation>& OutOperations)
    {
        for (const FMergeConflict& Conflict : Conflicts)
        {
            if (BlueprintType == TEXT("PlayerController"))
            {
                // Custom rules for PlayerController Blueprints
                if (Conflict.ConflictType == TEXT("Variable") && 
                    Conflict.ElementName.Contains(TEXT("Input")))
                {
                    // For input-related variables, prefer local changes
                    FMergeOperation Op = FDiffEngine::CreateOperation(
                        EMergeOperationType::UpdateVariable,
                        TEXT(""),
                        Conflict.ConflictId
                    );
                    Op.AdditionalData.Add(TEXT("Resolution"), TEXT("Local"));
                    Op.AdditionalData.Add(TEXT("Reason"), TEXT("InputVariableLocalPreference"));
                    OutOperations.Add(Op);
                }
            }
            else if (BlueprintType == TEXT("GameMode"))
            {
                // Custom rules for GameMode Blueprints
                if (Conflict.ConflictType == TEXT("Variable") && 
                    Conflict.ElementName.Contains(TEXT("Score")))
                {
                    // For score-related variables, prefer remote changes
                    FMergeOperation Op = FDiffEngine::CreateOperation(
                        EMergeOperationType::UpdateVariable,
                        TEXT(""),
                        Conflict.ConflictId
                    );
                    Op.AdditionalData.Add(TEXT("Resolution"), TEXT("Remote"));
                    Op.AdditionalData.Add(TEXT("Reason"), TEXT("ScoreVariableRemotePreference"));
                    OutOperations.Add(Op);
                }
            }
        }

        return OutOperations.Num() > 0;
    }
};

/**
 * Example showing custom merge workflow with specialized rules
 */
void RunCustomMergeWorkflow()
{
    UE_LOG(LogTemp, Log, TEXT("=== Custom Merge Workflow Example ==="));

    // This example shows how to implement custom merge logic
    // for specific Blueprint types or conflict scenarios

    // 1. Detect Blueprint type from snapshot
    // 2. Apply custom resolution rules
    // 3. Fall back to standard merge planner for unresolved conflicts
    // 4. Apply operations with custom validation

    UE_LOG(LogTemp, Log, TEXT("Custom merge workflow would be implemented here"));
}

/**
 * Console commands for testing the integration
 */
static FAutoConsoleCommand RunAIMergeCommand(
    TEXT("BlueprintMergeTool.RunAIExample"),
    TEXT("Run AI-assisted merge example"),
    FConsoleCommandDelegate::CreateStatic(RunAIAssistedMergeExample)
);

static FAutoConsoleCommand RunCustomMergeCommand(
    TEXT("BlueprintMergeTool.RunCustomExample"),
    TEXT("Run custom merge workflow example"),
    FConsoleCommandDelegate::CreateStatic(RunCustomMergeWorkflow)
);
