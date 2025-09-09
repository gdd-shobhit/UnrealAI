#include "../Public/ApplyEngine.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Engine/Engine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/SCS_Node.h"

bool FApplyEngine::ApplyMergePlan(
	UBlueprint* TargetBlueprint,
	const FMergePlan& MergePlan,
	FApplyResult& OutResult)
{
	if (!TargetBlueprint)
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Target Blueprint is null");
		return false;
	}

	if (!IsValid(TargetBlueprint))
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Target Blueprint is not valid");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Applying merge plan to Blueprint: %s"), *TargetBlueprint->GetName());
	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: %d operations to apply"), MergePlan.AutoResolvedOperations.Num());

	// Ensure we're on the game thread
	if (!IsInGameThread())
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Must be called from game thread"));
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Operations must be applied from the game thread");
		return false;
	}

	// Create undo transaction
	TSharedPtr<FScopedTransaction> Transaction = CreateUndoTransaction(
		FString::Printf(TEXT("Apply Blueprint Merge (%d operations)"), MergePlan.AutoResolvedOperations.Num())
	);

	// Clear result
	OutResult = FApplyResult();
	OutResult.bSuccess = true;

	// Apply each operation
	for (int32 i = 0; i < MergePlan.AutoResolvedOperations.Num(); i++)
	{
		const FMergeOperation& Operation = MergePlan.AutoResolvedOperations[i];
		FString OperationError;

		bool bOperationSuccess = ApplyOperation(TargetBlueprint, Operation, OperationError);

		FString OperationName = FString::Printf(TEXT("Op %d: %s on %s"), 
			i, 
			*StaticEnum<EMergeOperationType>()->GetValueAsString(Operation.OperationType), 
			*Operation.TargetId);

		if (bOperationSuccess)
		{
			OutResult.AppliedOperations.Add(OperationName);
			LogOperationResult(Operation.OperationType, Operation.TargetId, true);
		}
		else
		{
			OutResult.FailedOperations.Add(FString::Printf(TEXT("%s: %s"), *OperationName, *OperationError));
			OutResult.Warnings.Add(OperationError);
			LogOperationResult(Operation.OperationType, Operation.TargetId, false, OperationError);

			// Continue with other operations even if one fails
		}
	}

	// Validate Blueprint integrity after all operations
	TArray<FString> ValidationErrors;
	if (!ValidateBlueprintIntegrity(TargetBlueprint, ValidationErrors))
	{
		for (const FString& ValidationError : ValidationErrors)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Validation: %s"), *ValidationError));
		}
	}

	// Mark Blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBlueprint);

	// Compile and save
	if (OutResult.FailedOperations.Num() == 0) // Only compile if no critical failures
	{
		if (!CompileAndSaveBlueprint(TargetBlueprint, true, OutResult))
		{
			OutResult.bSuccess = false;
			OutResult.ErrorMessage = TEXT("Failed to compile or save Blueprint after applying operations");
		}
	}
	else
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = FString::Printf(TEXT("%d operations failed"), OutResult.FailedOperations.Num());
	}

	// Refresh editor if Blueprint is open
	RefreshBlueprintEditor(TargetBlueprint);

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Merge plan application completed. Success: %s, Applied: %d, Failed: %d"),
		OutResult.bSuccess ? TEXT("true") : TEXT("false"),
		OutResult.AppliedOperations.Num(),
		OutResult.FailedOperations.Num());

	return OutResult.bSuccess;
}

