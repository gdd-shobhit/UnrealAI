#include "BlueprintJsonParser.h"
#include "Engine/Blueprint.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"


FBlueprintJsonParser::FBlueprintJsonParser()
{
	// Constructor
}

FBlueprintJsonParser::~FBlueprintJsonParser()
{
	// Destructor
}

bool FBlueprintJsonParser::AddBlueprintFunctionFromDescriptor(UBlueprint* blueprint, const FFunctionGraphDescriptor& graphDesc)
{
	if (!blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("addBlueprintFunctionFromDescriptor: blueprint is null"));
		return false;
	}

	if (graphDesc.functionName.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("addBlueprintFunctionFromDescriptor: functionName is empty"));
		return false;
	}

	// 1) Create the graph
	UEdGraph* functionGraph = FBlueprintEditorUtils::CreateNewGraph(
		blueprint,
		graphDesc.functionName,
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!functionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create function graph for %s"), *graphDesc.functionName.ToString());
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph(blueprint, functionGraph, true, (UClass*)nullptr);

	// 2) Add variables to the blueprint
	if (!graphDesc.variables.IsEmpty())
	{
		if (!AddVariablesToBlueprint(blueprint, graphDesc.variables))
		{
			UE_LOG(LogTemp, Warning, TEXT("AddVariablesToBlueprint returned false (non-fatal)"));
		}
	}

	// 3) Ensure function entry / result nodes exist in descriptors OR create defaults
	// If the user did not include entry/result nodes in descriptor, we'll create them and ensure they are present
	TArray<FNodeDescriptor> nodeDescriptors = graphDesc.nodes; // copy so we can modify
	bool hasEntry = false, hasResult = false;
	for (const FNodeDescriptor& nd : nodeDescriptors)
	{
		// check known nodeClassPath strings for entry/result (best-effort)
		if (nd.nodeClassPath.Contains(TEXT("K2Node_FunctionEntry")) || nd.metadata.Contains("FunctionEntry"))
		{
			hasEntry = true;
		}
		if (nd.nodeClassPath.Contains(TEXT("K2Node_FunctionResult")) || nd.metadata.Contains("FunctionResult"))
		{
			hasResult = true;
		}
	}

	if (!hasEntry)
	{
		FNodeDescriptor entryDesc;
		entryDesc.nodeId = FGuid::NewGuid();
		entryDesc.nodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_FunctionEntry");
		entryDesc.nodePosition = FVector2D(-200, 0);
		entryDesc.metadata.Add(TEXT("autoCreated"), TEXT("true"));
		nodeDescriptors.Insert(entryDesc, 0);
	}

	if (!hasResult)
	{
		FNodeDescriptor resDesc;
		resDesc.nodeId = FGuid::NewGuid();
		resDesc.nodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_FunctionResult");
		resDesc.nodePosition = FVector2D(600, 0);
		resDesc.metadata.Add(TEXT("autoCreated"), TEXT("true"));
		nodeDescriptors.Add(resDesc);
	}

	// 4) Create nodes and map GUID => node
	TMap<FGuid, UEdGraphNode*> guidToNodeMap;
	if (!CreateNodesFromDescriptors(functionGraph, nodeDescriptors, guidToNodeMap))
	{
		UE_LOG(LogTemp, Error, TEXT("createNodesFromDescriptors failed"));
		return false;
	}

	// 5) Connect nodes using descriptor connections
	if (!ConnectNodesFromDescriptor(functionGraph, graphDesc.connections, guidToNodeMap))
	{
		UE_LOG(LogTemp, Warning, TEXT("connectNodesFromDescriptor returned false (some links may have failed)"));
		// proceed: partial connect is still useful
	}

	// 6) Finalize and mark blueprint dirty
	functionGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(blueprint);

	UE_LOG(LogTemp, Log, TEXT("addBlueprintFunctionFromDescriptor: created function %s with %d nodes and %d connections"),
		*graphDesc.functionName.ToString(), guidToNodeMap.Num(), graphDesc.connections.Num());

	return true;
}

