#include "UnrealToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BlueprintJsonParser.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealToolRegistry, Log, All);

TMap<FName, FUnrealToolRegistry::FToolEntry>& FUnrealToolRegistry::GetMap()
{
	static TMap<FName, FToolEntry> Map;
	return Map;
}

void FUnrealToolRegistry::RegisterTool(FName Name, const FString& Description, const FString& InputSchema, FExecuteToolDelegate Delegate)
{
	FToolEntry Entry;
	Entry.Description = Description;
	Entry.InputSchema = InputSchema;
	Entry.Delegate = Delegate;
	GetMap().Add(Name, MoveTemp(Entry));
}

bool FUnrealToolRegistry::ExecuteTool(FName Name, const FString& ArgsJson, FString& OutResult)
{
	FToolEntry* Entry = GetMap().Find(Name);
	if (!Entry || !Entry->Delegate.IsBound())
	{
		OutResult = FString::Printf(TEXT("Tool '%s' not found or not bound"), *Name.ToString());
		return false;
	}
	Entry->Delegate.Execute(ArgsJson, OutResult);
	return true;
}

TArray<FUnrealToolDef> FUnrealToolRegistry::GetRegisteredTools()
{
	TArray<FUnrealToolDef> Out;
	for (const auto& Pair : GetMap())
	{
		FUnrealToolDef Def;
		Def.Name = Pair.Key.ToString();
		Def.Description = Pair.Value.Description;
		Def.InputSchema = Pair.Value.InputSchema;
		Out.Add(Def);
	}
	return Out;
}

static void Tool_CreateBlueprint(const FString& ArgsJson, FString& OutResult)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgsJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		OutResult = TEXT("Invalid JSON for create_blueprint");
		return;
	}
	FString Name = Obj->GetStringField(TEXT("name"));
	FString ParentClassPath = Obj->GetStringField(TEXT("parent_class_path"));
	if (Name.IsEmpty())
	{
		OutResult = TEXT("create_blueprint requires 'name'");
		return;
	}
	UClass* ParentClass = AActor::StaticClass();
	if (!ParentClassPath.IsEmpty())
	{
		UClass* Loaded = LoadObject<UClass>(nullptr, *ParentClassPath);
		if (Loaded) ParentClass = Loaded;
	}
	FString PackagePath = TEXT("/Game/AI_Generated");
	FString AssetName = Name.Replace(TEXT(" "), TEXT("_")).Replace(TEXT("-"), TEXT("_"));
	FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
	int32 Counter = 1;
	while (FindObject<UPackage>(nullptr, *PackageName) != nullptr)
	{
		AssetName = FString::Printf(TEXT("%s_%d"), *Name.Replace(TEXT(" "), TEXT("_")), Counter++);
		PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
		if (Counter > 100) { OutResult = TEXT("Too many name conflicts"); return; }
	}
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) { OutResult = FString::Printf(TEXT("Failed to create package: %s"), *PackageName); return; }
	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, *AssetName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!BP) { OutResult = TEXT("CreateBlueprint returned null"); return; }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FString SavePath;
	if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, SavePath))
	{
		SavePath = FPaths::GetPath(SavePath) / (AssetName + TEXT(".uasset"));
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		if (UPackage::SavePackage(Package, BP, *SavePath, SaveArgs))
			OutResult = FString::Printf(TEXT("OK: %s"), *PackageName);
		else
			OutResult = FString::Printf(TEXT("Created but save failed: %s"), *PackageName);
	}
	else
		OutResult = FString::Printf(TEXT("OK: %s"), *PackageName);
}