bool FApplyEngine::ApplyOperation(
	UBlueprint* TargetBlueprint,
	const FMergeOperation& Operation,
	FString& OutError)
{
	if (!TargetBlueprint)
	{
		OutError = TEXT("Target Blueprint is null");
		return false;
	}

	switch (Operation.OperationType)
	{
	case EMergeOperationType::AddVariable:
		{
			// Parse variable data from additional data
			FString VariableDataJson = Operation.AdditionalData.FindRef(TEXT("VariableData"));
			if (!VariableDataJson.IsEmpty())
			{
				TSharedPtr<FJsonObject> VariableData;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(VariableDataJson);
				if (FJsonSerializer::Deserialize(Reader, VariableData))
				{
					return AddVariable(TargetBlueprint, VariableData, OutError);
				}
			}
			OutError = TEXT("Invalid variable data for AddVariable operation");
			return false;
		}

	case EMergeOperationType::RemoveVariable:
		return RemoveVariable(TargetBlueprint, Operation.TargetId, OutError);

	case EMergeOperationType::UpdateVariable:
		return UpdateVariable(TargetBlueprint, Operation.TargetId, Operation.PropertyName, Operation.NewValue, OutError);

	case EMergeOperationType::RemapVariableGuid:
		return RemapVariableGuid(TargetBlueprint, Operation.OldValue, Operation.NewValue, OutError);

	case EMergeOperationType::AddNode:
		{
			FString NodeDataJson = Operation.AdditionalData.FindRef(TEXT("NodeData"));
			if (!NodeDataJson.IsEmpty())
			{
				TSharedPtr<FJsonObject> NodeData;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NodeDataJson);
				if (FJsonSerializer::Deserialize(Reader, NodeData))
				{
					return AddNode(TargetBlueprint, Operation.TargetGraph, NodeData, OutError);
				}
			}
			OutError = TEXT("Invalid node data for AddNode operation");
			return false;
		}

	case EMergeOperationType::RemoveNode:
		return RemoveNode(TargetBlueprint, Operation.TargetGraph, Operation.TargetId, OutError);

	case EMergeOperationType::UpdateNodeProperty:
		return UpdateNodeProperty(TargetBlueprint, Operation.TargetGraph, Operation.TargetId, Operation.PropertyName, Operation.NewValue, OutError);

	case EMergeOperationType::MoveNode:
		{
			// Parse position from NewValue
			TArray<FString> Coords;
			Operation.NewValue.ParseIntoArray(Coords, TEXT(","), true);
			if (Coords.Num() >= 2)
			{
				float NewX = FCString::Atof(*Coords[0]);
				float NewY = FCString::Atof(*Coords[1]);
				return MoveNode(TargetBlueprint, Operation.TargetGraph, Operation.TargetId, NewX, NewY, OutError);
			}
			OutError = TEXT("Invalid position data for MoveNode operation");
			return false;
		}

	case EMergeOperationType::LinkPins:
		{
			// Parse connection data from TargetId (format: "SourceGuid.SourcePin->TargetGuid.TargetPin")
			FString SourcePart, TargetPart;
			if (Operation.TargetId.Split(TEXT("->"), &SourcePart, &TargetPart))
			{
				FString SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName;
				if (SourcePart.Split(TEXT("."), &SourceNodeGuid, &SourcePinName) &&
					TargetPart.Split(TEXT("."), &TargetNodeGuid, &TargetPinName))
				{
					return LinkPins(TargetBlueprint, Operation.TargetGraph, SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName, OutError);
				}
			}
			OutError = TEXT("Invalid connection format for LinkPins operation");
			return false;
		}

	case EMergeOperationType::UnlinkPins:
		{
			// Parse connection data similar to LinkPins
			FString SourcePart, TargetPart;
			if (Operation.TargetId.Split(TEXT("->"), &SourcePart, &TargetPart))
			{
				FString SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName;
				if (SourcePart.Split(TEXT("."), &SourceNodeGuid, &SourcePinName) &&
					TargetPart.Split(TEXT("."), &TargetNodeGuid, &TargetPinName))
				{
					return UnlinkPins(TargetBlueprint, Operation.TargetGraph, SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName, OutError);
				}
			}
			OutError = TEXT("Invalid connection format for UnlinkPins operation");
			return false;
		}

	case EMergeOperationType::AddComponent:
		{
			FString ComponentDataJson = Operation.AdditionalData.FindRef(TEXT("ComponentData"));
			if (!ComponentDataJson.IsEmpty())
			{
				TSharedPtr<FJsonObject> ComponentData;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ComponentDataJson);
				if (FJsonSerializer::Deserialize(Reader, ComponentData))
				{
					return AddComponent(TargetBlueprint, ComponentData, OutError);
				}
			}
			OutError = TEXT("Invalid component data for AddComponent operation");
			return false;
		}

	case EMergeOperationType::RemoveComponent:
		return RemoveComponent(TargetBlueprint, Operation.TargetId, OutError);

	case EMergeOperationType::UpdateComponent:
		return UpdateComponent(TargetBlueprint, Operation.TargetId, Operation.PropertyName, Operation.NewValue, OutError);

	default:
		OutError = FString::Printf(TEXT("Unsupported operation type: %d"), (int32)Operation.OperationType);
		return false;
	}
}

