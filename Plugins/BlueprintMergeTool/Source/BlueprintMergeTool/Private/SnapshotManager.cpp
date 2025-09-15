#include "../Public/SnapshotManager.h"
#include "../Public/BlueprintDataStructures.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Timeline.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformFilemanager.h"

bool FSnapshotManager::CreateSnapshot(UBlueprint* Blueprint, TSharedPtr<FJsonObject>& OutJson)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SnapshotManager: Blueprint is null"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("SnapshotManager: Creating snapshot for Blueprint: %s"), *Blueprint->GetName());

	OutJson = MakeShareable(new FJsonObject);

	// Basic Blueprint information
	OutJson->SetStringField(TEXT("BlueprintName"), Blueprint->GetName());
	OutJson->SetStringField(TEXT("BlueprintPath"), Blueprint->GetPathName());
	OutJson->SetStringField(TEXT("ParentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT(""));
	OutJson->SetStringField(TEXT("BlueprintType"), Blueprint->BlueprintType == BPTYPE_Normal ? TEXT("Normal") : 
		Blueprint->BlueprintType == BPTYPE_MacroLibrary ? TEXT("MacroLibrary") :
		Blueprint->BlueprintType == BPTYPE_Interface ? TEXT("Interface") :
		Blueprint->BlueprintType == BPTYPE_FunctionLibrary ? TEXT("FunctionLibrary") : TEXT("Unknown"));
	
	// Capture Blueprint GUID if available
	FGuid BlueprintGuid = Blueprint->GetBlueprintGuid();
	if (BlueprintGuid.IsValid())
	{
		OutJson->SetStringField(TEXT("BlueprintGuid"), BlueprintGuid.ToString());
	}

	// Capture variables
	TArray<TSharedPtr<FJsonValue>> VariablesArray;
	CaptureVariables(Blueprint, VariablesArray);
	OutJson->SetArrayField(TEXT("Variables"), VariablesArray);

	// Capture graphs
	TArray<TSharedPtr<FJsonValue>> GraphsArray;
	CaptureGraphs(Blueprint, GraphsArray);
	OutJson->SetArrayField(TEXT("Graphs"), GraphsArray);

	// Capture components
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	CaptureComponents(Blueprint, ComponentsArray);
	OutJson->SetArrayField(TEXT("Components"), ComponentsArray);

	// Capture timelines
	TArray<TSharedPtr<FJsonValue>> TimelinesArray;
	CaptureTimelines(Blueprint, TimelinesArray);
	OutJson->SetArrayField(TEXT("Timelines"), TimelinesArray);

	// Add metadata
	OutJson->SetStringField(TEXT("SnapshotVersion"), TEXT("1.0"));
	OutJson->SetStringField(TEXT("CreatedAt"), FDateTime::Now().ToIso8601());

	// Normalize for deterministic output
	NormalizeJsonObject(OutJson);

	// Generate hash
	FString Hash = GenerateSnapshotHash(OutJson);
	OutJson->SetStringField(TEXT("SnapshotHash"), Hash);

	UE_LOG(LogTemp, Log, TEXT("SnapshotManager: Successfully created snapshot with hash: %s"), *Hash);
	return true;
}

bool FSnapshotManager::CreateSnapshotFromPath(const FString& BlueprintPath, TSharedPtr<FJsonObject>& OutJson)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SnapshotManager: Failed to load Blueprint from path: %s"), *BlueprintPath);
		return false;
	}

	return CreateSnapshot(Blueprint, OutJson);
}

bool FSnapshotManager::SerializeToString(TSharedPtr<FJsonObject> JsonObject, FString& OutJsonString)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
}

bool FSnapshotManager::DeserializeFromString(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	return FJsonSerializer::Deserialize(Reader, OutJsonObject);
}