/** Add member variables (simple wrapper) */
bool FBlueprintJsonParser::AddVariablesToBlueprint(UBlueprint* blueprint, const TArray<TPair<FName, FEdGraphPinType>>& variables)
{
	if (!blueprint) return false;

	for (int32 i = 0; i < variables.Num(); ++i)
	{
		const FEdGraphPinType& varType = variables[i].Value;
		// Create a stable but readable name — you can change this policy
		FName varName = *FString::Printf(variables[i].Key);
		if (!FBlueprintEditorUtils::AddMemberVariable(blueprint, varName, varType))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to add member variable %s"), *varName.ToString());
		}
	}

	return true;
}

/** Create nodes from descriptors and add them to the graph. Returns a guid->node map. */
bool FBlueprintJsonParser::CreateNodesFromDescriptors(UEdGraph* functionGraph, const TArray<FNodeDescriptor>& nodeDescriptors, TMap<FGuid, UEdGraphNode*>& outGuidToNodeMap)
{
	if (!functionGraph) return false;

	for (const FNodeDescriptor& desc : nodeDescriptors)
	{
		// Try to load class
		UClass* nodeClass = nullptr;
		if (!desc.nodeClassPath.IsEmpty())
		{
			// Try to load by path name (path may be "/Script/BlueprintGraph.K2Node_CallFunction")
			nodeClass = LoadObject<UClass>(nullptr, *desc.nodeClassPath);
			if (!nodeClass)
			{
				// fallback to find UClass by name (strip package prefix)
				FString className = FPackageName::ObjectPathToObjectName(desc.nodeClassPath);
				nodeClass = FindObject<UClass>(ANY_PACKAGE, *className);
			}
		}

		// If we couldn't resolve a class, default to a generic UEdGraphNode (less ideal)
		if (!nodeClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("createNodesFromDescriptors: Could not resolve class for %s; using UEdGraphNode fallback"), *desc.nodeClassPath);
			nodeClass = UEdGraphNode::StaticClass();
		}

		UEdGraphNode* newNode = NewObject<UEdGraphNode>(functionGraph, nodeClass);
		if (!newNode)
		{
			UE_LOG(LogTemp, Error, TEXT("createNodesFromDescriptors: NewObject failed for class %s"), *nodeClass->GetName());
			continue;
		}

		// Preserve GUID if provided
		if (desc.nodeId.IsValid())
		{
			SetNodeGuid(newNode, desc.nodeId);
		}
		else
		{
			newNode->CreateNewGuid();
		}

		// Position & metadata
		newNode->NodePosX = (int32)desc.nodePosition.X;
		newNode->NodePosY = (int32)desc.nodePosition.Y;

		// Post place and create pins
		newNode->PostPlacedNewNode();
		newNode->AllocateDefaultPins();

		// Some nodes require additional setup, e.g. function entry/result
		if (UK2Node_FunctionEntry* entry = Cast<UK2Node_FunctionEntry>(newNode))
		{
			// If metadata contains function name, set it
			if (!entry->FunctionReference.GetMemberName().IsValid())
			{
				// Leave function ref blank; will be set by caller if needed
			}
		}
		else if (UK2Node_FunctionResult* result = Cast<UK2Node_FunctionResult>(newNode))
		{
			// same: function reference handled by caller
		}

		functionGraph->AddNode(newNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);

		outGuidToNodeMap.Add(newNode->NodeGuid, newNode);
	}

	return true;
}