bool FApplyEngine::ValidateBlueprintIntegrity(
	UBlueprint* Blueprint,
	TArray<FString>& OutValidationErrors)
{
	OutValidationErrors.Empty();

	if (!Blueprint)
	{
		OutValidationErrors.Add(TEXT("Blueprint is null"));
		return false;
	}

	// Check for duplicate variable names/GUIDs
	TSet<FString> VariableNames;
	TSet<FString> VariableGuids;

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		FString VarName = Variable.VarName.ToString();
		FString VarGuid = Variable.VarGuid.ToString();

		if (VariableNames.Contains(VarName))
		{
			OutValidationErrors.Add(FString::Printf(TEXT("Duplicate variable name: %s"), *VarName));
		}
		VariableNames.Add(VarName);

		if (!VarGuid.IsEmpty() && VariableGuids.Contains(VarGuid))
		{
			OutValidationErrors.Add(FString::Printf(TEXT("Duplicate variable GUID: %s"), *VarGuid));
		}
		VariableGuids.Add(VarGuid);
	}

	// Check graph integrity
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			OutValidationErrors.Add(TEXT("Found null graph in Blueprint"));
			continue;
		}

		// Check for orphaned nodes
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				OutValidationErrors.Add(FString::Printf(TEXT("Found null node in graph: %s"), *Graph->GetName()));
				continue;
			}

			// Check for broken pin connections
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					OutValidationErrors.Add(FString::Printf(TEXT("Found null pin in node: %s"), *Node->GetName()));
					continue;
				}

				// Check if linked pins still exist
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !IsValid(LinkedPin->GetOwningNode()))
					{
						OutValidationErrors.Add(FString::Printf(TEXT("Broken pin connection in node: %s, pin: %s"), 
							*Node->GetName(), *Pin->PinName.ToString()));
					}
				}
			}
		}
	}

	// Check component hierarchy integrity
	if (Blueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		TSet<FString> ComponentNames;

		for (USCS_Node* Node : AllNodes)
		{
			if (!Node)
			{
				OutValidationErrors.Add(TEXT("Found null component node"));
				continue;
			}

			FString CompName = Node->GetVariableName().ToString();
			if (ComponentNames.Contains(CompName))
			{
				OutValidationErrors.Add(FString::Printf(TEXT("Duplicate component name: %s"), *CompName));
			}
			ComponentNames.Add(CompName);
		}
	}

	return OutValidationErrors.Num() == 0;
}

bool FApplyEngine::CompileAndSaveBlueprint(
	UBlueprint* Blueprint,
	bool bForceCompile,
	FApplyResult& OutResult)
{
	if (!Blueprint)
	{
		OutResult.ErrorMessage = TEXT("Blueprint is null");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Compiling and saving Blueprint: %s"), *Blueprint->GetName());

	// Compile the Blueprint
	if (bForceCompile || FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint))
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);
		
		// Check for compilation errors
		if (Blueprint->Status == BS_Error)
		{
			OutResult.ErrorMessage = TEXT("Blueprint compilation failed with errors");
			OutResult.bBlueprintCompiled = false;
			UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Blueprint compilation failed"));
			return false;
		}
		else if (Blueprint->Status == BS_UpToDateWithWarnings)
		{
			OutResult.Warnings.Add(TEXT("Blueprint compiled with warnings"));
			UE_LOG(LogTemp, Warning, TEXT("ApplyEngine: Blueprint compiled with warnings"));
		}

		OutResult.bBlueprintCompiled = true;
	}

	// Save the Blueprint
	UPackage* Package = Blueprint->GetPackage();
	if (Package)
	{
		Package->SetDirtyFlag(true);
		
		// Use UEditorAssetSubsystem for proper saving
		if (UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>())
		{
			if (EditorAssetSubsystem->SaveAsset(Blueprint->GetPathName()))
			{
				OutResult.bBlueprintSaved = true;
				UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Blueprint saved successfully"));
			}
			else
			{
				OutResult.ErrorMessage = TEXT("Failed to save Blueprint");
				OutResult.bBlueprintSaved = false;
				UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Failed to save Blueprint"));
				return false;
			}
		}
		else
		{
			OutResult.ErrorMessage = TEXT("Could not get EditorAssetSubsystem");
			return false;
		}
	}
	else
	{
		OutResult.ErrorMessage = TEXT("Blueprint package is null");
		return false;
	}

	return true;
}