static void Tool_CreateNode(const FString& ArgsJson, FString& OutResult)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgsJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		OutResult = TEXT("Invalid JSON for create_node");
		return;
	}
	FString BlueprintPath = Obj->GetStringField(TEXT("blueprint_path"));
	FString GraphName = Obj->GetStringField(TEXT("graph_name"));
	FString NodeType = Obj->GetStringField(TEXT("node_type"));
	double X = Obj->GetNumberField(TEXT("position_x"));
	double Y = Obj->GetNumberField(TEXT("position_y"));
	if (BlueprintPath.IsEmpty() || GraphName.IsEmpty())
	{
		OutResult = TEXT("create_node requires blueprint_path and graph_name");
		return;
	}
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP)
	{
		OutResult = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath);
		return;
	}
	UEdGraph* Graph = FBlueprintEditorUtils::FindGraph(BP, FName(*GraphName));
	if (!Graph)
	{
		OutResult = FString::Printf(TEXT("Graph not found: %s"), *GraphName);
		return;
	}
	UEdGraphNode* NewNode = nullptr;
	if (NodeType.Contains(TEXT("CallFunction")) || NodeType.Contains(TEXT("Print")))
	{
		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		if (CallNode)
		{
			UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
			if (PrintFunc) CallNode->SetFromFunction(PrintFunc);
			CallNode->CreateNewGuid();
			CallNode->NodePosX = (int32)X;
			CallNode->NodePosY = (int32)Y;
			Graph->AddNode(CallNode);
			NewNode = CallNode;
		}
	}
	if (!NewNode)
	{
		OutResult = FString::Printf(TEXT("Unsupported node_type or creation failed: %s"), *NodeType);
		return;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	OutResult = FString::Printf(TEXT("OK: node %s"), *NewNode->NodeGuid.ToString());
}

static void Tool_LinkNodes(const FString& ArgsJson, FString& OutResult)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgsJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		OutResult = TEXT("Invalid JSON for link_nodes");
		return;
	}
	FString BlueprintPath = Obj->GetStringField(TEXT("blueprint_path"));
	FString GraphName = Obj->GetStringField(TEXT("graph_name"));
	FString FromIdStr = Obj->GetStringField(TEXT("from_node_id"));
	FString FromPin = Obj->GetStringField(TEXT("from_pin"));
	FString ToIdStr = Obj->GetStringField(TEXT("to_node_id"));
	FString ToPin = Obj->GetStringField(TEXT("to_pin"));
	if (BlueprintPath.IsEmpty() || GraphName.IsEmpty() || FromIdStr.IsEmpty() || ToIdStr.IsEmpty())
	{
		OutResult = TEXT("link_nodes requires blueprint_path, graph_name, from_node_id, to_node_id");
		return;
	}
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP) { OutResult = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath); return; }
	UEdGraph* Graph = FBlueprintEditorUtils::FindGraph(BP, FName(*GraphName));
	if (!Graph) { OutResult = FString::Printf(TEXT("Graph not found: %s"), *GraphName); return; }
	FGuid FromGuid, ToGuid;
	if (!FGuid::Parse(FromIdStr, FromGuid) || !FGuid::Parse(ToIdStr, ToGuid))
	{
		OutResult = TEXT("Invalid from_node_id or to_node_id GUID");
		return;
	}
	UEdGraphNode* FromNode = nullptr, *ToNode = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node->NodeGuid == FromGuid) FromNode = Node;
		if (Node->NodeGuid == ToGuid) ToNode = Node;
	}
	if (!FromNode || !ToNode)
	{
		OutResult = TEXT("One or both nodes not found by GUID");
		return;
	}
	UEdGraphPin* FromPinObj = FromPin.IsEmpty() ? nullptr : FromNode->FindPin(FName(*FromPin), EGPD_Output);
	if (!FromPinObj) FromPinObj = FromNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	if (!FromPinObj && FromNode->Pins.Num() > 0) FromPinObj = FromNode->FindPin(FromNode->Pins[0]->PinName, EGPD_Output);
	UEdGraphPin* ToPinObj = ToPin.IsEmpty() ? nullptr : ToNode->FindPin(FName(*ToPin), EGPD_Input);
	if (!ToPinObj) ToPinObj = ToNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	if (!ToPinObj && ToNode->Pins.Num() > 0) ToPinObj = ToNode->FindPin(ToNode->Pins[0]->PinName, EGPD_Input);
	if (!FromPinObj || !ToPinObj)
	{
		OutResult = TEXT("Could not resolve from_pin or to_pin");
		return;
	}
	FromPinObj->MakeLinkTo(ToPinObj);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	OutResult = TEXT("OK");
}

