#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "Engine/Blueprint.h"
#include "Dom/JsonObject.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

/**
 * Produces deterministic JSON snapshots of UBlueprint objects
 * Key features:
 * - Deterministic ordering by GUID/stable keys
 * - Complete variable, node, and graph structure capture
 * - Canonical representation for reliable diffing
 */
class BLUEPRINTMERGETOOL_API FSnapshotManager
{
public:
	/**
	 * Create a deterministic JSON snapshot of a Blueprint
	 * @param Blueprint The blueprint to snapshot
	 * @param OutJson The resulting JSON object
	 * @return True if successful
	 */
	static bool CreateSnapshot(UBlueprint* Blueprint, TSharedPtr<FJsonObject>& OutJson);

	/**
	 * Create a JSON snapshot from a Blueprint asset path
	 * @param BlueprintPath Path to the blueprint asset
	 * @param OutJson The resulting JSON object
	 * @return True if successful
	 */
	static bool CreateSnapshotFromPath(const FString& BlueprintPath, TSharedPtr<FJsonObject>& OutJson);

	/**
	 * Serialize snapshot to JSON string
	 * @param JsonObject The JSON object to serialize
	 * @param OutJsonString The resulting JSON string
	 * @return True if successful
	 */
	static bool SerializeToString(TSharedPtr<FJsonObject> JsonObject, FString& OutJsonString);

	/**
	 * Deserialize JSON string to snapshot object
	 * @param JsonString The JSON string to parse
	 * @param OutJsonObject The resulting JSON object
	 * @return True if successful
	 */
	static bool DeserializeFromString(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject);

public:
	/**
	 * Capture all variables from the Blueprint
	 * @param Blueprint Source blueprint
	 * @param OutVariablesArray JSON array to populate
	 */
	static void CaptureVariables(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutVariablesArray);

	/**
	 * Capture all graphs from the Blueprint
	 * @param Blueprint Source blueprint
	 * @param OutGraphsArray JSON array to populate
	 */
	static void CaptureGraphs(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutGraphsArray);

	/**
	 * Capture a single graph
	 * @param Graph The graph to capture
	 * @param OutGraphObject JSON object to populate
	 */
	static void CaptureGraph(UEdGraph* Graph, TSharedPtr<FJsonObject>& OutGraphObject);

	/**
	 * Capture all nodes from a graph
	 * @param Graph Source graph
	 * @param OutNodesArray JSON array to populate
	 */
	static void CaptureNodes(UEdGraph* Graph, TArray<TSharedPtr<FJsonValue>>& OutNodesArray);

	/**
	 * Capture a single node
	 * @param Node The node to capture
	 * @param OutNodeObject JSON object to populate
	 */
	static void CaptureNode(UEdGraphNode* Node, TSharedPtr<FJsonObject>& OutNodeObject);

	/**
	 * Capture pins from a node
	 * @param Node Source node
	 * @param OutPinsArray JSON array to populate
	 */
	static void CapturePins(UEdGraphNode* Node, TArray<TSharedPtr<FJsonValue>>& OutPinsArray);

	/**
	 * Capture pin connections
	 * @param Graph Source graph
	 * @param OutConnectionsArray JSON array to populate
	 */
	static void CaptureConnections(UEdGraph* Graph, TArray<TSharedPtr<FJsonValue>>& OutConnectionsArray);

	/**
	 * Capture components from the Blueprint
	 * @param Blueprint Source blueprint
	 * @param OutComponentsArray JSON array to populate
	 */
	static void CaptureComponents(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutComponentsArray);

	/**
	 * Capture timeline components
	 * @param Blueprint Source blueprint
	 * @param OutTimelinesArray JSON array to populate
	 */
	static void CaptureTimelines(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutTimelinesArray);

	/**
	 * Get a stable key for an object (GUID or fallback)
	 * @param Object The object to get a key for
	 * @return Stable key string
	 */
	static FString GetStableKey(UObject* Object);

	/**
	 * Get a stable key for a node (NodeGuid or fallback)
	 * @param Node The node to get a key for
	 * @return Stable key string
	 */
	static FString GetNodeStableKey(UEdGraphNode* Node);

	/**
	 * Normalize and sort JSON arrays for deterministic output
	 * @param JsonObject The object containing arrays to sort
	 */
	static void NormalizeJsonObject(TSharedPtr<FJsonObject>& JsonObject);

	/**
	 * Sort JSON array by a specific field
	 * @param JsonArray Array to sort
	 * @param SortField Field to sort by
	 */
	static void SortJsonArrayByField(TArray<TSharedPtr<FJsonValue>>& JsonArray, const FString& SortField);

	/**
	 * Remove transient/ephemeral fields from JSON
	 * @param JsonObject Object to clean
	 */
	static void RemoveTransientFields(TSharedPtr<FJsonObject>& JsonObject);

	/**
	 * Generate a hash for the snapshot for quick comparison
	 * @param JsonObject The snapshot object
	 * @return Hash string
	 */
	static FString GenerateSnapshotHash(TSharedPtr<FJsonObject> JsonObject);
};