bool FApplyEngine::AddVariable(
	UBlueprint* Blueprint,
	TSharedPtr<FJsonObject> VariableData,
	FString& OutError)
{
	if (!Blueprint || !VariableData.IsValid())
	{
		OutError = TEXT("Invalid parameters for AddVariable");
		return false;
	}

	FString VarName = VariableData->GetStringField(TEXT("VariableName"));
	FString VarGuidStr = VariableData->GetStringField(TEXT("VariableGuid"));
	FString VarType = VariableData->GetStringField(TEXT("VarType"));

	if (VarName.IsEmpty())
	{
		OutError = TEXT("Variable name is empty");
		return false;
	}

	// Check if variable already exists
	for (const FBPVariableDescription& ExistingVar : Blueprint->NewVariables)
	{
		if (ExistingVar.VarName.ToString() == VarName)
		{
			OutError = FString::Printf(TEXT("Variable '%s' already exists"), *VarName);
			return false;
		}
	}

	// Create new variable description
	FBPVariableDescription NewVariable;
	NewVariable.VarName = FName(*VarName);
	
	// Parse GUID
	if (!VarGuidStr.IsEmpty())
	{
		FGuid::Parse(VarGuidStr, NewVariable.VarGuid);
	}
	if (!NewVariable.VarGuid.IsValid())
	{
		NewVariable.VarGuid = FGuid::NewGuid();
	}

	// Set variable type
	NewVariable.VarType.PinCategory = FName(*VarType);
	if (VarType == TEXT("int") || VarType == TEXT("Int"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (VarType == TEXT("float") || VarType == TEXT("Float"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (VarType == TEXT("bool") || VarType == TEXT("Boolean"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (VarType == TEXT("string") || VarType == TEXT("String"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_String;
	}

	// Set other properties
	NewVariable.Category = FText::FromString(VariableData->GetStringField(TEXT("Category")));
	NewVariable.FriendlyName = VariableData->GetStringField(TEXT("FriendlyName"));
	NewVariable.MetaDataArray[NewVariable.FindMetaDataEntryIndexForKey(TEXT("Tooltip"))].DataValue = VariableData->GetStringField(TEXT("Tooltip"));
	NewVariable.DefaultValue = VariableData->GetStringField(TEXT("DefaultValue"));

	// Set flags
	if (VariableData->GetBoolField(TEXT("bExposeOnSpawn")))
		NewVariable.PropertyFlags |= CPF_ExposeOnSpawn;
	if (VariableData->GetBoolField(TEXT("bBlueprintReadOnly")))
		NewVariable.PropertyFlags |= CPF_BlueprintReadOnly;
	if (VariableData->GetBoolField(TEXT("bInstanceEditable")))
		NewVariable.PropertyFlags |= CPF_Edit;
	if (VariableData->GetBoolField(TEXT("bBlueprintVisible")))
		NewVariable.PropertyFlags |= CPF_BlueprintVisible;

	// Add the variable
	FBlueprintEditorUtils::AddMemberVariable(Blueprint, NewVariable.VarName, NewVariable.VarType, NewVariable.DefaultValue);
	
	// Update the variable description with additional properties
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == NewVariable.VarName)
		{
			Var = NewVariable;
			break;
		}
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Added variable: %s (%s)"), *VarName, *VarType);
	return true;
}

bool FApplyEngine::RemoveVariable(
	UBlueprint* Blueprint,
	const FString& VariableGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FBPVariableDescription* Variable = FindVariableByGuid(Blueprint, VariableGuid);
	if (!Variable)
	{
		OutError = FString::Printf(TEXT("Variable with GUID %s not found"), *VariableGuid);
		return false;
	}

	FName VarName = Variable->VarName;

	// Remove the variable using Blueprint editor utilities
	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Removed variable: %s"), *VarName.ToString());
	return true;
}

bool FApplyEngine::UpdateVariable(
	UBlueprint* Blueprint,
	const FString& VariableGuid,
	const FString& PropertyName,
	const FString& NewValue,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FBPVariableDescription* Variable = FindVariableByGuid(Blueprint, VariableGuid);
	if (!Variable)
	{
		OutError = FString::Printf(TEXT("Variable with GUID %s not found"), *VariableGuid);
		return false;
	}

	// Update the specified property
	if (PropertyName == TEXT("DefaultValue"))
	{
		Variable->DefaultValue = NewValue;
	}
	else if (PropertyName == TEXT("Category"))
	{
		Variable->Category = FText::FromString(NewValue);
	}
	else if (PropertyName == TEXT("FriendlyName"))
	{
		Variable->FriendlyName = NewValue;
	}
	else if (PropertyName == TEXT("Tooltip"))
	{
		Variable->MetaDataArray[Variable->FindMetaDataEntryIndexForKey(TEXT("Tooltip"))].DataValue = NewValue;
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown variable property: %s"), *PropertyName);
		return false;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated variable %s property %s = %s"), 
		*Variable->VarName.ToString(), *PropertyName, *NewValue);
	return true;
}

bool FApplyEngine::RemapVariableGuid(
	UBlueprint* Blueprint,
	const FString& OldGuid,
	const FString& NewGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FGuid OldGuidParsed, NewGuidParsed;
	if (!FGuid::Parse(OldGuid, OldGuidParsed) || !FGuid::Parse(NewGuid, NewGuidParsed))
	{
		OutError = TEXT("Invalid GUID format for remapping");
		return false;
	}

	// Find and update the variable GUID
	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarGuid == OldGuidParsed)
		{
			Variable.VarGuid = NewGuidParsed;
			
			// Update all references to this variable
			if (!UpdateVariableReferences(Blueprint, OldGuid, NewGuid, OutError))
			{
				return false;
			}

			UE_LOG(LogTemp, VeryVerbose, TEXT("Remapped variable GUID: %s -> %s"), *OldGuid, *NewGuid);
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Variable with GUID %s not found for remapping"), *OldGuid);
	return false;
}

bool FApplyEngine::AddNode(
	UBlueprint* Blueprint,
	const FString& GraphName,
	TSharedPtr<FJsonObject> NodeData,
	FString& OutError)
{
	if (!Blueprint || !NodeData.IsValid())
	{
		OutError = TEXT("Invalid parameters for AddNode");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* NewNode = CreateNodeFromData(TargetGraph, NodeData, OutError);
	if (!NewNode)
	{
		return false;
	}

	TargetGraph->AddNode(NewNode, true);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Added node: %s to graph: %s"), *NewNode->GetName(), *GraphName);
	return true;
}

bool FApplyEngine::RemoveNode(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& NodeGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* NodeToRemove = FindNodeByGuid(TargetGraph, NodeGuid);
	if (!NodeToRemove)
	{
		OutError = FString::Printf(TEXT("Node with GUID %s not found in graph %s"), *NodeGuid, *GraphName);
		return false;
	}

	// Break all connections first
	for (UEdGraphPin* Pin : NodeToRemove->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}

	// Remove the node
	TargetGraph->RemoveNode(NodeToRemove);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Removed node: %s from graph: %s"), *NodeGuid, *GraphName);
	return true;
}

bool FApplyEngine::UpdateNodeProperty(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& NodeGuid,
	const FString& PropertyName,
	const FString& NewValue,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeGuid);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Node with GUID %s not found"), *NodeGuid);
		return false;
	}

	// Update specific properties based on node type and property name
	if (PropertyName == TEXT("NodeComment"))
	{
		TargetNode->NodeComment = NewValue;
	}
	else if (PropertyName == TEXT("NodeTitle"))
	{
		// Node title is usually read-only, but we can update the underlying data for some node types
		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(TargetNode))
		{
			CustomEvent->CustomFunctionName = FName(*NewValue);
			CustomEvent->ReconstructNode();
		}
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown node property: %s"), *PropertyName);
		return false;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated node %s property %s = %s"), *NodeGuid, *PropertyName, *NewValue);
	return true;
}

bool FApplyEngine::MoveNode(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& NodeGuid,
	float NewX,
	float NewY,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeGuid);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Node with GUID %s not found"), *NodeGuid);
		return false;
	}

	TargetNode->NodePosX = NewX;
	TargetNode->NodePosY = NewY;

	UE_LOG(LogTemp, VeryVerbose, TEXT("Moved node %s to (%.0f, %.0f)"), *NodeGuid, NewX, NewY);
	return true;
}

bool FApplyEngine::LinkPins(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& SourceNodeGuid,
	const FString& SourcePinName,
	const FString& TargetNodeGuid,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeByGuid(TargetGraph, SourceNodeGuid);
	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, TargetNodeGuid);

	if (!SourceNode || !TargetNode)
	{
		OutError = TEXT("Source or target node not found for pin linking");
		return false;
	}

	UEdGraphPin* SourcePin = FindPin(SourceNode, TEXT(""), SourcePinName);
	UEdGraphPin* TargetPin = FindPin(TargetNode, TEXT(""), TargetPinName);

	if (!SourcePin || !TargetPin)
	{
		OutError = FString::Printf(TEXT("Source pin '%s' or target pin '%s' not found"), *SourcePinName, *TargetPinName);
		return false;
	}

	// Check compatibility
	if (TargetGraph->GetSchema()->CanCreateConnection(SourcePin, TargetPin).Response != CONNECT_RESPONSE_MAKE)
	{
		OutError = FString::Printf(TEXT("Cannot connect pins '%s' to '%s' - incompatible types"), *SourcePinName, *TargetPinName);
		return false;
	}

	// Make the connection
	SourcePin->MakeLinkTo(TargetPin);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Linked pins: %s.%s -> %s.%s"), 
		*SourceNodeGuid, *SourcePinName, *TargetNodeGuid, *TargetPinName);
	return true;
}

bool FApplyEngine::UnlinkPins(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& SourceNodeGuid,
	const FString& SourcePinName,
	const FString& TargetNodeGuid,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeByGuid(TargetGraph, SourceNodeGuid);
	if (!SourceNode)
	{
		OutError = FString::Printf(TEXT("Source node %s not found"), *SourceNodeGuid);
		return false;
	}

	UEdGraphPin* SourcePin = FindPin(SourceNode, TEXT(""), SourcePinName);
	if (!SourcePin)
	{
		OutError = FString::Printf(TEXT("Source pin '%s' not found"), *SourcePinName);
		return false;
	}

	// Find the specific connection to break
	for (UEdGraphPin* LinkedPin : SourcePin->LinkedTo)
	{
		if (LinkedPin && FindPin(LinkedPin->GetOwningNode(), TEXT(""), TargetPinName) == LinkedPin)
		{
			SourcePin->BreakLinkTo(LinkedPin);
			UE_LOG(LogTemp, VeryVerbose, TEXT("Unlinked pins: %s.%s -> %s.%s"), 
				*SourceNodeGuid, *SourcePinName, *TargetNodeGuid, *TargetPinName);
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Connection between %s.%s and %s.%s not found"), 
		*SourceNodeGuid, *SourcePinName, *TargetNodeGuid, *TargetPinName);
	return false;
}

bool FApplyEngine::AddComponent(
	UBlueprint* Blueprint,
	TSharedPtr<FJsonObject> ComponentData,
	FString& OutError)
{
	if (!Blueprint || !ComponentData.IsValid())
	{
		OutError = TEXT("Invalid parameters for AddComponent");
		return false;
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("Blueprint has no SimpleConstructionScript");
		return false;
	}

	FString ComponentName = ComponentData->GetStringField(TEXT("ComponentName"));
	FString ComponentClassName = ComponentData->GetStringField(TEXT("ComponentClass"));

	if (ComponentName.IsEmpty() || ComponentClassName.IsEmpty())
	{
		OutError = TEXT("Component name or class is empty");
		return false;
	}

	// Find the component class
	UClass* ComponentClass = FindObject<UClass>(nullptr, *ComponentClassName);
	if (!ComponentClass)
	{
		// Try common component classes
		if (ComponentClassName.Contains(TEXT("StaticMesh")))
		{
			ComponentClass = UStaticMeshComponent::StaticClass();
		}
		else if (ComponentClassName.Contains(TEXT("Scene")))
		{
			ComponentClass = USceneComponent::StaticClass();
		}
		else
		{
			ComponentClass = UActorComponent::StaticClass(); // Fallback
		}
	}

	// Create the component node
	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*ComponentName));
	if (!NewNode)
	{
		OutError = FString::Printf(TEXT("Failed to create component node for %s"), *ComponentName);
		return false;
	}

	// Set attachment if specified
	FString AttachToComponent = ComponentData->GetStringField(TEXT("AttachToComponent"));
	if (!AttachToComponent.IsEmpty())
	{
		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* ExistingNode : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (ExistingNode && ExistingNode->GetVariableName().ToString() == AttachToComponent)
			{
				ParentNode = ExistingNode;
				break;
			}
		}

		if (ParentNode)
		{
			ParentNode->AddChildNode(NewNode);
		}
		else
		{
			// Attach to root if parent not found
			Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(NewNode);
		}
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Added component: %s (%s)"), *ComponentName, *ComponentClassName);
	return true;
}

