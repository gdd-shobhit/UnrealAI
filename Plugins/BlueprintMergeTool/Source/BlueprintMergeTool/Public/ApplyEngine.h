#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "Engine/Blueprint.h"
#include "DiffEngine.h"
#include "MergePlanner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "ApplyEngine.generated.h"

/**
 * Result of applying merge operations
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FApplyResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess;

	UPROPERTY()
	TArray<FString> AppliedOperations;

	UPROPERTY()
	TArray<FString> FailedOperations;

	UPROPERTY()
	TArray<FString> Warnings;

	UPROPERTY()
	FString ErrorMessage;

	UPROPERTY()
	bool bBlueprintCompiled;

	UPROPERTY()
	bool bBlueprintSaved;

	FApplyResult()
		: bSuccess(false)
		, bBlueprintCompiled(false)
		, bBlueprintSaved(false)
	{
	}
};

/**
 * Applies merge operations to Blueprint assets using Editor APIs
 * Ensures thread safety and proper compilation/saving
 */
class BLUEPRINTMERGETOOL_API FApplyEngine
{
public:
	/**
	 * Apply a merge plan to a target Blueprint
	 * @param TargetBlueprint Blueprint to modify
	 * @param MergePlan Plan containing operations to apply
	 * @param OutResult Result of the application
	 * @return True if application was successful
	 */
	static bool ApplyMergePlan(
		UBlueprint* TargetBlueprint,
		const FMergePlan& MergePlan,
		FApplyResult& OutResult
	);

	/**
	 * Apply a single merge operation
	 * @param TargetBlueprint Blueprint to modify
	 * @param Operation Operation to apply
	 * @param OutError Error message if operation fails
	 * @return True if operation was successful
	 */
	static bool ApplyOperation(
		UBlueprint* TargetBlueprint,
		const FMergeOperation& Operation,
		FString& OutError
	);

	/**
	 * Validate Blueprint integrity after applying operations
	 * @param Blueprint Blueprint to validate
	 * @param OutValidationErrors Any validation errors found
	 * @return True if Blueprint is valid
	 */
	static bool ValidateBlueprintIntegrity(
		UBlueprint* Blueprint,
		TArray<FString>& OutValidationErrors
	);

