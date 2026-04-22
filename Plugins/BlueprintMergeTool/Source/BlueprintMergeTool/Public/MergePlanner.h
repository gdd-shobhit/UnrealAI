#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "DiffEngine.h"
#include "Dom/JsonObject.h"
#include "MergePlanner.generated.h"

/**
 * Automatic resolution strategies for conflicts
 */
UENUM(BlueprintType)
enum class EResolutionStrategy : uint8
{
	UseLocal,           // Always prefer local changes
	UseRemote,          // Always prefer remote changes
	UseBase,            // Revert to base version
	NonDestructive,     // Combine changes when possible, prefer additions
	ManualResolve,      // Requires user intervention
	LLMResolve,         // Use AI/LLM to resolve
	SmartMerge          // Intelligent merging based on context
};

/**
 * Configuration for automatic merge planning
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FMergePlannerConfig
{
	GENERATED_BODY()

	UPROPERTY()
	EResolutionStrategy DefaultStrategy;

	UPROPERTY()
	bool bPreferLocalForVariables;

	UPROPERTY()
	bool bPreferRemoteForNodes;

	UPROPERTY()
	bool bAutoResolvePositionConflicts;

	UPROPERTY()
	bool bEnableLLMResolution;

	UPROPERTY()
	float ConflictThresholdForManualReview;

	UPROPERTY()
	TMap<FString, EResolutionStrategy> PerTypeStrategies;

	UPROPERTY()
	bool bKeepBothConflictingNodes;

	FMergePlannerConfig()
		: DefaultStrategy(EResolutionStrategy::NonDestructive)
		, bPreferLocalForVariables(true)
		, bPreferRemoteForNodes(false)
		, bAutoResolvePositionConflicts(true)
		, bEnableLLMResolution(true)
		, ConflictThresholdForManualReview(0.7f)
		, bKeepBothConflictingNodes(false)
	{
	}
};

/**
 * Result of merge planning
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FMergePlan
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMergeOperation> AutoResolvedOperations;

	UPROPERTY()
	TArray<FMergeConflict> UnresolvedConflicts;

	UPROPERTY()
	TArray<FMergeConflict> ManualReviewRequired;

	UPROPERTY()
	FString PlanSummary;

	UPROPERTY()
	bool bRequiresManualReview;

	UPROPERTY()
	bool bRequiresLLMResolution;

	FMergePlan()
		: bRequiresManualReview(false)
		, bRequiresLLMResolution(false)
	{
	}
};

/**
 * Interface for LLM resolution adapter
 */
class BLUEPRINTMERGETOOL_API ILLMAdapter
{
public:
	virtual ~ILLMAdapter() = default;

	/**
	 * Resolve conflicts using LLM
	 * @param Conflicts Array of conflicts to resolve
	 * @param Context Additional context for resolution
	 * @param OutResolvedOperations Resulting operations
	 * @return True if resolution was successful
	 */
	virtual bool ResolveConflicts(
		const TArray<FMergeConflict>& Conflicts,
		const FString& Context,
		TArray<FMergeOperation>& OutResolvedOperations
	) = 0;

	/**
	 * Check if LLM is available
	 * @return True if LLM can be used
	 */
	virtual bool IsAvailable() const = 0;
};

/**
 * Automatic resolution rules + LLM fallback for merge conflicts
 * Produces concrete merged operations plan from diff results
 */
class BLUEPRINTMERGETOOL_API FMergePlanner
{
public:
	/**
	 * Create a merge plan from diff results
	 * @param DiffResult The diff containing operations and conflicts
	 * @param Config Configuration for merge planning
	 * @param OutMergePlan The resulting merge plan
	 * @return True if planning was successful
	 */
	static bool CreateMergePlan(
		const FDiffResult& DiffResult,
		const FMergePlannerConfig& Config,
		FMergePlan& OutMergePlan
	);

	/**
	 * Apply automatic resolution rules to conflicts
	 * @param Conflicts Input conflicts to resolve
	 * @param Config Resolution configuration
	 * @param OutResolvedOperations Operations from resolved conflicts
	 * @param OutUnresolvedConflicts Conflicts that couldn't be auto-resolved
	 */
	static void ApplyAutomaticResolution(
		const TArray<FMergeConflict>& Conflicts,
		const FMergePlannerConfig& Config,
		TArray<FMergeOperation>& OutResolvedOperations,
		TArray<FMergeConflict>& OutUnresolvedConflicts
	);

	/**
	 * Use LLM to resolve remaining conflicts
	 * @param Conflicts Conflicts to resolve with LLM
	 * @param LLMAdapter The LLM adapter to use
	 * @param Context Additional context for the LLM
	 * @param OutResolvedOperations Operations from LLM resolution
	 * @param OutFailedConflicts Conflicts the LLM couldn't resolve
	 */
	static bool UseLLMResolution(
		const TArray<FMergeConflict>& Conflicts,
		ILLMAdapter* LLMAdapter,
		const FString& Context,
		TArray<FMergeOperation>& OutResolvedOperations,
		TArray<FMergeConflict>& OutFailedConflicts
	);

	/**
	 * Set the LLM adapter for conflict resolution
	 * @param Adapter The adapter to use
	 */
	static void SetLLMAdapter(TSharedPtr<ILLMAdapter> Adapter);