bool FApplyEngine::RemoveComponent(
	UBlueprint* Blueprint,
	const FString& ComponentName,
	FString& OutError)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("Invalid Blueprint or SimpleConstructionScript");
		return false;
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		OutError = FString::Printf(TEXT("Component '%s' not found"), *ComponentName);
		return false;
	}

	// Remove the component
	Blueprint->SimpleConstructionScript->RemoveNode(ComponentNode);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Removed component: %s"), *ComponentName);
	return true;
}

bool FApplyEngine::UpdateComponent(
	UBlueprint* Blueprint,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& NewValue,
	FString& OutError)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("Invalid Blueprint or SimpleConstructionScript");
		return false;
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		OutError = FString::Printf(TEXT("Component '%s' not found"), *ComponentName);
		return false;
	}

	// Update properties based on property name
	if (PropertyName == TEXT("RelativeLocation"))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
		{
			FVector Location;
			if (Location.InitFromString(NewValue))
			{
				SceneComp->SetRelativeLocation(Location);
			}
		}
	}
	else if (PropertyName == TEXT("RelativeRotation"))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
		{
			FRotator Rotation;
			if (Rotation.InitFromString(NewValue))
			{
				SceneComp->SetRelativeRotation(Rotation);
			}
		}
	}
	else if (PropertyName == TEXT("RelativeScale"))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
		{
			FVector Scale;
			if (Scale.InitFromString(NewValue))
			{
				SceneComp->SetRelativeScale3D(Scale);
			}
		}
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated component %s property %s = %s"), *ComponentName, *PropertyName, *NewValue);
	return true;
}