void FSnapshotManager::CaptureVariables(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutVariablesArray)
{
	if (!Blueprint)
	{
		return;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("SnapshotManager: Capturing %d variables"), Blueprint->NewVariables.Num());

	// Create a map for sorting by GUID
	TMap<FString, TSharedPtr<FJsonObject>> VariableMap;

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VariableObject = MakeShareable(new FJsonObject);
		
		// Core variable information
		VariableObject->SetStringField(TEXT("VariableName"), Variable.VarName.ToString());
		VariableObject->SetStringField(TEXT("VariableGuid"), Variable.VarGuid.ToString());
		VariableObject->SetStringField(TEXT("VarType"), Variable.VarType.PinCategory.ToString());
		VariableObject->SetStringField(TEXT("VarSubCategory"), Variable.VarType.PinSubCategory.ToString());
		
		// Capture VarSubCategoryObject for struct/object/class/interface variables
		if (Variable.VarType.PinSubCategoryObject.IsValid())
		{
			VariableObject->SetStringField(TEXT("VarSubCategoryObject"), Variable.VarType.PinSubCategoryObject->GetName());
		}
		
		UE_LOG(LogTemp, VeryVerbose, TEXT("SnapshotManager: Captured variable - Name: %s, Type: %s, SubType: %s, SubTypeObject: %s"), 
			*Variable.VarName.ToString(), *Variable.VarType.PinCategory.ToString(), *Variable.VarType.PinSubCategory.ToString(),
			Variable.VarType.PinSubCategoryObject.IsValid() ? *Variable.VarType.PinSubCategoryObject->GetName() : TEXT("None"));
		
		// Variable flags
		VariableObject->SetBoolField(TEXT("bExposeOnSpawn"), (Variable.PropertyFlags & CPF_ExposeOnSpawn) != 0);
		VariableObject->SetBoolField(TEXT("bBlueprintReadOnly"), (Variable.PropertyFlags & CPF_BlueprintReadOnly) != 0);
		VariableObject->SetBoolField(TEXT("bInstanceEditable"), (Variable.PropertyFlags & CPF_Edit) != 0);
		VariableObject->SetBoolField(TEXT("bBlueprintVisible"), (Variable.PropertyFlags & CPF_BlueprintVisible) != 0);

		// Category and tooltip
		VariableObject->SetStringField(TEXT("Category"), Variable.Category.ToString());
		VariableObject->SetStringField(TEXT("FriendlyName"), Variable.FriendlyName);
		VariableObject->SetStringField(TEXT("VarTooltip"), TEXT(""));

		// Default value
		if (!Variable.DefaultValue.IsEmpty())
		{
			VariableObject->SetStringField(TEXT("DefaultValue"), Variable.DefaultValue);
		}

		// Replication settings
		if (Variable.ReplicationCondition != COND_None)
		{
			VariableObject->SetNumberField(TEXT("ReplicationCondition"), (int32)Variable.ReplicationCondition);
		}

		// Use GUID as key for sorting
		FString SortKey = Variable.VarGuid.IsValid() ? Variable.VarGuid.ToString() : Variable.VarName.ToString();
		VariableMap.Add(SortKey, VariableObject);
	}

	// Sort by key and add to array
	TArray<FString> SortedKeys;
	VariableMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	for (const FString& Key : SortedKeys)
	{
		OutVariablesArray.Add(MakeShareable(new FJsonValueObject(VariableMap[Key])));
	}
}

void FSnapshotManager::CaptureGraphs(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutGraphsArray)
{
	if (!Blueprint)
	{
		return;
	}

	TArray<UEdGraph*> AllGraphs;
	GetAllBlueprintGraphs(Blueprint, AllGraphs);

	UE_LOG(LogTemp, VeryVerbose, TEXT("SnapshotManager: Capturing %d graphs"), AllGraphs.Num());

	// Create map for sorting
	TMap<FString, TSharedPtr<FJsonObject>> GraphMap;

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> GraphObject = MakeShareable(new FJsonObject);
		CaptureGraph(Graph, GraphObject);

		// Use graph name as sort key
		FString SortKey = Graph->GetName();
		GraphMap.Add(SortKey, GraphObject);
	}

	// Sort by key and add to array
	TArray<FString> SortedKeys;
	GraphMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	for (const FString& Key : SortedKeys)
	{
		OutGraphsArray.Add(MakeShareable(new FJsonValueObject(GraphMap[Key])));
	}
}