/** Connect pins according to the connection descriptors */
bool FBlueprintJsonParser::ConnectNodesFromDescriptor(UEdGraph* functionGraph, const TArray<FNodeConnection>& connections, TMap<FGuid, UEdGraphNode*>& guidToNodeMap)
{
	if (!functionGraph) return false;

	for (const FNodeConnection& conn : connections)
	{
		const UEdGraphNode** fromNodePtr = guidToNodeMap.Find(conn.from.nodeId);
		const UEdGraphNode** toNodePtr = guidToNodeMap.Find(conn.to.nodeId);

		if (!fromNodePtr || !toNodePtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("ConnectNodesFromDescriptor: missing node for connection (from %s -> to %s)"),
				*conn.from.nodeId.ToString(), *conn.to.nodeId.ToString());
			continue;
		}

		UEdGraphNode* fromNode = const_cast<UEdGraphNode*>(*fromNodePtr);
		UEdGraphNode* toNode = const_cast<UEdGraphNode*>(*toNodePtr);

		// Resolve pins
		UEdGraphPin* outPin = ResolvePinByNameOrType(fromNode, conn.from, nullptr);
		UEdGraphPin* inPin = ResolvePinByNameOrType(toNode, conn.to, nullptr);

		if (!outPin || !inPin)
		{
			UE_LOG(LogTemp, Warning, TEXT("ConnectNodesFromDescriptor: failed to resolve pins for %s -> %s"), *conn.from.nodeId.ToString(), *conn.to.nodeId.ToString());
			continue;
		}

		// If pin directions are inverted, swap them
		if (outPin->Direction == EGPD_Input && inPin->Direction == EGPD_Output)
		{
			UEdGraphPin* temp = outPin;
			outPin = inPin;
			inPin = temp;
		}

		// Now validate directions: outPin should be output, inPin should be input
		if (outPin->Direction != EGPD_Output || inPin->Direction != EGPD_Input)
		{
			UE_LOG(LogTemp, Warning, TEXT("ConnectNodesFromDescriptor: pin direction mismatch (outDir=%d inDir=%d)"), (int)outPin->Direction, (int)inPin->Direction);
			// attempt to continue: try to flip if possible
		}

		// Type compatibility
		if (!ArePinTypesCompatible(outPin->PinType, inPin->PinType))
		{
			UE_LOG(LogTemp, Warning, TEXT("ConnectNodesFromDescriptor: pin types incompatible (%s -> %s). Trying conversion..."),
				*outPin->PinType.PinCategory.ToString(), *inPin->PinType.PinCategory.ToString());

			// Attempt to insert conversion node; if it returns false, skip connection
			if (!InsertConversionNodeIfNeeded(functionGraph, outPin, inPin))
			{
				UE_LOG(LogTemp, Warning, TEXT("ConnectNodesFromDescriptor: no conversion available; skipping link"));
				continue;
			}
			// If conversion node inserted, it should have made the link to inPin
		}
		else
		{
			// Make link directly
			outPin->MakeLinkTo(inPin);
		}
	}

	return true;
}

/**
 * Resolve a pin on a node by either explicit pinName or by matching a preferred type/direction.
 * Returns the matched pin or nullptr.
 */