UEdGraph* FApplyEngine::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	return nullptr;
}

UEdGraphNode* FApplyEngine::FindNodeByGuid(UEdGraph* Graph, const FString& NodeGuid)
{
	if (!Graph)
	{
		return nullptr;
	}

	FGuid TargetGuid;
	if (!FGuid::Parse(NodeGuid, TargetGuid))
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node* K2Node = Cast<UK2Node>(Node))
		{
			if (K2Node->NodeGuid == TargetGuid)
			{
				return Node;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* FApplyEngine::FindPin(UEdGraphNode* Node, const FString& PinId, const FString& PinName)
{
	if (!Node)
	{
		return nullptr;
	}

	// Try to find by PinId first
	if (!PinId.IsEmpty())
	{
		FGuid TargetPinId;
		if (FGuid::Parse(PinId, TargetPinId))
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->PinId == TargetPinId)
				{
					return Pin;
				}
			}
		}
	}

	// Fallback to name
	if (!PinName.IsEmpty())
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName.ToString() == PinName)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}

FBPVariableDescription* FApplyEngine::FindVariableByGuid(UBlueprint* Blueprint, const FString& VariableGuid)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	FGuid TargetGuid;
	if (!FGuid::Parse(VariableGuid, TargetGuid))
	{
		return nullptr;
	}

	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarGuid == TargetGuid)
		{
			return &Variable;
		}
	}

	return nullptr;
}