void FSnapshotManager::CaptureGraph(UEdGraph* Graph, TSharedPtr<FJsonObject>& OutGraphObject)
{
	if (!Graph || !OutGraphObject.IsValid())
	{
		return;
	}

	// Basic graph information
	OutGraphObject->SetStringField(TEXT("GraphName"), Graph->GetName());
	OutGraphObject->SetStringField(TEXT("GraphGuid"), Graph->GraphGuid.ToString());
	OutGraphObject->SetStringField(TEXT("GraphSchema"), Graph->Schema ? Graph->Schema->GetName() : TEXT(""));

	// Graph type classification (prefer structural detection)
	FString GraphType = TEXT("Unknown");
	bool bIsFunctionGraph = false;
	UK2Node_FunctionEntry* DetectedFunctionEntry = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			DetectedFunctionEntry = Entry;
			bIsFunctionGraph = true;
			break;
		}
	}

	if (bIsFunctionGraph)
	{
		GraphType = TEXT("Function");
		// Derive a stable function identifier; prefer graph name for compatibility across engine versions
		FString FunctionName = Graph->GetName();
		if (FunctionName.IsEmpty())
		{
			FunctionName = DetectedFunctionEntry->GetNodeTitle(ENodeTitleType::ListView).ToString();
		}
		OutGraphObject->SetStringField(TEXT("FunctionName"), FunctionName);
	}
	else if (Graph->GetName().Contains(TEXT("EventGraph")))
	{
		GraphType = TEXT("EventGraph");
	}
	else if (Graph->GetName().Contains(TEXT("ConstructionScript")))
	{
		GraphType = TEXT("ConstructionScript");
	}
	OutGraphObject->SetStringField(TEXT("GraphType"), GraphType);

	// Capture nodes
	TArray<TSharedPtr<FJsonValue>> NodesArray;
	CaptureNodes(Graph, NodesArray);
	OutGraphObject->SetArrayField(TEXT("Nodes"), NodesArray);

	// Capture connections
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	CaptureConnections(Graph, ConnectionsArray);
	OutGraphObject->SetArrayField(TEXT("Connections"), ConnectionsArray);
}

void FSnapshotManager::CaptureNodes(UEdGraph* Graph, TArray<TSharedPtr<FJsonValue>>& OutNodesArray)
{
	if (!Graph)
	{
		return;
	}

	// Create map for sorting by stable key
	TMap<FString, TSharedPtr<FJsonObject>> NodeMap;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedPtr<FJsonObject> NodeObject = MakeShareable(new FJsonObject);
		CaptureNode(Node, NodeObject);

		FString SortKey = GetNodeStableKey(Node);
		NodeMap.Add(SortKey, NodeObject);
	}

	// Sort by key and add to array
	TArray<FString> SortedKeys;
	NodeMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	for (const FString& Key : SortedKeys)
	{
		OutNodesArray.Add(MakeShareable(new FJsonValueObject(NodeMap[Key])));
	}
}