	/**
	 * Compile and save Blueprint after modifications
	 * @param Blueprint Blueprint to compile and save
	 * @param bForceCompile Whether to force compilation even if no changes detected
	 * @param OutResult Compilation and save result
	 * @return True if successful
	 */
	static bool CompileAndSaveBlueprint(
		UBlueprint* Blueprint,
		bool bForceCompile,
		FApplyResult& OutResult
	);

private:
	/**
	 * Add a variable to the Blueprint
	 * @param Blueprint Target blueprint
	 * @param VariableData JSON data for the variable
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool AddVariable(
		UBlueprint* Blueprint,
		TSharedPtr<FJsonObject> VariableData,
		FString& OutError
	);

	/**
	 * Remove a variable from the Blueprint
	 * @param Blueprint Target blueprint
	 * @param VariableGuid GUID of variable to remove
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool RemoveVariable(
		UBlueprint* Blueprint,
		const FString& VariableGuid,
		FString& OutError
	);

	/**
	 * Update a variable in the Blueprint
	 * @param Blueprint Target blueprint
	 * @param VariableGuid GUID of variable to update
	 * @param PropertyName Property to update
	 * @param NewValue New value for the property
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool UpdateVariable(
		UBlueprint* Blueprint,
		const FString& VariableGuid,
		const FString& PropertyName,
		const FString& NewValue,
		FString& OutError
	);

	/**
	 * Remap a variable GUID
	 * @param Blueprint Target blueprint
	 * @param OldGuid Old GUID to replace
	 * @param NewGuid New GUID to use
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool RemapVariableGuid(
		UBlueprint* Blueprint,
		const FString& OldGuid,
		const FString& NewGuid,
		FString& OutError
	);

	/**
	 * Add a node to a graph
	 * @param Blueprint Target blueprint
	 * @param GraphName Name of target graph
	 * @param NodeData JSON data for the node
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool AddNode(
		UBlueprint* Blueprint,
		const FString& GraphName,
		TSharedPtr<FJsonObject> NodeData,
		FString& OutError
	);

	/**
	 * Remove a node from a graph
	 * @param Blueprint Target blueprint
	 * @param GraphName Name of source graph
	 * @param NodeGuid GUID of node to remove
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool RemoveNode(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& NodeGuid,
		FString& OutError
	);

	/**
	 * Update a node property
	 * @param Blueprint Target blueprint
	 * @param GraphName Name of graph containing the node
	 * @param NodeGuid GUID of node to update
	 * @param PropertyName Property to update
	 * @param NewValue New value for the property
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool UpdateNodeProperty(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& NodeGuid,
		const FString& PropertyName,
		const FString& NewValue,
		FString& OutError
	);

	/**
	 * Move a node to a new position
	 * @param Blueprint Target blueprint
	 * @param GraphName Name of graph containing the node
	 * @param NodeGuid GUID of node to move
	 * @param NewX New X position
	 * @param NewY New Y position
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool MoveNode(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& NodeGuid,
		float NewX,
		float NewY,
		FString& OutError
	);

	/**
	 * Link two pins together
	 * @param Blueprint Target blueprint
	 * @param GraphName Name of graph containing the pins
	 * @param SourceNodeGuid Source node GUID
	 * @param SourcePinName Source pin name
	 * @param TargetNodeGuid Target node GUID
	 * @param TargetPinName Target pin name
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool LinkPins(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& SourceNodeGuid,
		const FString& SourcePinName,
		const FString& TargetNodeGuid,
		const FString& TargetPinName,
		FString& OutError
	);

	/**
	 * Unlink two pins
	 * @param Blueprint Target blueprint
	 * @param GraphName Name of graph containing the pins
	 * @param SourceNodeGuid Source node GUID
	 * @param SourcePinName Source pin name
	 * @param TargetNodeGuid Target node GUID
	 * @param TargetPinName Target pin name
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool UnlinkPins(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& SourceNodeGuid,
		const FString& SourcePinName,
		const FString& TargetNodeGuid,
		const FString& TargetPinName,
		FString& OutError
	);

	/**
	 * Add a component to the Blueprint
	 * @param Blueprint Target blueprint
	 * @param ComponentData JSON data for the component
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool AddComponent(
		UBlueprint* Blueprint,
		TSharedPtr<FJsonObject> ComponentData,
		FString& OutError
	);

	/**
	 * Remove a component from the Blueprint
	 * @param Blueprint Target blueprint
	 * @param ComponentName Name of component to remove
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool RemoveComponent(
		UBlueprint* Blueprint,
		const FString& ComponentName,
		FString& OutError
	);

	/**
	 * Update a component property
	 * @param Blueprint Target blueprint
	 * @param ComponentName Name of component to update
	 * @param PropertyName Property to update
	 * @param NewValue New value for the property
	 * @param OutError Error message if operation fails
	 * @return True if successful
	 */
	static bool UpdateComponent(
		UBlueprint* Blueprint,
		const FString& ComponentName,
		const FString& PropertyName,
		const FString& NewValue,
		FString& OutError
	);

public:
	/**
	 * Find a graph by name in the Blueprint
	 * @param Blueprint Blueprint to search
	 * @param GraphName Name of graph to find
	 * @return Graph pointer or nullptr if not found
	 */
	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);

	/**
	 * Find a node by GUID in a graph
	 * @param Graph Graph to search
	 * @param NodeGuid GUID of node to find
	 * @return Node pointer or nullptr if not found
	 */
	static UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& NodeGuid);

	/**
	 * Find a pin by ID or name in a node
	 * @param Node Node to search
	 * @param PinId Pin ID to find (preferred)
	 * @param PinName Pin name to find (fallback)
	 * @return Pin pointer or nullptr if not found
	 */
	static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinId, const FString& PinName);

	/**
	 * Find a variable by GUID in the Blueprint
	 * @param Blueprint Blueprint to search
	 * @param VariableGuid GUID of variable to find
	 * @return Pointer to variable description or nullptr if not found
	 */
	static FBPVariableDescription* FindVariableByGuid(UBlueprint* Blueprint, const FString& VariableGuid);

	/**
	 * Create a new node in a graph based on JSON data
	 * @param Graph Target graph
	 * @param NodeData JSON data describing the node
	 * @param OutError Error message if creation fails
	 * @return Created node or nullptr if failed
	 */
	static UEdGraphNode* CreateNodeFromData(
		UEdGraph* Graph,
		TSharedPtr<FJsonObject> NodeData,
		FString& OutError
	);

	/**
	 * Update all variable references when a GUID is remapped
	 * @param Blueprint Target blueprint
	 * @param OldGuid Old GUID being replaced
	 * @param NewGuid New GUID to use
	 * @param OutError Error message if update fails
	 * @return True if successful
	 */
	static bool UpdateVariableReferences(
		UBlueprint* Blueprint,
		const FString& OldGuid,
		const FString& NewGuid,
		FString& OutError
	);

	/**
	 * Ensure operation is executed on the game thread
	 * @param Operation Function to execute
	 * @return True if successful
	 */
	static bool EnsureGameThread(TFunction<bool()> Operation);

	/**
	 * Create an undo transaction for the operation
	 * @param Description Description of the transaction
	 * @return Transaction scope object
	 */
	static TSharedPtr<class FScopedTransaction> CreateUndoTransaction(const FString& Description);

	/**
	 * Log operation progress
	 * @param OperationType Type of operation being performed
	 * @param TargetName Name of the target being modified
	 * @param bSuccess Whether the operation succeeded
	 * @param ErrorMessage Error message if failed
	 */
	static void LogOperationResult(
		EMergeOperationType OperationType,
		const FString& TargetName,
		bool bSuccess,
		const FString& ErrorMessage = TEXT("")
	);

	/**
	 * Check if the Blueprint is currently being edited
	 * @param Blueprint Blueprint to check
	 * @return True if Blueprint is open in an editor
	 */
	static bool IsBlueprintBeingEdited(UBlueprint* Blueprint);

	/**
	 * Refresh Blueprint editor if it's open
	 * @param Blueprint Blueprint to refresh
	 */
	static void RefreshBlueprintEditor(UBlueprint* Blueprint);
};
