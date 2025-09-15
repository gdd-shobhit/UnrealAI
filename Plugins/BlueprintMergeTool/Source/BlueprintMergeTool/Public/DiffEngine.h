#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "Dom/JsonObject.h"
#include "DiffEngine.generated.h"

// Compile-time toggle for GUID-based matching
// 0 = compare by semantic/name keys (preferred for testing without VCS)
// 1 = compare by GUIDs (preferred when Perforce ensures stable GUIDs)
#ifndef BPT_MERGE_USE_GUID_MATCHING
#define BPT_MERGE_USE_GUID_MATCHING 0
#endif

/**
 * Operation types for Blueprint modifications
 */
UENUM(BlueprintType)
enum class EMergeOperationType : uint8
{
	// Node operations
	AddNode,
	RemoveNode,
	UpdateNodeProperty,
	MoveNode,
	
	// Variable operations
	AddVariable,
	RemoveVariable,
	UpdateVariable,
	RemapVariableGuid,
	
	// Pin/Connection operations
	LinkPins,
	UnlinkPins,
	UpdatePinProperty,
	
	// Component operations
	AddComponent,
	RemoveComponent,
	UpdateComponent,
	
	// Graph operations
	AddGraph,
	RemoveGraph,
	RenameGraph,
	
	// Timeline operations
	AddTimeline,
	RemoveTimeline,
	UpdateTimeline
};

/**
 * Conflict severity levels
 */
UENUM(BlueprintType)
enum class EConflictSeverity : uint8
{
	Low,      // Different positions, cosmetic changes
	Medium,   // Property changes, non-breaking modifications  
	High,     // Structural changes, potential data loss
	Critical  // Conflicting fundamental changes
};

/**
 * Represents a single merge operation
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FMergeOperation
{
	GENERATED_BODY()

	UPROPERTY()
	EMergeOperationType OperationType;

	UPROPERTY()
	FString TargetGraph;

	UPROPERTY()
	FString TargetId; // NodeGuid, VariableGuid, etc.

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	FString OldValue;

	UPROPERTY()
	FString NewValue;

	UPROPERTY()
	TMap<FString, FString> AdditionalData;

	FMergeOperation()
		: OperationType(EMergeOperationType::AddNode)
	{
	}
};

/**
 * Represents a conflict between different versions
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FMergeConflict
{
	GENERATED_BODY()

	UPROPERTY()
	FString ConflictId;

	UPROPERTY()
	FString ConflictType; // "Variable", "Node", "Connection", etc.

	UPROPERTY()
	FString ElementName; // Human readable name

	UPROPERTY()
	FString BaseValue;

	UPROPERTY()
	FString LocalValue;

	UPROPERTY()
	FString RemoteValue;

	UPROPERTY()
	EConflictSeverity Severity;

	UPROPERTY()
	TArray<FString> DifferingFields;

	UPROPERTY()
	FString ResolutionStrategy; // "UseLocal", "UseRemote", "Manual", etc.

	FMergeConflict()
		: Severity(EConflictSeverity::Medium)
	{
	}
};

/**
 * Result of a three-way diff operation
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FDiffResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMergeOperation> Operations;

	UPROPERTY()
	TArray<FMergeConflict> Conflicts;

	UPROPERTY()
	bool bHasConflicts;

	UPROPERTY()
	FString DiffSummary;

	FDiffResult()
		: bHasConflicts(false)
	{
	}
};

/**
 * Computes three-way structural diffs between Blueprint snapshots
 * Produces operations and identifies conflicts for resolution
 */
class BLUEPRINTMERGETOOL_API FDiffEngine
{
public:
	/**
	 * Perform a three-way diff between Base, Local, and Remote snapshots
	 * @param BaseSnapshot The common ancestor snapshot
	 * @param LocalSnapshot The local changes snapshot  
	 * @param RemoteSnapshot The remote changes snapshot
	 * @param OutResult The resulting diff with operations and conflicts
	 * @return True if diff was successful
	 */
	static bool PerformThreeWayDiff(
		TSharedPtr<FJsonObject> BaseSnapshot,
		TSharedPtr<FJsonObject> LocalSnapshot, 
		TSharedPtr<FJsonObject> RemoteSnapshot,
		FDiffResult& OutResult
	);

	/**
	 * Perform a two-way diff between snapshots
	 * @param SourceSnapshot The source snapshot
	 * @param TargetSnapshot The target snapshot
	 * @param OutResult The resulting diff with operations
	 * @return True if diff was successful
	 */
	static bool PerformTwoWayDiff(
		TSharedPtr<FJsonObject> SourceSnapshot,
		TSharedPtr<FJsonObject> TargetSnapshot,
		FDiffResult& OutResult
	);