UEdGraphPin* FBlueprintJsonParser::ResolvePinByNameOrType(UEdGraphNode* node, const FNodePinRef& pinRef, const FEdGraphPinType* preferredType)
{
	if (!node) return nullptr;

	// 1) If explicit pinName provided, prefer that
	if (pinRef.pinName != NAME_None)
	{
		UEdGraphPin* found = node->FindPin(pinRef.pinName);
		if (found) return found;
		// Try case-insensitive or prefixed variants (best-effort)
		for (UEdGraphPin* pin : node->Pins)
		{
			if (pin && pin->PinName.IsEqual(pinRef.pinName, ENameCase::IgnoreCase))
			{
				return pin;
			}
		}
	}

	// 2) Try preferred direction & preferred type match
	for (UEdGraphPin* pin : node->Pins)
	{
		if (!pin) continue;

		// If user specified direction, prefer matching pins
		if (pinRef.pinDirection != EGPD_MAX && pin->Direction != pinRef.pinDirection)
		{
			continue;
		}

		// Avoid exec pins unless caller explicitly requested them (prefer data pins for data connections)
		if (preferredType)
		{
			if (ArePinTypesCompatible(pin->PinType, *preferredType))
			{
				return pin;
			}
		}
	}

	// 3) If no preferred type, try to pick the first pin that matches requested direction
	for (UEdGraphPin* pin : node->Pins)
	{
		if (!pin) continue;
		if (pinRef.pinDirection != EGPD_MAX)
		{
			if (pin->Direction == pinRef.pinDirection)
				return pin;
		}
	}

	// 4) Last resort: return first non-exec pin (prefer output)
	for (UEdGraphPin* pin : node->Pins)
	{
		if (!pin) continue;
		if (pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			return pin;
	}

	// 5) fallback: return first pin at all
	return node->Pins.Num() ? node->Pins[0] : nullptr;
}

/** Very lightweight pin type compatibility check. Expand as needed. */
bool FBlueprintJsonParser::ArePinTypesCompatible(const FEdGraphPinType& a, const FEdGraphPinType& b)
{
	// If exact category match, accept
	if (a.PinCategory == b.PinCategory)
	{
		// For objects/structs, compare subcategory object if present
		if (a.PinCategory == UEdGraphSchema_K2::PC_Object || a.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			// If either has no subcategory object, accept (loose)
			if (a.PinSubCategoryObject == nullptr || b.PinSubCategoryObject == nullptr)
			{
				return true;
			}
			// Otherwise compare exact class/type
			return (a.PinSubCategoryObject == b.PinSubCategoryObject);
		}
		return true;
	}

	// Allow certain convertible categories: e.g., int -> float, numeric promotions
	{
		// Int -> Float or Float -> Int should be considered convertible
		if ((a.PinCategory == UEdGraphSchema_K2::PC_Int && b.PinCategory == UEdGraphSchema_K2::PC_Float) ||
			(a.PinCategory == UEdGraphSchema_K2::PC_Float && b.PinCategory == UEdGraphSchema_K2::PC_Int))
		{
			return true;
		}

		// Any -> String could be convertible (but requires conversion node)
		if (b.PinCategory == UEdGraphSchema_K2::PC_String)
		{
			return true; // mark convertible; may require conversion node
		}
	}

	// Default: not compatible
	return false;
}

/**
 * Insert a conversion node if possible. Minimal implementation:
 * - Logs and returns false if no conversion implemented.
 * - If you implement conversion nodes (ToString, Cast, etc), create them here and link between outPin and inPin.
 *
 * IMPORTANT: this is engine/graph-specific; implement conversion patterns you need (numeric casts, ToString, object casts).
 */
bool FBlueprintJsonParser::InsertConversionNodeIfNeeded(UEdGraph* functionGraph, UEdGraphPin* outPin, UEdGraphPin* inPin)
{
	if (!functionGraph || !outPin || !inPin) return false;

	// Simple case: numeric int->float or float->int: create an explicit cast/conversion call node if you have it
	// For now: do nothing and return false so caller will skip connection
	// TODO: Insert appropriate conversion function nodes (e.g., call UKismetMathLibrary functions or ToString).
	UE_LOG(LogTemp, Verbose, TEXT("insertConversionNodeIfNeeded: conversion requested from %s to %s but no automatic conversion implemented."),
		*outPin->PinType.PinCategory.ToString(), *inPin->PinType.PinCategory.ToString());

	return false;
}

/** Set/override node GUID safely (UE versions differ slightly). */
void FBlueprintJsonParser::SetNodeGuid(UEdGraphNode* node, const FGuid& guid)
{
	if (!node) return;

	// Many UEdGraphNode implementations expose NodeGuid directly
	// Assign it. If engine uses different property, adapt accordingly.
	node->NodeGuid = guid;
}


void FBlueprintJsonParser::ConvertBlueprintToJsonAndLog(UBlueprint* Blueprint)
{
	UE_LOG(LogTemp, Log, TEXT("=== ConvertBlueprintToJsonAndLog START ==="));
	
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot convert null blueprint to JSON"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Converting blueprint to JSON: %s"), *Blueprint->GetName());
	UE_LOG(LogTemp, Log, TEXT("Blueprint pointer valid: %s"), Blueprint ? TEXT("YES") : TEXT("NO"));

	// Extract blueprint info
	UE_LOG(LogTemp, Log, TEXT("Extracting blueprint info..."));
	FBlueprintInfo BlueprintInfo = ExtractBlueprintInfo(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("Extracted info - Name: %s, Parent: %s, Variables: %d, Functions: %d"), 
		*BlueprintInfo.blueprintName, 
		*BlueprintInfo.parentClass, 
		BlueprintInfo.variables.Num(), 
		BlueprintInfo.functions.Num());

	// Convert to JSON
	UE_LOG(LogTemp, Log, TEXT("Converting to JSON..."));
	TSharedPtr<FJsonObject> JsonObject = BlueprintInfoToJson(BlueprintInfo);
	UE_LOG(LogTemp, Log, TEXT("JSON object created: %s"), JsonObject.IsValid() ? TEXT("YES") : TEXT("NO"));

	// Log the JSON
	UE_LOG(LogTemp, Log, TEXT("Logging JSON object..."));
	LogJsonObject(JsonObject);
	
	UE_LOG(LogTemp, Log, TEXT("=== ConvertBlueprintToJsonAndLog END ==="));
}