UEdGraphNode* FApplyEngine::CreateNodeFromData(
	UEdGraph* Graph,
	TSharedPtr<FJsonObject> NodeData,
	FString& OutError)
{
	if (!Graph || !NodeData.IsValid())
	{
		OutError = TEXT("Invalid parameters for CreateNodeFromData");
		return nullptr;
	}

	FString NodeClass = NodeData->GetStringField(TEXT("NodeClass"));
	if (NodeClass.IsEmpty())
	{
		OutError = TEXT("Node class is empty");
		return nullptr;
	}

	// Create node based on class
	UEdGraphNode* NewNode = nullptr;

	if (NodeClass.Contains(TEXT("K2Node_CallFunction")))
	{
		UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(Graph);
		
		// Set function reference if provided
		FString FunctionName = NodeData->GetStringField(TEXT("FunctionName"));
		if (!FunctionName.IsEmpty())
		{
			// Try to find the function
			UFunction* Function = nullptr;
			FString FunctionClass = NodeData->GetStringField(TEXT("FunctionClass"));
			
			if (!FunctionClass.IsEmpty())
			{
				UClass* OwnerClass = FindObject<UClass>(nullptr, *FunctionClass);
				if (OwnerClass)
				{
					Function = OwnerClass->FindFunctionByName(FName(*FunctionName));
				}
			}

			if (Function)
			{
				CallFunctionNode->SetFromFunction(Function);
			}
		}

		NewNode = CallFunctionNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_Event")))
	{
		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		
		FString EventName = NodeData->GetStringField(TEXT("EventName"));
		if (!EventName.IsEmpty())
		{
			EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
		}

		NewNode = EventNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_CustomEvent")))
	{
		UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(Graph);
		
		FString CustomEventName = NodeData->GetStringField(TEXT("CustomEventName"));
		if (!CustomEventName.IsEmpty())
		{
			CustomEventNode->CustomFunctionName = FName(*CustomEventName);
		}

		NewNode = CustomEventNode;
	}
	else
	{
		// Try to create a generic node
		UClass* NodeClassObj = FindObject<UClass>(nullptr, *NodeClass);
		if (NodeClassObj && NodeClassObj->IsChildOf<UEdGraphNode>())
		{
			NewNode = NewObject<UEdGraphNode>(Graph, NodeClassObj);
		}
		else
		{
			OutError = FString::Printf(TEXT("Unknown or invalid node class: %s"), *NodeClass);
			return nullptr;
		}
	}

	if (NewNode)
	{
		// Set position
		NewNode->NodePosX = NodeData->GetNumberField(TEXT("NodePosX"));
		NewNode->NodePosY = NodeData->GetNumberField(TEXT("NodePosY"));

		// Set GUID if provided
		FString NodeGuidStr = NodeData->GetStringField(TEXT("NodeGuid"));
		if (!NodeGuidStr.IsEmpty())
		{
			if (UK2Node* K2Node = Cast<UK2Node>(NewNode))
			{
				FGuid NodeGuid;
				if (FGuid::Parse(NodeGuidStr, NodeGuid))
				{
					K2Node->NodeGuid = NodeGuid;
				}
			}
		}

		// Allocate default pins
		NewNode->AllocateDefaultPins();

		UE_LOG(LogTemp, VeryVerbose, TEXT("Created node: %s (%s)"), *NewNode->GetName(), *NodeClass);
	}

	return NewNode;
}

bool FApplyEngine::UpdateVariableReferences(
	UBlueprint* Blueprint,
	const FString& OldGuid,
	const FString& NewGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FGuid OldGuidParsed, NewGuidParsed;
	if (!FGuid::Parse(OldGuid, OldGuidParsed) || !FGuid::Parse(NewGuid, NewGuidParsed))
	{
		OutError = TEXT("Invalid GUID format");
		return false;
	}

	int32 UpdateCount = 0;

	// Update variable references across known graph containers (UE5)
	auto UpdateGraphNodes = [&](UEdGraph* Graph)
	{
		if (!Graph) { return; }
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
			{
				if (VarGetNode->VariableReference.GetMemberGuid() == OldGuidParsed)
				{
					VarGetNode->VariableReference.SetExternalMember(
						VarGetNode->VariableReference.GetMemberName(),
						VarGetNode->VariableReference.GetMemberParentClass(),
						NewGuidParsed
					);
					VarGetNode->ReconstructNode();
					UpdateCount++;
				}
			}
			else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
			{
				if (VarSetNode->VariableReference.GetMemberGuid() == OldGuidParsed)
				{
					VarSetNode->VariableReference.SetExternalMember(
						VarSetNode->VariableReference.GetMemberName(),
						VarSetNode->VariableReference.GetMemberParentClass(),
						NewGuidParsed
					);
					VarSetNode->ReconstructNode();
					UpdateCount++;
				}
			}
		}
	};

	for (UEdGraph* Graph : Blueprint->UbergraphPages) { UpdateGraphNodes(Graph); }
	for (UEdGraph* Graph : Blueprint->FunctionGraphs) { UpdateGraphNodes(Graph); }
	for (UEdGraph* Graph : Blueprint->MacroGraphs) { UpdateGraphNodes(Graph); }

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated %d variable references for GUID remap: %s -> %s"), 
		UpdateCount, *OldGuid, *NewGuid);
	return true;
}