static void Tool_CreateComponent(const FString& ArgsJson, FString& OutResult)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgsJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		OutResult = TEXT("Invalid JSON for create_component");
		return;
	}
	FString BlueprintPath = Obj->GetStringField(TEXT("blueprint_path"));
	FString ComponentClassPath = Obj->GetStringField(TEXT("component_class"));
	if (BlueprintPath.IsEmpty())
	{
		OutResult = TEXT("create_component requires blueprint_path");
		return;
	}
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP) { OutResult = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath); return; }
	if (!BP->SimpleConstructionScript) { OutResult = TEXT("Blueprint has no SCS"); return; }
	UClass* CompClass = USceneComponent::StaticClass();
	if (!ComponentClassPath.IsEmpty())
	{
		UClass* Loaded = LoadObject<UClass>(nullptr, *ComponentClassPath);
		if (Loaded) CompClass = Loaded;
	}
	USCS_Node* NewNode = BP->SimpleConstructionScript->CreateNode(CompClass);
	if (!NewNode) { OutResult = TEXT("CreateNode failed"); return; }
	USCS_Node* Root = BP->SimpleConstructionScript->GetDefaultSceneRootNode();
	if (Root) Root->AddChildNode(NewNode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	OutResult = FString::Printf(TEXT("OK: %s"), *NewNode->GetName());
}

void FUnrealToolRegistry::RegisterBuiltInTools()
{
	RegisterTool(
		FName(TEXT("create_blueprint")),
		TEXT("Create a new Blueprint asset. Args: name (string), parent_class_path (optional string, e.g. /Script/Engine.Actor)"),
		TEXT("{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"parent_class_path\":{\"type\":\"string\"}}}"),
		FExecuteToolDelegate::CreateStatic(Tool_CreateBlueprint)
	);
	RegisterTool(
		FName(TEXT("create_node")),
		TEXT("Add a node to a Blueprint graph. Args: blueprint_path, graph_name, node_type, position_x, position_y"),
		TEXT("{\"type\":\"object\",\"properties\":{\"blueprint_path\":{\"type\":\"string\"},\"graph_name\":{\"type\":\"string\"},\"node_type\":{\"type\":\"string\"},\"position_x\":{\"type\":\"number\"},\"position_y\":{\"type\":\"number\"}}}"),
		FExecuteToolDelegate::CreateStatic(Tool_CreateNode)
	);
	RegisterTool(
		FName(TEXT("link_nodes")),
		TEXT("Connect two nodes in a Blueprint graph. Args: blueprint_path, graph_name, from_node_id, from_pin, to_node_id, to_pin"),
		TEXT("{\"type\":\"object\",\"properties\":{\"blueprint_path\":{\"type\":\"string\"},\"graph_name\":{\"type\":\"string\"},\"from_node_id\":{\"type\":\"string\"},\"from_pin\":{\"type\":\"string\"},\"to_node_id\":{\"type\":\"string\"},\"to_pin\":{\"type\":\"string\"}}}"),
		FExecuteToolDelegate::CreateStatic(Tool_LinkNodes)
	);
	RegisterTool(
		FName(TEXT("create_component")),
		TEXT("Add a component to a Blueprint. Args: blueprint_path, component_class (optional, e.g. /Script/Engine.SceneComponent)"),
		TEXT("{\"type\":\"object\",\"properties\":{\"blueprint_path\":{\"type\":\"string\"},\"component_class\":{\"type\":\"string\"}}}"),
		FExecuteToolDelegate::CreateStatic(Tool_CreateComponent)
	);
	UE_LOG(LogUnrealToolRegistry, Log, TEXT("Registered %d built-in MCP tools"), GetMap().Num());
}