FBlueprintInfo FBlueprintJsonParser::ExtractBlueprintInfo(UBlueprint* blueprint)
{
	FBlueprintInfo Info;

	if (!blueprint)
	{
		return Info;
	}

	// Basic info
	Info.blueprintName = blueprint->GetName();
	Info.parentClass = blueprint->ParentClass->GetName();

	// Extract variables
	for (const FBPVariableDescription& Variable : blueprint->NewVariables)
	{
		FString VariableInfo = FString::Printf(TEXT("%s (%s)"), 
			*Variable.VarName.ToString(), 
			*Variable.VarType.PinCategory.ToString());
		Info.variables.Add(VariableInfo);
	}

	// Extract functions (simplified)
	if (blueprint)
	{
		for (UEdGraph* Graph : blueprint->FunctionGraphs)
		{
			if (!Graph) continue;

			Info.functions.Add(Graph->GetFName().ToString());
		}

		//AddFunctionToBlueprint(blueprint, "TestFunction");
		FBlueprintJsonParser::AddBlueprintFunctionFromDescriptor(blueprint, myGraphDescriptor);
		// 6. Mark Blueprint dirty + recompile
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprint);
		FKismetEditorUtilities::CompileBlueprint(blueprint);

		UE_LOG(LogTemp, Log, TEXT("Extracted info: %s variables, %s functions"),
			*FString::FromInt(Info.variables.Num()),
			*FString::FromInt(Info.functions.Num()));
	}

	return Info;
}