bool FApplyEngine::EnsureGameThread(TFunction<bool()> Operation)
{
	if (IsInGameThread())
	{
		return Operation();
	}
	else
	{
		// Queue operation on game thread
		bool bResult = false;
		AsyncTask(ENamedThreads::GameThread, [&Operation, &bResult]()
		{
			bResult = Operation();
		});
		return bResult;
	}
}

TSharedPtr<FScopedTransaction> FApplyEngine::CreateUndoTransaction(const FString& Description)
{
	return MakeShareable(new FScopedTransaction(FText::FromString(Description)));
}

void FApplyEngine::LogOperationResult(
	EMergeOperationType OperationType,
	const FString& TargetName,
	bool bSuccess,
	const FString& ErrorMessage)
{
	FString OpTypeName = StaticEnum<EMergeOperationType>()->GetValueAsString(OperationType);
	
	if (bSuccess)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("✅ %s on %s succeeded"), *OpTypeName, *TargetName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ %s on %s failed: %s"), *OpTypeName, *TargetName, *ErrorMessage);
	}
}

bool FApplyEngine::IsBlueprintBeingEdited(UBlueprint* Blueprint)
{
	if (!Blueprint || !GEditor)
	{
		return false;
	}

	// Conservative fallback: avoid calling removed APIs; consider the BP as editable/open state unknown
	return false;
}

void FApplyEngine::RefreshBlueprintEditor(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return;
	}

	// Use standard refresh/compile path compatible with UE5
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}