void FSnapshotManager::CaptureNode(UEdGraphNode* Node, TSharedPtr<FJsonObject>& OutNodeObject)
{
	if (!Node || !OutNodeObject.IsValid())
	{
		return;
	}

	// Basic node information
	OutNodeObject->SetStringField(TEXT("NodeName"), Node->GetName());
	OutNodeObject->SetStringField(TEXT("NodeClass"), Node->GetClass()->GetName());
	OutNodeObject->SetStringField(TEXT("NodeTitle"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	
	// Node GUID (most important for identity)
	if (UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		OutNodeObject->SetStringField(TEXT("NodeGuid"), K2Node->NodeGuid.ToString());
	}

	// Node position
	OutNodeObject->SetNumberField(TEXT("NodePosX"), Node->NodePosX);
	OutNodeObject->SetNumberField(TEXT("NodePosY"), Node->NodePosY);

	// Node-specific information
	if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
	{
		if (CallFunctionNode->FunctionReference.GetMemberName() != NAME_None)
		{
			OutNodeObject->SetStringField(TEXT("FunctionName"), CallFunctionNode->FunctionReference.GetMemberName().ToString());
			if (CallFunctionNode->FunctionReference.GetMemberParentClass())
			{
				OutNodeObject->SetStringField(TEXT("FunctionClass"), CallFunctionNode->FunctionReference.GetMemberParentClass()->GetName());
			}
		}
	}
	else if (UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(Node))
	{
		OutNodeObject->SetStringField(TEXT("VariableName"), VariableGetNode->VariableReference.GetMemberName().ToString());
		OutNodeObject->SetStringField(TEXT("VariableGuid"), VariableGetNode->VariableReference.GetMemberGuid().ToString());
	}
	else if (UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(Node))
	{
		OutNodeObject->SetStringField(TEXT("VariableName"), VariableSetNode->VariableReference.GetMemberName().ToString());
		OutNodeObject->SetStringField(TEXT("VariableGuid"), VariableSetNode->VariableReference.GetMemberGuid().ToString());
	}
	else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		OutNodeObject->SetStringField(TEXT("EventName"), EventNode->EventReference.GetMemberName().ToString());
		OutNodeObject->SetStringField(TEXT("CustomFunctionName"), EventNode->CustomFunctionName.ToString());
	}
	else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
	{
		OutNodeObject->SetStringField(TEXT("CustomEventName"), CustomEventNode->CustomFunctionName.ToString());
	}
	else if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>(Node))
	{
		OutNodeObject->SetStringField(TEXT("TimelineName"), TimelineNode->TimelineName.ToString());
	}

	// Capture pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	CapturePins(Node, PinsArray);
	OutNodeObject->SetArrayField(TEXT("Pins"), PinsArray);

	// Advanced properties (node export text as fallback)
	FString NodeExportText = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	if (!NodeExportText.IsEmpty())
	{
		// Store a hash instead of the full text to keep snapshots manageable
		FString ExportHash = FMD5::HashAnsiString(*NodeExportText);
		OutNodeObject->SetStringField(TEXT("ExportHash"), ExportHash);
	}
}

void FSnapshotManager::CapturePins(UEdGraphNode* Node, TArray<TSharedPtr<FJsonValue>>& OutPinsArray)
{
	if (!Node)
	{
		return;
	}

	// Create map for sorting
	TMap<FString, TSharedPtr<FJsonObject>> PinMap;

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PinObject = MakeShareable(new FJsonObject);
		
		PinObject->SetStringField(TEXT("PinName"), Pin->PinName.ToString());
		PinObject->SetStringField(TEXT("PinId"), Pin->PinId.ToString());
		PinObject->SetStringField(TEXT("PinType"), Pin->PinType.PinCategory.ToString());
		PinObject->SetStringField(TEXT("PinSubCategory"), Pin->PinType.PinSubCategory.ToString());
		PinObject->SetStringField(TEXT("Direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
		
		// Capture PinSubCategoryObject for struct/object pins (like Vector2, Vector3, etc.)
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PinObject->SetStringField(TEXT("PinSubCategoryObject"), Pin->PinType.PinSubCategoryObject->GetName());
		}
		
		// Default value
		if (!Pin->DefaultValue.IsEmpty())
		{
			PinObject->SetStringField(TEXT("DefaultValue"), Pin->DefaultValue);
		}

		// Pin flags
		PinObject->SetBoolField(TEXT("bHidden"), Pin->bHidden);
		PinObject->SetBoolField(TEXT("bNotConnectable"), Pin->bNotConnectable);
		PinObject->SetBoolField(TEXT("bDefaultValueIsReadOnly"), Pin->bDefaultValueIsReadOnly);

		// Use PinId as sort key, fallback to name
		FString SortKey = Pin->PinId.IsValid() ? Pin->PinId.ToString() : Pin->PinName.ToString();
		PinMap.Add(SortKey, PinObject);
	}

	// Sort by key and add to array
	TArray<FString> SortedKeys;
	PinMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	for (const FString& Key : SortedKeys)
	{
		OutPinsArray.Add(MakeShareable(new FJsonValueObject(PinMap[Key])));
	}
}