void FBlueprintJsonParser::AddFunctionToBlueprint(UBlueprint* blueprint,
	FName functionName,
	const TArray<TSubclassOf<UEdGraphNode>>& nodeClasses,
	const TArray<TPair<int32, int32>>& connections,
	const TArray<TPair<FName, FEdGraphPinType>>& variables)
{
	UEdGraph* functionToAdd = FBlueprintEditorUtils::CreateNewGraph(
		blueprint,
		functionName,
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	// Register it as a function graph in the Blueprint
	FBlueprintEditorUtils::AddFunctionGraph(blueprint, functionToAdd, true, (UClass*)nullptr);

	// 2. Add variables
	for (const TPair<FName, FEdGraphPinType>& var : variables)
	{
		FName varName = *FString::Printf(var.Key);
		FBlueprintEditorUtils::AddMemberVariable(blueprint, varName, var.Value);
	}

	// Entry node processing
	UK2Node_FunctionEntry* entryNode = nullptr;
	for (UEdGraphNode* Node : functionToAdd->Nodes)
	{
		entryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (entryNode)
			break;
	}

	if (entryNode == nullptr)
	{
		entryNode = NewObject<UK2Node_FunctionEntry>(functionToAdd);
		entryNode->CreateNewGuid();
		entryNode->PostPlacedNewNode();
		entryNode->FunctionReference.SetSelfMember(functionName);

		// Note: Entry node already comes with ExecPins
		// entryNode->AllocateDefaultPins();

		functionToAdd->AddNode(entryNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	}
	
	// Add Function Result (return) node
	UK2Node_FunctionResult* ResultNode = NewObject<UK2Node_FunctionResult>(functionToAdd);
	ResultNode->CreateNewGuid();
	ResultNode->PostPlacedNewNode();
	ResultNode->FunctionReference.SetSelfMember(functionName);

	// Note: Result node already comes with ExecPins
	// ResultNode->AllocateDefaultPins();
	functionToAdd->AddNode(ResultNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);

	// 5. Spawn requested nodes
	TArray<UEdGraphNode*> createdNodes;
	for (TSubclassOf<UEdGraphNode> nodeClass : nodeClasses)
	{
		if (!*nodeClass) continue;

		UEdGraphNode* newNode = NewObject<UEdGraphNode>(functionToAdd, nodeClass);
		newNode->CreateNewGuid();
		newNode->PostPlacedNewNode();
		newNode->AllocateDefaultPins();
		functionToAdd->AddNode(newNode, false, false);

		createdNodes.Add(newNode);
	}

	// Add entry & result for indexing
	createdNodes.Insert(entryNode, 0);
	createdNodes.Add(ResultNode);

	// 5. Connect execution pins (Entry → Result)
	ConnectNodes(createdNodes, connections);

	// ResultNode->AutowireNewNode(entryNode->GetThenPin());
	// Note: More systematic connection
	/*if (ThenPin && ReturnExecPin)
	{
		ThenPin->MakeLinkTo(ReturnExecPin);
	}*/
}

void FBlueprintJsonParser::ConnectNodes(
	const TArray<UEdGraphNode*>& nodes,
	const TArray<TPair<int32, int32>>& connections
)
{
	for (const TPair<int32, int32>& conn : connections)
	{
		if (!nodes.IsValidIndex(conn.Key) || !nodes.IsValidIndex(conn.Value))
			continue;

		UEdGraphNode* fromNode = nodes[conn.Key];
		UEdGraphNode* toNode = nodes[conn.Value];

		if (!fromNode || !toNode) continue;

		// Look for exec pins
		UEdGraphPin* outPin = fromNode->FindPin(TEXT("Then"));
		UEdGraphPin* inPin = toNode->FindPin(TEXT("Execute"));

		if (outPin && inPin)
		{
			outPin->MakeLinkTo(inPin);
		}
	}
}

TSharedPtr<FJsonObject> FBlueprintJsonParser::BlueprintInfoToJson(const FBlueprintInfo& BlueprintInfo)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	// Basic info
	JsonObject->SetStringField(TEXT("blueprint_name"), BlueprintInfo.blueprintName);
	JsonObject->SetStringField(TEXT("parent_class"), BlueprintInfo.parentClass);

	// Variables array
	TArray<TSharedPtr<FJsonValue>> VariablesArray;
	for (const FString& Variable : BlueprintInfo.variables)
	{
		VariablesArray.Add(MakeShareable(new FJsonValueString(Variable)));
	}
	JsonObject->SetArrayField(TEXT("variables"), VariablesArray);

	// Functions array
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (const FString& Function : BlueprintInfo.functions)
	{
		FunctionsArray.Add(MakeShareable(new FJsonValueString(Function)));
	}
	JsonObject->SetArrayField(TEXT("functions"), FunctionsArray);

	return JsonObject;
}

void FBlueprintJsonParser::LogJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot log invalid JSON object"));
		return;
	}

	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	// Log the JSON string
	UE_LOG(LogTemp, Log, TEXT("Blueprint JSON:"));
	UE_LOG(LogTemp, Log, TEXT("%s"), *OutputString);
}
