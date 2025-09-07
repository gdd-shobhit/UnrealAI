#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"

// Forward declarations
class UBlueprint;

/**
 * Simple data structure for storing blueprint information
 */
struct FBlueprintInfo
{
	FString blueprintName;
	FString parentClass;
	TArray<FString> variables;
	TArray<FString> functions;
	TArray<FString> events;
};

/**
 * Lightweight data model describing nodes, pins and connections.
 * These are plain C++ structs to be easy to construct/serialize.
 */

struct FNodePinRef
{
	FGuid nodeId;
	FName pinName = NAME_None; // NAME_None => auto-resolve by type/direction
	EEdGraphPinDirection pinDirection = EGPD_MAX; // optional hint
};

struct FNodeConnection
{
	FNodePinRef from;
	FNodePinRef to;
};

struct FNodeDescriptor
{
	FGuid nodeId;
	FString nodeClassPath; // full path to UClass or class path string, e.g. "/Script/BlueprintGraph.K2Node_CallFunction"
	FVector2D nodePosition = FVector2D::ZeroVector;
	TMap<FName, FEdGraphPinType> pinHints; // optional hints: pinName->type
	TMap<FString, FString> metadata; // optional custom data (function refs, member names etc)
};

struct FFunctionGraphDescriptor
{
	FName functionName;
	TArray<FNodeDescriptor> nodes;
	TArray<FNodeConnection> connections;
	TArray<TPair<FName,FEdGraphPinType>> variables; // function-local variables / params represented as pin types

	// helper to build index maps at runtime (not serialized)
	TMap<FGuid, TArray<FNodeConnection>> connectionsByNode;
	void buildConnectionIndex() const
	{
		// intentionally left empty here; built in parser when needed
	}
};
class UNREALAI_API FBlueprintJsonParser
{
public:
	FBlueprintJsonParser();
	~FBlueprintJsonParser();

	bool AddBlueprintFunctionFromDescriptor(UBlueprint* blueprint, const FFunctionGraphDescriptor& graphDesc);

	/**
	 * Converts a blueprint to JSON and logs it
	 */
	void ConvertBlueprintToJsonAndLog(UBlueprint* Blueprint);

	/**
	 * Extracts basic information from a blueprint
	 */
	FBlueprintInfo ExtractBlueprintInfo(UBlueprint* Blueprint);

	/**
	 * Adds a function to blueprint
	 */
	void AddFunctionToBlueprint(UBlueprint* blueprint,
		FName functionName,
		const TArray<TSubclassOf<UEdGraphNode>>& nodeClasses,
		const TArray<TPair<int32, int32>>& connections,
		const TArray<TPair<FName, FEdGraphPinType>>& variables);

	void ConnectNodes(const TArray<UEdGraphNode*>& nodes, const TArray<TPair<int32, int32>>& connections);

	/**
	 * Converts blueprint info to JSON object
	 */
	TSharedPtr<FJsonObject> BlueprintInfoToJson(const FBlueprintInfo& BlueprintInfo);

	/**
	 * Logs JSON object to UE_LOG
	 */
	void LogJsonObject(const TSharedPtr<FJsonObject>& JsonObject);

private:
	// Internal steps
	static bool AddVariablesToBlueprint(UBlueprint* blueprint, const TArray<TPair<FName, FEdGraphPinType>>& variables);
	static bool CreateNodesFromDescriptors(UEdGraph* functionGraph, const TArray<FNodeDescriptor>& nodeDescriptors, TMap<FGuid, UEdGraphNode*>& outGuidToNodeMap);
	static bool ConnectNodesFromDescriptor(UEdGraph* functionGraph, const TArray<FNodeConnection>& connections, TMap<FGuid, UEdGraphNode*>& guidToNodeMap);
public:
	// Helpers
	static UEdGraphPin* ResolvePinByNameOrType(UEdGraphNode* node, const FNodePinRef& pinRef, const FEdGraphPinType* preferredType = nullptr);
	static bool ArePinTypesCompatible(const FEdGraphPinType& a, const FEdGraphPinType& b);
	static bool InsertConversionNodeIfNeeded(UEdGraph* functionGraph, UEdGraphPin* outPin, UEdGraphPin* inPin);
	static void SetNodeGuid(UEdGraphNode* node, const FGuid& guid);
	static bool ParseFunctionGraphDescriptorFromJson(const FString& jsonString, FFunctionGraphDescriptor& outDesc);
	static bool AddBlueprintFunctionFromJsonFile(UBlueprint* blueprint, const FString& jsonFilePath);
};