void FSnapshotManager::CaptureConnections(UEdGraph* Graph, TArray<TSharedPtr<FJsonValue>>& OutConnectionsArray)
{
	if (!Graph)
	{
		return;
	}

	TArray<TSharedPtr<FJsonObject>> ConnectionObjects;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* ConnectedPin : Pin->LinkedTo)
			{
				if (!ConnectedPin)
				{
					continue;
				}

				TSharedPtr<FJsonObject> ConnectionObject = MakeShareable(new FJsonObject);
				
				// Source (output) pin information
				ConnectionObject->SetStringField(TEXT("SourceNodeGuid"), GetNodeStableKey(Pin->GetOwningNode()));
				ConnectionObject->SetStringField(TEXT("SourcePinId"), Pin->PinId.ToString());
				ConnectionObject->SetStringField(TEXT("SourcePinName"), Pin->PinName.ToString());
				
				// Target (input) pin information
				ConnectionObject->SetStringField(TEXT("TargetNodeGuid"), GetNodeStableKey(ConnectedPin->GetOwningNode()));
				ConnectionObject->SetStringField(TEXT("TargetPinId"), ConnectedPin->PinId.ToString());
				ConnectionObject->SetStringField(TEXT("TargetPinName"), ConnectedPin->PinName.ToString());
				
				// Connection type
				ConnectionObject->SetStringField(TEXT("ConnectionType"), Pin->PinType.PinCategory.ToString());

				ConnectionObjects.Add(ConnectionObject);
			}
		}
	}

	// Sort connections for deterministic output
	ConnectionObjects.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
	{
		FString KeyA = A->GetStringField(TEXT("SourceNodeGuid")) + A->GetStringField(TEXT("SourcePinId"));
		FString KeyB = B->GetStringField(TEXT("SourceNodeGuid")) + B->GetStringField(TEXT("SourcePinId"));
		return KeyA < KeyB;
	});

	for (const TSharedPtr<FJsonObject>& ConnectionObject : ConnectionObjects)
	{
		OutConnectionsArray.Add(MakeShareable(new FJsonValueObject(ConnectionObject)));
	}
}

void FSnapshotManager::CaptureComponents(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutComponentsArray)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return;
	}

	// Create map for sorting
	TMap<FString, TSharedPtr<FJsonObject>> ComponentMap;

	const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedPtr<FJsonObject> ComponentObject = MakeShareable(new FJsonObject);
		
		ComponentObject->SetStringField(TEXT("ComponentName"), Node->GetVariableName().ToString());
		ComponentObject->SetStringField(TEXT("ComponentClass"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT(""));
		ComponentObject->SetStringField(TEXT("InternalVariableName"), Node->GetVariableName().ToString());
		
		// Parent attachment - simplified for now
		ComponentObject->SetStringField(TEXT("AttachToComponent"), TEXT(""));

		// Component template properties (basic info only)
		if (Node->ComponentTemplate)
		{
			ComponentObject->SetStringField(TEXT("ComponentTemplateName"), Node->ComponentTemplate->GetName());
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate))
			{
				FVector Location = SceneComp->GetRelativeLocation();
				FRotator Rotation = SceneComp->GetRelativeRotation();
				FVector Scale = SceneComp->GetRelativeScale3D();
				
				ComponentObject->SetStringField(TEXT("RelativeLocation"), Location.ToString());
				ComponentObject->SetStringField(TEXT("RelativeRotation"), Rotation.ToString());
				ComponentObject->SetStringField(TEXT("RelativeScale"), Scale.ToString());
			}
		}

		FString SortKey = Node->GetVariableName().ToString();
		ComponentMap.Add(SortKey, ComponentObject);
	}

	// Sort by key and add to array
	TArray<FString> SortedKeys;
	ComponentMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	for (const FString& Key : SortedKeys)
	{
		OutComponentsArray.Add(MakeShareable(new FJsonValueObject(ComponentMap[Key])));
	}
}