	/**
	 * Get the current LLM adapter
	 * @return Current adapter or nullptr
	 */
	static TSharedPtr<ILLMAdapter> GetLLMAdapter();

	/**
	 * Validate a merge plan before execution
	 * @param MergePlan Plan to validate
	 * @param OutValidationErrors Any validation errors found
	 * @return True if plan is valid
	 */
	static bool ValidateMergePlan(
		const FMergePlan& MergePlan,
		TArray<FString>& OutValidationErrors
	);

private:
	/**
	 * Apply non-destructive resolution strategy
	 * @param Conflict Conflict to resolve
	 * @param OutOperations Generated operations
	 * @return True if resolved
	 */
	static bool ApplyNonDestructiveResolution(
		const FMergeConflict& Conflict,
		const FMergePlannerConfig& Config,
		TArray<FMergeOperation>& OutOperations
	);

	/**
	 * Apply strategy-specific resolution
	 * @param Conflict Conflict to resolve
	 * @param Strategy Strategy to apply
	 * @param OutOperations Generated operations
	 * @return True if resolved
	 */
	static bool ApplyStrategyResolution(
		const FMergeConflict& Conflict,
		EResolutionStrategy Strategy,
		const FMergePlannerConfig& Config,
		TArray<FMergeOperation>& OutOperations
	);

	/**
	 * Check if a conflict can be auto-resolved
	 * @param Conflict Conflict to check
	 * @param Config Configuration to use
	 * @return True if can be auto-resolved
	 */
	static bool CanAutoResolve(
		const FMergeConflict& Conflict,
		const FMergePlannerConfig& Config
	);

	/**
	 * Resolve position conflicts automatically
	 * @param Conflict Position conflict to resolve
	 * @param OutOperations Generated operations
	 * @return True if resolved
	 */
	static bool ResolvePositionConflict(
		const FMergeConflict& Conflict,
		TArray<FMergeOperation>& OutOperations
	);

	/**
	 * Resolve variable conflicts with smart rules
	 * @param Conflict Variable conflict to resolve
	 * @param Config Configuration to use
	 * @param OutOperations Generated operations
	 * @return True if resolved
	 */
	static bool ResolveVariableConflict(
		const FMergeConflict& Conflict,
		const FMergePlannerConfig& Config,
		TArray<FMergeOperation>& OutOperations
	);

	/**
	 * Resolve node conflicts with smart rules
	 * @param Conflict Node conflict to resolve
	 * @param Config Configuration to use
	 * @param OutOperations Generated operations
	 * @return True if resolved
	 */
	static bool ResolveNodeConflict(
		const FMergeConflict& Conflict,
		const FMergePlannerConfig& Config,
		TArray<FMergeOperation>& OutOperations
	);

	/**
	 * Generate context for LLM resolution
	 * @param Conflicts Conflicts needing resolution
	 * @param DiffResult Original diff result
	 * @return Context string for LLM
	 */
	static FString GenerateLLMContext(
		const TArray<FMergeConflict>& Conflicts,
		const FDiffResult& DiffResult
	);

	/**
	 * Build LLM prompt for conflict resolution
	 * @param Conflicts Conflicts to resolve
	 * @param Context Additional context
	 * @return Structured prompt for LLM
	 */
	static FString BuildLLMResolutionPrompt(
		const TArray<FMergeConflict>& Conflicts,
		const FString& Context
	);

	/**
	 * Parse LLM response into operations
	 * @param LLMResponse Response from LLM
	 * @param OutOperations Parsed operations
	 * @return True if parsing was successful
	 */
	static bool ParseLLMResponse(
		const FString& LLMResponse,
		TArray<FMergeOperation>& OutOperations
	);

	/**
	 * Detect and handle GUID remapping needs
	 * @param Operations Operations to analyze
	 * @param OutRemapOperations Additional remap operations
	 */
	static void DetectGuidRemappingNeeds(
		const TArray<FMergeOperation>& Operations,
		TArray<FMergeOperation>& OutRemapOperations
	);

	/**
	 * Optimize operation order for safe execution
	 * @param Operations Operations to optimize
	 * @param OutOptimizedOperations Reordered operations
	 */
	static void OptimizeOperationOrder(
		const TArray<FMergeOperation>& Operations,
		TArray<FMergeOperation>& OutOptimizedOperations
	);

	/**
	 * Generate a human-readable summary of the merge plan
	 * @param MergePlan Plan to summarize
	 * @return Summary string
	 */
	static FString GeneratePlanSummary(const FMergePlan& MergePlan);

	/**
	 * Detect function-level conflicts: when multiple node conflicts exist within the same function,
	 * replace them with a single FunctionWithInternalConflicts conflict
	 * @param InputConflicts Input conflicts to process
	 * @param DiffResult The diff result containing graph information
	 * @param OutProcessedConflicts Output conflicts with function-level conflicts detected
	 * @param OutFunctionsWithConflicts Output set of function names that have function-level conflicts
	 */
	static void DetectFunctionLevelConflicts(
		const TArray<FMergeConflict>& InputConflicts,
		const FDiffResult& DiffResult,
		TArray<FMergeConflict>& OutProcessedConflicts,
		TSet<FString>& OutFunctionsWithConflicts
	);

private:
	static TSharedPtr<ILLMAdapter> CurrentLLMAdapter;
};