	/**
	 * Compare two JSON values and determine if they're different
	 * @param ValueA First value to compare
	 * @param ValueB Second value to compare
	 * @param OutDifferingFields List of fields that differ
	 * @return True if values are different
	 */
	static bool CompareJsonValues(
		TSharedPtr<FJsonValue> ValueA,
		TSharedPtr<FJsonValue> ValueB,
		TArray<FString>& OutDifferingFields
	);

	/**
	 * Analyze conflict severity based on the type of change
	 * @param ConflictType The type of conflict
	 * @param DifferingFields The fields that differ
	 * @return Severity level
	 */
	static EConflictSeverity AnalyzeConflictSeverity(
		const FString& ConflictType,
		const TArray<FString>& DifferingFields
	);

private:
	/**
	 * Diff variables between snapshots
	 * @param BaseVars Base variables array
	 * @param LocalVars Local variables array  
	 * @param RemoteVars Remote variables array
	 * @param OutOperations Generated operations
	 * @param OutConflicts Generated conflicts
	 */
	static void DiffVariables(
		const TArray<TSharedPtr<FJsonValue>>& BaseVars,
		const TArray<TSharedPtr<FJsonValue>>& LocalVars,
		const TArray<TSharedPtr<FJsonValue>>& RemoteVars,
		TArray<FMergeOperation>& OutOperations,
		TArray<FMergeConflict>& OutConflicts
	);

	/**
	 * Diff graphs between snapshots
	 * @param BaseGraphs Base graphs array
	 * @param LocalGraphs Local graphs array
	 * @param RemoteGraphs Remote graphs array
	 * @param OutOperations Generated operations
	 * @param OutConflicts Generated conflicts
	 */
	static void DiffGraphs(
		const TArray<TSharedPtr<FJsonValue>>& BaseGraphs,
		const TArray<TSharedPtr<FJsonValue>>& LocalGraphs,
		const TArray<TSharedPtr<FJsonValue>>& RemoteGraphs,
		TArray<FMergeOperation>& OutOperations,
		TArray<FMergeConflict>& OutConflicts
	);

	/**
	 * Diff nodes within a graph
	 * @param GraphName Name of the graph being diffed
	 * @param BaseNodes Base nodes array
	 * @param LocalNodes Local nodes array
	 * @param RemoteNodes Remote nodes array
	 * @param OutOperations Generated operations
	 * @param OutConflicts Generated conflicts
	 */
	static void DiffNodes(
		const FString& GraphName,
		const TArray<TSharedPtr<FJsonValue>>& BaseNodes,
		const TArray<TSharedPtr<FJsonValue>>& LocalNodes,
		const TArray<TSharedPtr<FJsonValue>>& RemoteNodes,
		TArray<FMergeOperation>& OutOperations,
		TArray<FMergeConflict>& OutConflicts
	);

	/**
	 * Diff connections within a graph
	 * @param GraphName Name of the graph being diffed
	 * @param BaseConnections Base connections array
	 * @param LocalConnections Local connections array
	 * @param RemoteConnections Remote connections array
	 * @param OutOperations Generated operations
	 * @param OutConflicts Generated conflicts
	 */
	static void DiffConnections(
		const FString& GraphName,
		const TArray<TSharedPtr<FJsonValue>>& BaseConnections,
		const TArray<TSharedPtr<FJsonValue>>& LocalConnections,
		const TArray<TSharedPtr<FJsonValue>>& RemoteConnections,
		TArray<FMergeOperation>& OutOperations,
		TArray<FMergeConflict>& OutConflicts
	);

	/**
	 * Diff components between snapshots
	 * @param BaseComponents Base components array
	 * @param LocalComponents Local components array
	 * @param RemoteComponents Remote components array
	 * @param OutOperations Generated operations
	 * @param OutConflicts Generated conflicts
	 */
	static void DiffComponents(
		const TArray<TSharedPtr<FJsonValue>>& BaseComponents,
		const TArray<TSharedPtr<FJsonValue>>& LocalComponents,
		const TArray<TSharedPtr<FJsonValue>>& RemoteComponents,
		TArray<FMergeOperation>& OutOperations,
		TArray<FMergeConflict>& OutConflicts
	);