void FSnapshotManager::CaptureTimelines(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutTimelinesArray)
{
	if (!Blueprint)
	{
		return;
	}

	// Create map for sorting
	TMap<FString, TSharedPtr<FJsonObject>> TimelineMap;

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		// Check if this is a timeline variable
		if (Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_Object &&
			Variable.VarType.PinSubCategoryObject.IsValid())
		{
			UClass* VarClass = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get());
			if (VarClass && VarClass->GetName().Contains(TEXT("Timeline")))
			{
				TSharedPtr<FJsonObject> TimelineObject = MakeShareable(new FJsonObject);
				
				TimelineObject->SetStringField(TEXT("TimelineName"), Variable.VarName.ToString());
				TimelineObject->SetStringField(TEXT("TimelineGuid"), Variable.VarGuid.ToString());
				TimelineObject->SetStringField(TEXT("TimelineClass"), VarClass->GetName());

				FString SortKey = Variable.VarGuid.IsValid() ? Variable.VarGuid.ToString() : Variable.VarName.ToString();
				TimelineMap.Add(SortKey, TimelineObject);
			}
		}
	}

	// Sort by key and add to array
	TArray<FString> SortedKeys;
	TimelineMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	for (const FString& Key : SortedKeys)
	{
		OutTimelinesArray.Add(MakeShareable(new FJsonValueObject(TimelineMap[Key])));
	}
}

FString FSnapshotManager::GetStableKey(UObject* Object)
{
	if (!Object)
	{
		return TEXT("");
	}

	// Try to get a GUID first
	if (UK2Node* K2Node = Cast<UK2Node>(Object))
	{
		if (K2Node->NodeGuid.IsValid())
		{
			return K2Node->NodeGuid.ToString();
		}
	}

	// Fallback to path + name + class
	return FString::Printf(TEXT("%s_%s_%s"), 
		*Object->GetPathName(), 
		*Object->GetName(), 
		*Object->GetClass()->GetName());
}

FString FSnapshotManager::GetNodeStableKey(UEdGraphNode* Node)
{
	if (!Node)
	{
		return TEXT("");
	}

	// Priority 1: NodeGuid for K2Nodes
	if (UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		if (K2Node->NodeGuid.IsValid())
		{
			return K2Node->NodeGuid.ToString();
		}
	}

	// Priority 2: Semantic match for specific node types
	if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
	{
		return FString::Printf(TEXT("CallFunction_%s_%s"), 
			*CallFunctionNode->FunctionReference.GetMemberName().ToString(),
			*CallFunctionNode->GetClass()->GetName());
	}
	else if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
	{
		return FString::Printf(TEXT("VarGet_%s_%s"), 
			*VarGetNode->VariableReference.GetMemberName().ToString(),
			VarGetNode->VariableReference.GetMemberGuid().IsValid() ? *VarGetNode->VariableReference.GetMemberGuid().ToString() : TEXT("NoGuid"));
	}
	else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
	{
		return FString::Printf(TEXT("VarSet_%s_%s"), 
			*VarSetNode->VariableReference.GetMemberName().ToString(),
			VarSetNode->VariableReference.GetMemberGuid().IsValid() ? *VarSetNode->VariableReference.GetMemberGuid().ToString() : TEXT("NoGuid"));
	}

	// Priority 3: Fallback to object path + class
	return FString::Printf(TEXT("%s_%s"), *Node->GetPathName(), *Node->GetClass()->GetName());
}

