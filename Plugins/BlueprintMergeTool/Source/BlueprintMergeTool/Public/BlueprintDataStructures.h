#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "BlueprintDataStructures.generated.h"

// Forward declarations
class FSnapshotManager;

/**
 * Represents a Blueprint variable with all its properties
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FBlueprintMergeVariableData
{
	GENERATED_BODY()

	// Core variable information
	UPROPERTY()
	FName VariableName;

	UPROPERTY()
	FGuid VariableGuid;

	UPROPERTY()
	FEdGraphPinType VarType;

	UPROPERTY()
	FText Category;

	UPROPERTY()
	FString FriendlyName;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	uint64 PropertyFlags;

	UPROPERTY()
	TArray<FBPVariableMetaDataEntry> MetaDataArray;

	UPROPERTY()
	int32 ReplicationCondition;

	FBlueprintMergeVariableData()
		: PropertyFlags(0)
		, ReplicationCondition(0)
	{
	}

	// Constructor from FBPVariableDescription
	FBlueprintMergeVariableData(const FBPVariableDescription& VariableDesc)
		: VariableName(VariableDesc.VarName)
		, VariableGuid(VariableDesc.VarGuid)
		, VarType(VariableDesc.VarType)
		, Category(VariableDesc.Category)
		, FriendlyName(VariableDesc.FriendlyName)
		, DefaultValue(VariableDesc.DefaultValue)
		, PropertyFlags(VariableDesc.PropertyFlags)
		, MetaDataArray(VariableDesc.MetaDataArray)
		, ReplicationCondition((int32)VariableDesc.ReplicationCondition.GetValue())
	{
	}

	// Convert to FBPVariableDescription
	FBPVariableDescription ToVariableDescription() const
	{
		FBPVariableDescription Desc;
		Desc.VarName = VariableName;
		Desc.VarGuid = VariableGuid;
		Desc.VarType = VarType;
		Desc.Category = Category;
		Desc.FriendlyName = FriendlyName;
		Desc.DefaultValue = DefaultValue;
		Desc.PropertyFlags = PropertyFlags;
		Desc.MetaDataArray = MetaDataArray;
		Desc.ReplicationCondition = (ELifetimeCondition)ReplicationCondition;
		return Desc;
	}
};

/**
 * Represents a Blueprint node with all its properties
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FBlueprintMergeNodeData
{
	GENERATED_BODY()

	// Basic node information
	UPROPERTY()
	FString NodeName;

	UPROPERTY()
	FString NodeClass;

	UPROPERTY()
	FString NodeTitle;

	UPROPERTY()
	FGuid NodeGuid;

	UPROPERTY()
	FVector2D NodePosition;

	// Node-specific data
	UPROPERTY()
	TMap<FString, FString> NodeProperties;

	// Enhanced data for custom classes and functions
	UPROPERTY()
	FString FunctionClassPath; // Full class path for custom functions

	UPROPERTY()
	FString FunctionModuleName; // Module name for custom functions

	UPROPERTY()
	TArray<FString> CustomParameterTypes; // Custom parameter types

	UPROPERTY()
	TMap<FString, FString> CustomReferences; // Custom class/struct/enum references

	// Pin information
	UPROPERTY()
	TArray<FString> InputPins;

	UPROPERTY()
	TArray<FString> OutputPins;

	FBlueprintMergeNodeData()
		: NodePosition(FVector2D::ZeroVector)
	{
	}

	// Constructor from UEdGraphNode
	FBlueprintMergeNodeData(UEdGraphNode* Node)
	{
		if (Node)
		{
			NodeName = Node->GetName();
			NodeClass = Node->GetClass()->GetName();
			NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			NodePosition = FVector2D(Node->NodePosX, Node->NodePosY);

			if (UK2Node* K2Node = Cast<UK2Node>(Node))
			{
				NodeGuid = K2Node->NodeGuid;
			}

			// Capture node-specific properties
			CaptureNodeProperties(Node);
		}
	}

private:
	void CaptureNodeProperties(UEdGraphNode* Node)
	{
		if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			if (CallFunctionNode->FunctionReference.GetMemberName() != NAME_None)
			{
				NodeProperties.Add(TEXT("FunctionName"), CallFunctionNode->FunctionReference.GetMemberName().ToString());
				
				// Enhanced capture for custom classes
				if (CallFunctionNode->FunctionReference.GetMemberParentClass())
				{
					UClass* ParentClass = CallFunctionNode->FunctionReference.GetMemberParentClass();
					NodeProperties.Add(TEXT("FunctionClass"), ParentClass->GetName());
					
					// Capture full class path for custom classes
					FunctionClassPath = ParentClass->GetPathName();
					FunctionModuleName = ParentClass->GetPackage()->GetName();
					
					// Check if this is a custom class (not in Engine modules)
					if (!FunctionModuleName.StartsWith(TEXT("Engine")) && 
						!FunctionModuleName.StartsWith(TEXT("Core")) &&
						!FunctionModuleName.StartsWith(TEXT("UnrealEd")))
					{
						CustomReferences.Add(TEXT("CustomClass"), FunctionClassPath);
						UE_LOG(LogTemp, Log, TEXT("BlueprintMerge: Detected custom class reference: %s"), *FunctionClassPath);
					}
				}
				
				// Capture function signature for custom functions
				if (UFunction* Function = CallFunctionNode->FunctionReference.ResolveMember<UFunction>())
				{
					// Capture parameter types
					for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
					{
						FProperty* Property = *PropIt;
						if (Property && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
						{
							FString ParamType = Property->GetCPPType();
							CustomParameterTypes.Add(ParamType);
							
							// Check for custom types
							if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
							{
								if (ObjectProp->PropertyClass && 
									!ObjectProp->PropertyClass->GetPackage()->GetName().StartsWith(TEXT("Engine")))
								{
									CustomReferences.Add(FString::Printf(TEXT("Param_%s"), *Property->GetName()), 
										ObjectProp->PropertyClass->GetPathName());
								}
							}
						}
					}
				}
			}
		}
		else if (UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(Node))
		{
			NodeProperties.Add(TEXT("VariableName"), VariableGetNode->VariableReference.GetMemberName().ToString());
			NodeProperties.Add(TEXT("VariableGuid"), VariableGetNode->VariableReference.GetMemberGuid().ToString());
			
			// Capture variable type information
			if (FProperty* VariableProperty = VariableGetNode->VariableReference.ResolveMember<FProperty>())
			{
				FString VarType = VariableProperty->GetCPPType();
				NodeProperties.Add(TEXT("VariableType"), VarType);
				
				// Check for custom variable types
				if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(VariableProperty))
				{
					if (ObjectProp->PropertyClass && 
						!ObjectProp->PropertyClass->GetPackage()->GetName().StartsWith(TEXT("Engine")))
					{
						CustomReferences.Add(TEXT("VariableType"), ObjectProp->PropertyClass->GetPathName());
					}
				}
			}
		}
		else if (UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(Node))
		{
			NodeProperties.Add(TEXT("VariableName"), VariableSetNode->VariableReference.GetMemberName().ToString());
			NodeProperties.Add(TEXT("VariableGuid"), VariableSetNode->VariableReference.GetMemberGuid().ToString());
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			NodeProperties.Add(TEXT("EventName"), EventNode->EventReference.GetMemberName().ToString());
			NodeProperties.Add(TEXT("CustomFunctionName"), EventNode->CustomFunctionName.ToString());
		}
		else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
		{
			NodeProperties.Add(TEXT("CustomEventName"), CustomEventNode->CustomFunctionName.ToString());
		}
	}
};

/**
 * Represents a Blueprint graph with all its data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FBlueprintMergeGraphData
{
	GENERATED_BODY()

	// Basic graph information
	UPROPERTY()
	FString GraphName;

	UPROPERTY()
	FGuid GraphGuid;

	UPROPERTY()
	FString GraphSchema;

	UPROPERTY()
	FString GraphType;

	// Graph contents
	UPROPERTY()
	TArray<FBlueprintMergeNodeData> Nodes;

	UPROPERTY()
	TArray<FString> Connections; // Simplified for now

	FBlueprintMergeGraphData()
	{
	}

	// Constructor from UEdGraph
	FBlueprintMergeGraphData(UEdGraph* Graph)
	{
		if (Graph)
		{
			GraphName = Graph->GetName();
			GraphGuid = Graph->GraphGuid;
			GraphSchema = Graph->Schema ? Graph->Schema->GetName() : TEXT("");

			// Determine graph type
			if (Graph->GetName().Contains(TEXT("EventGraph")))
			{
				GraphType = TEXT("EventGraph");
			}
			else if (Graph->GetName().Contains(TEXT("ConstructionScript")))
			{
				GraphType = TEXT("ConstructionScript");
			}
			else if (UBlueprint* Blueprint = Cast<UBlueprint>(Graph->GetOuter()))
			{
				if (FBlueprintEditorUtils::IsGraphNameUnique(Blueprint, Graph->GetFName()))
				{
					GraphType = TEXT("Function");
				}
			}
			else
			{
				GraphType = TEXT("Unknown");
			}

			// Capture nodes
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					Nodes.Add(FBlueprintMergeNodeData(Node));
				}
			}
		}
	}
};

/**
 * Complete Blueprint data structure
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FBlueprintMergeData
{
	GENERATED_BODY()

	// Blueprint identification
	UPROPERTY()
	FString BlueprintName;

	UPROPERTY()
	FGuid BlueprintGuid;

	UPROPERTY()
	FString BlueprintPath;

	// Blueprint contents
	UPROPERTY()
	TArray<FBlueprintMergeVariableData> Variables;

	UPROPERTY()
	TArray<FBlueprintMergeGraphData> Graphs;

	UPROPERTY()
	TArray<FString> Components; // Simplified for now

	FBlueprintMergeData()
	{
	}

	// Constructor from UBlueprint
	FBlueprintMergeData(UBlueprint* Blueprint)
	{
		if (Blueprint)
		{
			BlueprintName = Blueprint->GetName();
			BlueprintGuid = Blueprint->GetBlueprintGuid();
			BlueprintPath = Blueprint->GetPathName();

			// Capture variables
			for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
			{
				Variables.Add(FBlueprintMergeVariableData(Variable));
			}

		// Capture graphs - implementation moved to .cpp file
		CaptureGraphs(Blueprint);
		}
	}

private:
	// Helper method to capture graphs (implemented in .cpp file)
	void CaptureGraphs(UBlueprint* Blueprint);
};