	/**
	 * Build lookup maps for efficient comparison
	 * @param JsonArray Array of JSON objects to map
	 * @param KeyField Field to use as the key
	 * @param OutMap Resulting lookup map
	 */
	static void BuildLookupMap(
		const TArray<TSharedPtr<FJsonValue>>& JsonArray,
		const FString& KeyField,
		TMap<FString, TSharedPtr<FJsonObject>>& OutMap
	);

	/**
	 * Get a stable identifier for a JSON object
	 * @param JsonObject Object to get identifier for
	 * @param PreferredKeyFields Fields to try as identifiers in order of preference
	 * @return Stable identifier string
	 */
	static FString GetObjectIdentifier(
		TSharedPtr<FJsonObject> JsonObject,
		const TArray<FString>& PreferredKeyFields
	);


public:
	/**
	 * Create a merge operation
	 * @param OpType Type of operation
	 * @param TargetGraph Target graph name
	 * @param TargetId Target object ID
	 * @param PropertyName Property being modified
	 * @param OldValue Previous value
	 * @param NewValue New value
	 * @return Constructed merge operation
	 */
	static FMergeOperation CreateOperation(
		EMergeOperationType OpType,
		const FString& TargetGraph,
		const FString& TargetId,
		const FString& PropertyName = TEXT(""),
		const FString& OldValue = TEXT(""),
		const FString& NewValue = TEXT("")
	);

	/**
	 * Create a merge conflict
	 * @param ConflictType Type of conflict
	 * @param ElementName Human readable element name
	 * @param BaseValue Base version value
	 * @param LocalValue Local version value
	 * @param RemoteValue Remote version value
	 * @param DifferingFields Fields that are different
	 * @return Constructed merge conflict
	 */
	static FMergeConflict CreateConflict(
		const FString& ConflictType,
		const FString& ElementName,
		const FString& BaseValue,
		const FString& LocalValue,
		const FString& RemoteValue,
		const TArray<FString>& DifferingFields
	);

	/**
	 * Generate a summary of the diff result
	 * @param DiffResult Result to summarize
	 * @return Human readable summary
	 */
	static FString GenerateDiffSummary(const FDiffResult& DiffResult);

	/**
	 * Compare JSON objects field by field
	 * @param ObjectA First object
	 * @param ObjectB Second object
	 * @param OutDifferingFields Fields that differ
	 * @return True if objects are different
	 */
	static bool CompareJsonObjects(
		TSharedPtr<FJsonObject> ObjectA,
		TSharedPtr<FJsonObject> ObjectB,
		TArray<FString>& OutDifferingFields
	);

	/**
	 * Compare JSON arrays element by element
	 * @param ArrayA First array
	 * @param ArrayB Second array
	 * @param OutDifferingFields Fields that differ
	 * @return True if arrays are different
	 */
	static bool CompareJsonArrays(
		const TArray<TSharedPtr<FJsonValue>>& ArrayA,
		const TArray<TSharedPtr<FJsonValue>>& ArrayB,
		TArray<FString>& OutDifferingFields
	);

	/**
	 * Detect if a change represents a move operation
	 * @param BaseObj Base object
	 * @param ModifiedObj Modified object
	 * @return True if this is a move operation
	 */
	static bool IsMoveOperation(
		TSharedPtr<FJsonObject> BaseObj,
		TSharedPtr<FJsonObject> ModifiedObj
	);

	/**
	 * Check if two variables have the same GUID (for Perforce/Git integration)
	 * @param LocalVar Local variable object
	 * @param RemoteVar Remote variable object
	 * @return True if GUIDs match (should be true with proper version control)
	 */
	static bool AreVariableGuidsIdentical(
		TSharedPtr<FJsonObject> LocalVar,
		TSharedPtr<FJsonObject> RemoteVar
	);

	/**
	 * Compare two variables ignoring fields that should be different (like GUID)
	 * @param LocalVar Local variable object
	 * @param RemoteVar Remote variable object
	 * @param OutDifferingFields Fields that differ (excluding ignored fields)
	 * @return True if variables differ in meaningful ways
	 */
	static bool CompareVariablesIgnoringGuid(
		TSharedPtr<FJsonObject> LocalVar,
		TSharedPtr<FJsonObject> RemoteVar,
		TArray<FString>& OutDifferingFields
	);

	/**
	 * Check if two connection objects represent the same connection
	 * @param ConnA First connection
	 * @param ConnB Second connection
	 * @return True if they represent the same connection
	 */
	static bool AreConnectionsEqual(
		TSharedPtr<FJsonObject> ConnA,
		TSharedPtr<FJsonObject> ConnB
	);
};