void FSnapshotManager::NormalizeJsonObject(TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	// Sort arrays by their key fields
	const TSharedPtr<FJsonValue>* VariablesValue = JsonObject->Values.Find(TEXT("Variables"));
	if (VariablesValue && (*VariablesValue)->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> VariablesArray = (*VariablesValue)->AsArray();
		SortJsonArrayByField(VariablesArray, TEXT("VariableGuid"));
		JsonObject->SetArrayField(TEXT("Variables"), VariablesArray);
	}

	const TSharedPtr<FJsonValue>* GraphsValue = JsonObject->Values.Find(TEXT("Graphs"));
	if (GraphsValue && (*GraphsValue)->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> GraphsArray = (*GraphsValue)->AsArray();
		SortJsonArrayByField(GraphsArray, TEXT("GraphName"));
		JsonObject->SetArrayField(TEXT("Graphs"), GraphsArray);

		// Normalize each graph
		for (TSharedPtr<FJsonValue>& GraphValue : GraphsArray)
		{
			if (GraphValue->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> GraphObject = GraphValue->AsObject();
				
				// Sort nodes
				const TSharedPtr<FJsonValue>* NodesValue = GraphObject->Values.Find(TEXT("Nodes"));
				if (NodesValue && (*NodesValue)->Type == EJson::Array)
				{
					TArray<TSharedPtr<FJsonValue>> NodesArray = (*NodesValue)->AsArray();
					SortJsonArrayByField(NodesArray, TEXT("NodeGuid"));
					GraphObject->SetArrayField(TEXT("Nodes"), NodesArray);
				}

				// Sort connections
				const TSharedPtr<FJsonValue>* ConnectionsValue = GraphObject->Values.Find(TEXT("Connections"));
				if (ConnectionsValue && (*ConnectionsValue)->Type == EJson::Array)
				{
					TArray<TSharedPtr<FJsonValue>> ConnectionsArray = (*ConnectionsValue)->AsArray();
					SortJsonArrayByField(ConnectionsArray, TEXT("SourceNodeGuid"));
					GraphObject->SetArrayField(TEXT("Connections"), ConnectionsArray);
				}
			}
		}
	}

	// Remove transient fields
	RemoveTransientFields(JsonObject);
}

void FSnapshotManager::SortJsonArrayByField(TArray<TSharedPtr<FJsonValue>>& JsonArray, const FString& SortField)
{
	JsonArray.Sort([SortField](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (A->Type != EJson::Object || B->Type != EJson::Object)
		{
			return false;
		}

		TSharedPtr<FJsonObject> ObjA = A->AsObject();
		TSharedPtr<FJsonObject> ObjB = B->AsObject();

		FString ValueA = ObjA->GetStringField(SortField);
		FString ValueB = ObjB->GetStringField(SortField);

		return ValueA < ValueB;
	});
}

void FSnapshotManager::RemoveTransientFields(TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	// Remove fields that are not relevant for diffing
	TArray<FString> FieldsToRemove = {
		TEXT("NodePosX"), // Node positions are transient
		TEXT("NodePosY"),
		TEXT("CreatedAt") // Timestamps are transient
	};

	for (const FString& Field : FieldsToRemove)
	{
		JsonObject->RemoveField(Field);
	}
}

FString FSnapshotManager::GenerateSnapshotHash(TSharedPtr<FJsonObject> JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return TEXT("");
	}

	// Create a temporary copy without the hash field
	TSharedPtr<FJsonObject> TempObject = MakeShareable(new FJsonObject(*JsonObject));
	TempObject->RemoveField(TEXT("SnapshotHash"));

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(TempObject.ToSharedRef(), Writer);

	return FMD5::HashAnsiString(*JsonString);
}

void FSnapshotManager::GetAllBlueprintGraphs(UBlueprint* Blueprint, TArray<UEdGraph*>& OutGraphs)
{
	if (!Blueprint)
	{
		return;
	}

	OutGraphs.Empty();
	
	// Try UE 5.6+ method first, fallback to UE 5.5 approach
	// Note: GetAllGraphs is only available in UE 5.6+
	#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		Blueprint->GetAllGraphs(OutGraphs);
	#else
		// UE 5.5 and earlier approach
		OutGraphs = Blueprint->UbergraphPages;
		OutGraphs.Append(Blueprint->FunctionGraphs);
		OutGraphs.Append(Blueprint->MacroGraphs);
	#endif
}

bool FSnapshotManager::CreateStructuredSnapshot(UBlueprint* Blueprint, FBlueprintMergeData& OutBlueprintData)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SnapshotManager: Blueprint is null"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("SnapshotManager: Creating structured snapshot for Blueprint: %s"), *Blueprint->GetName());

	// Create the structured data directly
	OutBlueprintData = FBlueprintMergeData(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SnapshotManager: Structured snapshot created - Variables: %d, Graphs: %d"), 
		OutBlueprintData.Variables.Num(), OutBlueprintData.Graphs.Num());

	return true;
}
