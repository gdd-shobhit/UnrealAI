#include "../Public/DiffEngine.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

bool FDiffEngine::PerformThreeWayDiff(
	TSharedPtr<FJsonObject> BaseSnapshot,
	TSharedPtr<FJsonObject> LocalSnapshot,
	TSharedPtr<FJsonObject> RemoteSnapshot,
	FDiffResult& OutResult)
{
	if (!BaseSnapshot.IsValid() || !LocalSnapshot.IsValid() || !RemoteSnapshot.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("DiffEngine: Invalid snapshots provided for three-way diff"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("DiffEngine: Starting three-way diff"));

	// Clear previous results
	OutResult.Operations.Empty();
	OutResult.Conflicts.Empty();
	OutResult.bHasConflicts = false;

	// Diff variables
	const TArray<TSharedPtr<FJsonValue>>* BaseVars = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* LocalVars = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* RemoteVars = nullptr;

	BaseSnapshot->TryGetArrayField(TEXT("Variables"), BaseVars);
	LocalSnapshot->TryGetArrayField(TEXT("Variables"), LocalVars);
	RemoteSnapshot->TryGetArrayField(TEXT("Variables"), RemoteVars);

	if (BaseVars && LocalVars && RemoteVars)
	{
		DiffVariables(*BaseVars, *LocalVars, *RemoteVars, OutResult.Operations, OutResult.Conflicts);
	}

	// Diff graphs
	const TArray<TSharedPtr<FJsonValue>>* BaseGraphs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* LocalGraphs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* RemoteGraphs = nullptr;

	BaseSnapshot->TryGetArrayField(TEXT("Graphs"), BaseGraphs);
	LocalSnapshot->TryGetArrayField(TEXT("Graphs"), LocalGraphs);
	RemoteSnapshot->TryGetArrayField(TEXT("Graphs"), RemoteGraphs);

	if (BaseGraphs && LocalGraphs && RemoteGraphs)
	{
		DiffGraphs(*BaseGraphs, *LocalGraphs, *RemoteGraphs, OutResult.Operations, OutResult.Conflicts);
	}

	// Diff components
	const TArray<TSharedPtr<FJsonValue>>* BaseComponents = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* LocalComponents = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* RemoteComponents = nullptr;

	BaseSnapshot->TryGetArrayField(TEXT("Components"), BaseComponents);
	LocalSnapshot->TryGetArrayField(TEXT("Components"), LocalComponents);
	RemoteSnapshot->TryGetArrayField(TEXT("Components"), RemoteComponents);

	if (BaseComponents && LocalComponents && RemoteComponents)
	{
		DiffComponents(*BaseComponents, *LocalComponents, *RemoteComponents, OutResult.Operations, OutResult.Conflicts);
	}

	// Update conflict status
	OutResult.bHasConflicts = OutResult.Conflicts.Num() > 0;

	// Generate summary
	OutResult.DiffSummary = GenerateDiffSummary(OutResult);

	UE_LOG(LogTemp, Log, TEXT("DiffEngine: Three-way diff completed. Operations: %d, Conflicts: %d"), 
		OutResult.Operations.Num(), OutResult.Conflicts.Num());
	
	// Log detailed operation information
	for (int32 i = 0; i < OutResult.Operations.Num(); i++)
	{
		const FMergeOperation& Op = OutResult.Operations[i];
		FString OpTypeName = TEXT("Unknown");
		switch (Op.OperationType)
		{
		case EMergeOperationType::AddNode: OpTypeName = TEXT("AddNode"); break;
		case EMergeOperationType::RemoveNode: OpTypeName = TEXT("RemoveNode"); break;
		case EMergeOperationType::UpdateNodeProperty: OpTypeName = TEXT("UpdateNodeProperty"); break;
		case EMergeOperationType::AddVariable: OpTypeName = TEXT("AddVariable"); break;
		case EMergeOperationType::RemoveVariable: OpTypeName = TEXT("RemoveVariable"); break;
		case EMergeOperationType::UpdateVariable: OpTypeName = TEXT("UpdateVariable"); break;
		case EMergeOperationType::LinkPins: OpTypeName = TEXT("LinkPins"); break;
		case EMergeOperationType::UnlinkPins: OpTypeName = TEXT("UnlinkPins"); break;
		case EMergeOperationType::AddComponent: OpTypeName = TEXT("AddComponent"); break;
		case EMergeOperationType::RemoveComponent: OpTypeName = TEXT("RemoveComponent"); break;
		case EMergeOperationType::UpdateComponent: OpTypeName = TEXT("UpdateComponent"); break;
		case EMergeOperationType::AddGraph: OpTypeName = TEXT("AddGraph"); break;
		case EMergeOperationType::RemoveGraph: OpTypeName = TEXT("RemoveGraph"); break;
		}
		
		// Get data from AdditionalData based on operation type
		FString DataKey = TEXT("");
		if (Op.OperationType == EMergeOperationType::AddVariable)
		{
			DataKey = TEXT("VariableData");
		}
		else if (Op.OperationType == EMergeOperationType::AddNode)
		{
			DataKey = TEXT("NodeData");
		}
		else if (Op.OperationType == EMergeOperationType::AddGraph)
		{
			DataKey = TEXT("GraphData");
		}
		
		FString OperationData = DataKey.IsEmpty() ? TEXT("") : Op.AdditionalData.FindRef(DataKey);
		int32 DataLength = OperationData.Len();
		
		UE_LOG(LogTemp, Log, TEXT("DiffEngine: Operation %d - Type: %s, TargetId: %s, TargetGraph: %s, DataLength: %d"), 
			i, *OpTypeName, *Op.TargetId, *Op.TargetGraph, DataLength);
		
		// Log first 200 characters of data for debugging
		if (DataLength > 0)
		{
			FString DataPreview = DataLength > 200 ? OperationData.Left(200) + TEXT("...") : OperationData;
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Operation %d Data Preview: %s"), i, *DataPreview);
		}
	}

	return true;
}

bool FDiffEngine::PerformTwoWayDiff(
	TSharedPtr<FJsonObject> SourceSnapshot,
	TSharedPtr<FJsonObject> TargetSnapshot,
	FDiffResult& OutResult)
{
	if (!SourceSnapshot.IsValid() || !TargetSnapshot.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("DiffEngine: Invalid snapshots provided for two-way diff"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("DiffEngine: Starting two-way diff"));

	// Clear previous results
	OutResult.Operations.Empty();
	OutResult.Conflicts.Empty();
	OutResult.bHasConflicts = false;

	// For two-way diff, we treat source as base and target as both local and remote
	// This means we generate operations but no conflicts
	TArray<TSharedPtr<FJsonValue>> EmptyArray;

	// Diff variables
	const TArray<TSharedPtr<FJsonValue>>* SourceVars = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* TargetVars = nullptr;

	SourceSnapshot->TryGetArrayField(TEXT("Variables"), SourceVars);
	TargetSnapshot->TryGetArrayField(TEXT("Variables"), TargetVars);

	if (SourceVars && TargetVars)
	{
		DiffVariables(*SourceVars, *TargetVars, *TargetVars, OutResult.Operations, OutResult.Conflicts);
	}

	// Diff graphs
	const TArray<TSharedPtr<FJsonValue>>* SourceGraphs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* TargetGraphs = nullptr;

	SourceSnapshot->TryGetArrayField(TEXT("Graphs"), SourceGraphs);
	TargetSnapshot->TryGetArrayField(TEXT("Graphs"), TargetGraphs);

	if (SourceGraphs && TargetGraphs)
	{
		DiffGraphs(*SourceGraphs, *TargetGraphs, *TargetGraphs, OutResult.Operations, OutResult.Conflicts);
	}

	// Diff components
	const TArray<TSharedPtr<FJsonValue>>* SourceComponents = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* TargetComponents = nullptr;

	SourceSnapshot->TryGetArrayField(TEXT("Components"), SourceComponents);
	TargetSnapshot->TryGetArrayField(TEXT("Components"), TargetComponents);

	if (SourceComponents && TargetComponents)
	{
		DiffComponents(*SourceComponents, *TargetComponents, *TargetComponents, OutResult.Operations, OutResult.Conflicts);
	}

	// Generate summary
	OutResult.DiffSummary = GenerateDiffSummary(OutResult);

	UE_LOG(LogTemp, Log, TEXT("DiffEngine: Two-way diff completed. Operations: %d"), OutResult.Operations.Num());

	return true;
}

bool FDiffEngine::CompareJsonValues(
	TSharedPtr<FJsonValue> ValueA,
	TSharedPtr<FJsonValue> ValueB,
	TArray<FString>& OutDifferingFields)
{
	if (!ValueA.IsValid() && !ValueB.IsValid())
	{
		return false; // Both null, no difference
	}

	if (!ValueA.IsValid() || !ValueB.IsValid())
	{
		OutDifferingFields.Add(TEXT("Existence")); // One is null, the other isn't
		return true;
	}

	if (ValueA->Type != ValueB->Type)
	{
		OutDifferingFields.Add(TEXT("Type"));
		return true;
	}

	switch (ValueA->Type)
	{
	case EJson::String:
		if (ValueA->AsString() != ValueB->AsString())
		{
			OutDifferingFields.Add(TEXT("StringValue"));
			return true;
		}
		break;

	case EJson::Number:
		if (ValueA->AsNumber() != ValueB->AsNumber())
		{
			OutDifferingFields.Add(TEXT("NumberValue"));
			return true;
		}
		break;

	case EJson::Boolean:
		if (ValueA->AsBool() != ValueB->AsBool())
		{
			OutDifferingFields.Add(TEXT("BooleanValue"));
			return true;
		}
		break;

	case EJson::Object:
		return CompareJsonObjects(ValueA->AsObject(), ValueB->AsObject(), OutDifferingFields);

	case EJson::Array:
		return CompareJsonArrays(ValueA->AsArray(), ValueB->AsArray(), OutDifferingFields);

	case EJson::Null:
		// Both are null, no difference
		break;

	default:
		OutDifferingFields.Add(TEXT("UnknownType"));
		return true;
	}

	return false;
}

EConflictSeverity FDiffEngine::AnalyzeConflictSeverity(
	const FString& ConflictType,
	const TArray<FString>& DifferingFields)
{
	// Critical conflicts - fundamental structural changes
	if (ConflictType == TEXT("Variable") && 
		(DifferingFields.Contains(TEXT("VarType")) || DifferingFields.Contains(TEXT("VariableGuid"))))
	{
		return EConflictSeverity::Critical;
	}

	if (ConflictType == TEXT("Node") && 
		(DifferingFields.Contains(TEXT("NodeClass")) || DifferingFields.Contains(TEXT("FunctionName"))))
	{
		return EConflictSeverity::Critical;
	}

	// High conflicts - significant changes that might break things
	if (ConflictType == TEXT("Connection") || ConflictType == TEXT("Component"))
	{
		return EConflictSeverity::High;
	}

	if (DifferingFields.Contains(TEXT("DefaultValue")) || DifferingFields.Contains(TEXT("PropertyValue")))
	{
		return EConflictSeverity::High;
	}

	// Medium conflicts - property changes
	if (DifferingFields.Contains(TEXT("Category")) || DifferingFields.Contains(TEXT("FriendlyName")) ||
		DifferingFields.Contains(TEXT("VarTooltip")) || DifferingFields.Contains(TEXT("NodeTitle")))
	{
		return EConflictSeverity::Medium;
	}

	// Low conflicts - cosmetic changes
	if (DifferingFields.Contains(TEXT("NodePosX")) || DifferingFields.Contains(TEXT("NodePosY")) ||
		DifferingFields.Contains(TEXT("RelativeLocation")) || DifferingFields.Contains(TEXT("RelativeRotation")))
	{
		return EConflictSeverity::Low;
	}

	return EConflictSeverity::Medium; // Default
}

void FDiffEngine::DiffVariables(
	const TArray<TSharedPtr<FJsonValue>>& BaseVars,
	const TArray<TSharedPtr<FJsonValue>>& LocalVars,
	const TArray<TSharedPtr<FJsonValue>>& RemoteVars,
	TArray<FMergeOperation>& OutOperations,
	TArray<FMergeConflict>& OutConflicts)
{
	// NOTE: This function groups variables by NAME, not GUID, to handle the case where
	// the same variable exists in different Blueprint instances with different GUIDs.
	// 
	// In a proper Perforce/Git integration:
	// - Same variables should have same GUIDs across all versions
	// Matching strategy is controlled by BPT_MERGE_USE_GUID_MATCHING macro:
	// - When 0: Match by VariableName (preferred for testing without VCS)
	// - When 1: Match by VariableGuid (preferred when Perforce/Git ensures stable GUIDs)
	
	// Build lookup maps by variable name or GUID based on macro setting
	TMap<FString, TSharedPtr<FJsonObject>> BaseVarMap, LocalVarMap, RemoteVarMap;
#if BPT_MERGE_USE_GUID_MATCHING
	BuildLookupMap(BaseVars, TEXT("VariableGuid"), BaseVarMap);
	BuildLookupMap(LocalVars, TEXT("VariableGuid"), LocalVarMap);
	BuildLookupMap(RemoteVars, TEXT("VariableGuid"), RemoteVarMap);
#else
	BuildLookupMap(BaseVars, TEXT("VariableName"), BaseVarMap);
	BuildLookupMap(LocalVars, TEXT("VariableName"), LocalVarMap);
	BuildLookupMap(RemoteVars, TEXT("VariableName"), RemoteVarMap);
#endif

	// Get union of all variable identifiers (name or GUID depending on macro)
	TSet<FString> AllVarIds;
	BaseVarMap.GetKeys(AllVarIds);
	LocalVarMap.GetKeys(AllVarIds);
	RemoteVarMap.GetKeys(AllVarIds);

	for (const FString& VarId : AllVarIds)
	{
		TSharedPtr<FJsonObject> BaseVar = BaseVarMap.FindRef(VarId);
		TSharedPtr<FJsonObject> LocalVar = LocalVarMap.FindRef(VarId);
		TSharedPtr<FJsonObject> RemoteVar = RemoteVarMap.FindRef(VarId);
		
		// Get variable name for logging/display purposes
		FString VarName = VarId;
		if (LocalVar.IsValid())
		{
			VarName = LocalVar->GetStringField(TEXT("VariableName"));
		}
		else if (RemoteVar.IsValid())
		{
			VarName = RemoteVar->GetStringField(TEXT("VariableName"));
		}
		else if (BaseVar.IsValid())
		{
			VarName = BaseVar->GetStringField(TEXT("VariableName"));
		}

		// Variable added in local only
		if (!BaseVar.IsValid() && LocalVar.IsValid() && !RemoteVar.IsValid())
		{
			FString LocalVarGuid = LocalVar->GetStringField(TEXT("VariableGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddVariable, TEXT(""), LocalVarGuid);
			// Store the actual variable data for the Apply Engine
			FString VariableDataString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&VariableDataString);
			FJsonSerializer::Serialize(LocalVar.ToSharedRef(), Writer);
			Op.AdditionalData.Add(TEXT("VariableData"), VariableDataString);
			
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: AddVariable (Local) - Name: %s, GUID: %s"), *VarName, *LocalVarGuid);
			OutOperations.Add(Op);
		}
		// Variable added in remote only
		else if (!BaseVar.IsValid() && !LocalVar.IsValid() && RemoteVar.IsValid())
		{
			FString RemoteVarGuid = RemoteVar->GetStringField(TEXT("VariableGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddVariable, TEXT(""), RemoteVarGuid);
			// Store the actual variable data for the Apply Engine
			FString VariableDataString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&VariableDataString);
			FJsonSerializer::Serialize(RemoteVar.ToSharedRef(), Writer);
			Op.AdditionalData.Add(TEXT("VariableData"), VariableDataString);
			
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: AddVariable (Remote) - Name: %s, GUID: %s"), *VarName, *RemoteVarGuid);
			OutOperations.Add(Op);
		}
		// Variable added in both (conflict)
		else if (!BaseVar.IsValid() && LocalVar.IsValid() && RemoteVar.IsValid())
		{
			FString LocalVarGuid = LocalVar->GetStringField(TEXT("VariableGuid"));
			FString RemoteVarGuid = RemoteVar->GetStringField(TEXT("VariableGuid"));
			
			// Check if GUIDs are identical (should be true with proper Perforce/Git integration)
			bool bSameGuid = AreVariableGuidsIdentical(LocalVar, RemoteVar);
			
			TArray<FString> DifferingFields;
			bool bDifferentContent = CompareVariablesIgnoringGuid(LocalVar, RemoteVar, DifferingFields);
			
			// Debug logging to see what's being compared
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Comparing variable '%s' - Local GUID: %s, Remote GUID: %s"), 
				*VarName, *LocalVarGuid, *RemoteVarGuid);
			
			// Log the actual variable data for debugging
			FString LocalVarData, RemoteVarData;
			TSharedRef<TJsonWriter<>> LocalWriter = TJsonWriterFactory<>::Create(&LocalVarData);
			TSharedRef<TJsonWriter<>> RemoteWriter = TJsonWriterFactory<>::Create(&RemoteVarData);
			FJsonSerializer::Serialize(LocalVar.ToSharedRef(), LocalWriter);
			FJsonSerializer::Serialize(RemoteVar.ToSharedRef(), RemoteWriter);
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Local variable data: %s"), *LocalVarData);
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Remote variable data: %s"), *RemoteVarData);
			
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Content comparison result (ignoring GUID): %s, Differing fields: %s"), 
				bDifferentContent ? TEXT("DIFFERENT") : TEXT("IDENTICAL"),
				*FString::Join(DifferingFields, TEXT(", ")));
			
			if (bDifferentContent)
			{
				// Variables have same name but different properties - this is a conflict
				UE_LOG(LogTemp, Warning, TEXT("DiffEngine: Variable '%s' added in both but differs in: %s"), 
					*VarName, *FString::Join(DifferingFields, TEXT(", ")));
				
				FMergeConflict Conflict = CreateConflict(
					TEXT("Variable"),
					VarName,
					TEXT("(not present)"),
					LocalVar->GetStringField(TEXT("VariableName")),
					RemoteVar->GetStringField(TEXT("VariableName")),
					DifferingFields
				);
				Conflict.Severity = AnalyzeConflictSeverity(TEXT("Variable"), DifferingFields);
				OutConflicts.Add(Conflict);
			}
			else if (!bSameGuid)
			{
				// Same content but different GUIDs - this shouldn't happen with proper version control
				// For now, we'll treat it as identical and skip the operation
				UE_LOG(LogTemp, Warning, TEXT("DiffEngine: Variable '%s' has same content but different GUIDs (Local: %s, Remote: %s) - treating as identical"), 
					*VarName, *LocalVarGuid, *RemoteVarGuid);
				UE_LOG(LogTemp, Log, TEXT("DiffEngine: Variable '%s' added identically in both - skipping operation (same name, type, flags, default value)"), *VarName);
			}
			else
			{
				// Variables are completely identical (name, type, flags, default value, GUID) - no operation needed
				UE_LOG(LogTemp, Log, TEXT("DiffEngine: Variable '%s' added identically in both - skipping operation (same name, type, flags, default value, GUID)"), *VarName);
			}
		}
		// Variable removed in local only
		else if (BaseVar.IsValid() && !LocalVar.IsValid() && RemoteVar.IsValid())
		{
			FString BaseVarGuid = BaseVar->GetStringField(TEXT("VariableGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::RemoveVariable, TEXT(""), BaseVarGuid);
			Op.AdditionalData.Add(TEXT("Reason"), TEXT("LocalRemoval"));
			OutOperations.Add(Op);
		}
		// Variable removed in remote only
		else if (BaseVar.IsValid() && LocalVar.IsValid() && !RemoteVar.IsValid())
		{
			FString BaseVarGuid = BaseVar->GetStringField(TEXT("VariableGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::RemoveVariable, TEXT(""), BaseVarGuid);
			Op.AdditionalData.Add(TEXT("Reason"), TEXT("RemoteRemoval"));
			OutOperations.Add(Op);
		}
		// Variable removed in both
		else if (BaseVar.IsValid() && !LocalVar.IsValid() && !RemoteVar.IsValid())
		{
			FString BaseVarGuid = BaseVar->GetStringField(TEXT("VariableGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::RemoveVariable, TEXT(""), BaseVarGuid);
			Op.AdditionalData.Add(TEXT("Reason"), TEXT("BothRemoval"));
			OutOperations.Add(Op);
		}
		// Variable exists in all three - check for modifications
		else if (BaseVar.IsValid() && LocalVar.IsValid() && RemoteVar.IsValid())
		{
			TArray<FString> LocalDiffs, RemoteDiffs;
			bool bLocalChanged = CompareJsonObjects(BaseVar, LocalVar, LocalDiffs);
			bool bRemoteChanged = CompareJsonObjects(BaseVar, RemoteVar, RemoteDiffs);

			if (bLocalChanged && bRemoteChanged)
			{
				// Both changed - potential conflict
				TArray<FString> LocalRemoteDiffs;
				if (CompareJsonObjects(LocalVar, RemoteVar, LocalRemoteDiffs))
				{
					// Different changes - conflict
					FMergeConflict Conflict = CreateConflict(
						TEXT("Variable"),
						VarName,
						BaseVar->GetStringField(TEXT("VariableName")),
						LocalVar->GetStringField(TEXT("VariableName")),
						RemoteVar->GetStringField(TEXT("VariableName")),
						LocalRemoteDiffs
					);
					Conflict.Severity = AnalyzeConflictSeverity(TEXT("Variable"), LocalRemoteDiffs);
					OutConflicts.Add(Conflict);
				}
				else
				{
					// Same changes - no conflict needed
					FString BaseVarGuid = BaseVar->GetStringField(TEXT("VariableGuid"));
					FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), BaseVarGuid);
					OutOperations.Add(Op);
				}
			}
			else if (bLocalChanged)
			{
				// Only local changed
				FString BaseVarGuid = BaseVar->GetStringField(TEXT("VariableGuid"));
				FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), BaseVarGuid);
				Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
				OutOperations.Add(Op);
			}
			else if (bRemoteChanged)
			{
				// Only remote changed
				FString BaseVarGuid = BaseVar->GetStringField(TEXT("VariableGuid"));
				FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), BaseVarGuid);
				Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
				OutOperations.Add(Op);
			}
			// If neither changed, no operation needed
		}
	}
}

void FDiffEngine::DiffGraphs(
	const TArray<TSharedPtr<FJsonValue>>& BaseGraphs,
	const TArray<TSharedPtr<FJsonValue>>& LocalGraphs,
	const TArray<TSharedPtr<FJsonValue>>& RemoteGraphs,
	TArray<FMergeOperation>& OutOperations,
	TArray<FMergeConflict>& OutConflicts)
{
	// Build lookup maps by a stable graph key (FunctionName if present, else GraphName)
	TMap<FString, TSharedPtr<FJsonObject>> BaseGraphMap, LocalGraphMap, RemoteGraphMap;
	auto BuildGraphMap = [](const TArray<TSharedPtr<FJsonValue>>& Graphs, TMap<FString, TSharedPtr<FJsonObject>>& OutMap)
	{
		for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
		{
			if (!GraphValue.IsValid() || GraphValue->Type != EJson::Object)
			{
				continue;
			}
			TSharedPtr<FJsonObject> GraphObj = GraphValue->AsObject();
			FString Key;
			FString GraphType = GraphObj->GetStringField(TEXT("GraphType"));
			if (GraphType == TEXT("Function"))
			{
				const TSharedPtr<FJsonValue>* FnField = GraphObj->Values.Find(TEXT("FunctionName"));
				if (FnField && (*FnField)->Type == EJson::String)
				{
					Key = (*FnField)->AsString();
				}
			}
			if (Key.IsEmpty())
			{
				Key = GraphObj->GetStringField(TEXT("GraphName"));
			}
			OutMap.Add(Key, GraphObj);
		}
	};

	BuildGraphMap(BaseGraphs, BaseGraphMap);
	BuildGraphMap(LocalGraphs, LocalGraphMap);
	BuildGraphMap(RemoteGraphs, RemoteGraphMap);

	// Get union of all graph names
	TSet<FString> AllGraphNames;
	BaseGraphMap.GetKeys(AllGraphNames);
	LocalGraphMap.GetKeys(AllGraphNames);
	RemoteGraphMap.GetKeys(AllGraphNames);

	for (const FString& GraphName : AllGraphNames)
	{
		TSharedPtr<FJsonObject> BaseGraph = BaseGraphMap.FindRef(GraphName);
		TSharedPtr<FJsonObject> LocalGraph = LocalGraphMap.FindRef(GraphName);
		TSharedPtr<FJsonObject> RemoteGraph = RemoteGraphMap.FindRef(GraphName);

		// Handle graph-level changes (addition/removal)
		if (!BaseGraph.IsValid() && LocalGraph.IsValid() && RemoteGraph.IsValid())
		{
			// Graph added in both - check if they're the same
			TArray<FString> DifferingFields;
			if (CompareJsonObjects(LocalGraph, RemoteGraph, DifferingFields))
			{
				// Graphs have same name but different properties - this is a conflict
				UE_LOG(LogTemp, Warning, TEXT("DiffEngine: Graph '%s' added in both but differs in: %s"), 
					*GraphName, *FString::Join(DifferingFields, TEXT(", ")));
				
				// Serialize the actual graph data for "Keep Both" strategy
				FString LocalGraphData, RemoteGraphData;
				TSharedRef<TJsonWriter<>> LocalWriter = TJsonWriterFactory<>::Create(&LocalGraphData);
				TSharedRef<TJsonWriter<>> RemoteWriter = TJsonWriterFactory<>::Create(&RemoteGraphData);
				FJsonSerializer::Serialize(LocalGraph.ToSharedRef(), LocalWriter);
				FJsonSerializer::Serialize(RemoteGraph.ToSharedRef(), RemoteWriter);
				
				FMergeConflict Conflict = CreateConflict(
					TEXT("Graph"),
					GraphName,
					TEXT("(not present)"),
					TEXT("Added locally"),
					TEXT("Added remotely"),
					DifferingFields,
					LocalGraphData,
					RemoteGraphData
				);
				OutConflicts.Add(Conflict);
			}
			else
			{
				// Graphs are completely identical - no operation needed
				UE_LOG(LogTemp, Log, TEXT("DiffEngine: Graph '%s' added identically in both - skipping operation"), *GraphName);
			}
		}
		else if (!BaseGraph.IsValid() && LocalGraph.IsValid())
		{
			// Graph added locally only
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddGraph, GraphName, TEXT(""));
			// Attach full graph payload for one-shot creation
			FString GraphDataString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&GraphDataString);
			FJsonSerializer::Serialize(LocalGraph.ToSharedRef(), Writer);
			Op.AdditionalData.Add(TEXT("GraphData"), GraphDataString);
			OutOperations.Add(Op);
		}
		else if (!BaseGraph.IsValid() && RemoteGraph.IsValid())
		{
			// Graph added remotely only
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Function '%s' exists only in remote - creating AddGraph operation"), *GraphName);
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddGraph, GraphName, TEXT(""));
			// Attach full graph payload for one-shot creation
			FString GraphDataString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&GraphDataString);
			FJsonSerializer::Serialize(RemoteGraph.ToSharedRef(), Writer);
			Op.AdditionalData.Add(TEXT("GraphData"), GraphDataString);
			OutOperations.Add(Op);
		}
		else if (BaseGraph.IsValid() && !LocalGraph.IsValid() && !RemoteGraph.IsValid())
		{
			// Graph removed in both
			FMergeOperation Op = CreateOperation(EMergeOperationType::RemoveGraph, GraphName, TEXT(""));
			OutOperations.Add(Op);
		}
		else if (BaseGraph.IsValid() && LocalGraph.IsValid() && RemoteGraph.IsValid())
		{
			// Graph exists in all three - diff the contents
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: Function '%s' exists in all three versions - diffing contents"), *GraphName);

			// Diff nodes
			const TArray<TSharedPtr<FJsonValue>>* BaseNodes = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* LocalNodes = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* RemoteNodes = nullptr;

			BaseGraph->TryGetArrayField(TEXT("Nodes"), BaseNodes);
			LocalGraph->TryGetArrayField(TEXT("Nodes"), LocalNodes);
			RemoteGraph->TryGetArrayField(TEXT("Nodes"), RemoteNodes);

			if (BaseNodes && LocalNodes && RemoteNodes)
			{
				DiffNodes(GraphName, *BaseNodes, *LocalNodes, *RemoteNodes, OutOperations, OutConflicts);
			}

			// Diff connections
			const TArray<TSharedPtr<FJsonValue>>* BaseConnections = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* LocalConnections = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* RemoteConnections = nullptr;

			BaseGraph->TryGetArrayField(TEXT("Connections"), BaseConnections);
			LocalGraph->TryGetArrayField(TEXT("Connections"), LocalConnections);
			RemoteGraph->TryGetArrayField(TEXT("Connections"), RemoteConnections);

			if (BaseConnections && LocalConnections && RemoteConnections)
			{
				DiffConnections(GraphName, *BaseConnections, *LocalConnections, *RemoteConnections, OutOperations, OutConflicts);
			}
		}
	}
}

void FDiffEngine::DiffNodes(
	const FString& GraphName,
	const TArray<TSharedPtr<FJsonValue>>& BaseNodes,
	const TArray<TSharedPtr<FJsonValue>>& LocalNodes,
	const TArray<TSharedPtr<FJsonValue>>& RemoteNodes,
	TArray<FMergeOperation>& OutOperations,
	TArray<FMergeConflict>& OutConflicts)
{
	// Build lookup maps by NodeGuid or semantic key based on toggle
	TMap<FString, TSharedPtr<FJsonObject>> BaseNodeMap, LocalNodeMap, RemoteNodeMap;
#if BPT_MERGE_USE_GUID_MATCHING
	BuildLookupMap(BaseNodes, TEXT("NodeGuid"), BaseNodeMap);
	BuildLookupMap(LocalNodes, TEXT("NodeGuid"), LocalNodeMap);
	BuildLookupMap(RemoteNodes, TEXT("NodeGuid"), RemoteNodeMap);
#else
	BuildLookupMap(BaseNodes, TEXT("NodeTitle"), BaseNodeMap);
	BuildLookupMap(LocalNodes, TEXT("NodeTitle"), LocalNodeMap);
	BuildLookupMap(RemoteNodes, TEXT("NodeTitle"), RemoteNodeMap);
#endif

	// Get union of all node identifiers
	TSet<FString> AllNodeIds;
	BaseNodeMap.GetKeys(AllNodeIds);
	LocalNodeMap.GetKeys(AllNodeIds);
	RemoteNodeMap.GetKeys(AllNodeIds);

	for (const FString& NodeGuid : AllNodeIds)
	{
		TSharedPtr<FJsonObject> BaseNode = BaseNodeMap.FindRef(NodeGuid);
		TSharedPtr<FJsonObject> LocalNode = LocalNodeMap.FindRef(NodeGuid);
		TSharedPtr<FJsonObject> RemoteNode = RemoteNodeMap.FindRef(NodeGuid);

		FString NodeName = BaseNode.IsValid() ? BaseNode->GetStringField(TEXT("NodeName")) :
						   LocalNode.IsValid() ? LocalNode->GetStringField(TEXT("NodeName")) :
						   RemoteNode.IsValid() ? RemoteNode->GetStringField(TEXT("NodeName")) : TEXT("Unknown");

		// Node added in local only
		if (!BaseNode.IsValid() && LocalNode.IsValid() && !RemoteNode.IsValid())
		{
			const FString TargetId = LocalNode->GetStringField(TEXT("NodeGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddNode, GraphName, TargetId);
			// Store the actual node data for the Apply Engine
			FString NodeDataString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NodeDataString);
			FJsonSerializer::Serialize(LocalNode.ToSharedRef(), Writer);
			Op.AdditionalData.Add(TEXT("NodeData"), NodeDataString);
			
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: AddNode (Local) - Key: %s, Graph: %s, Data: %s"), *NodeGuid, *GraphName, *NodeDataString);
			OutOperations.Add(Op);
		}
		// Node added in remote only
		else if (!BaseNode.IsValid() && !LocalNode.IsValid() && RemoteNode.IsValid())
		{
			const FString TargetId = RemoteNode->GetStringField(TEXT("NodeGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddNode, GraphName, TargetId);
			// Store the actual node data for the Apply Engine
			FString NodeDataString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NodeDataString);
			FJsonSerializer::Serialize(RemoteNode.ToSharedRef(), Writer);
			Op.AdditionalData.Add(TEXT("NodeData"), NodeDataString);
			
			UE_LOG(LogTemp, Log, TEXT("DiffEngine: AddNode (Remote) - Key: %s, Graph: %s, Data: %s"), *NodeGuid, *GraphName, *NodeDataString);
			OutOperations.Add(Op);
		}
		// Node added in both (conflict)
		else if (!BaseNode.IsValid() && LocalNode.IsValid() && RemoteNode.IsValid())
		{
			TArray<FString> DifferingFields;
			if (CompareJsonObjects(LocalNode, RemoteNode, DifferingFields))
			{
				// Ignore GUID-only differences
				DifferingFields.RemoveAll([](const FString& Field){ return Field.Equals(TEXT("NodeGuid"), ESearchCase::IgnoreCase); });
				if (DifferingFields.Num() == 0)
				{
					UE_LOG(LogTemp, Log, TEXT("DiffEngine: Node '%s' added identically in both (GUID-only diff) - skipping operation"), *NodeName);
				}
				else
				{
					// Nodes have same name but different properties - this is a conflict
					UE_LOG(LogTemp, Warning, TEXT("DiffEngine: Node '%s' added in both but differs in: %s"), 
						*NodeName, *FString::Join(DifferingFields, TEXT(", ")));
				
					// Serialize the actual node data for "Keep Both" strategy
					FString LocalNodeData, RemoteNodeData;
					TSharedRef<TJsonWriter<>> LocalWriter = TJsonWriterFactory<>::Create(&LocalNodeData);
					TSharedRef<TJsonWriter<>> RemoteWriter = TJsonWriterFactory<>::Create(&RemoteNodeData);
					FJsonSerializer::Serialize(LocalNode.ToSharedRef(), LocalWriter);
					FJsonSerializer::Serialize(RemoteNode.ToSharedRef(), RemoteWriter);
				
					FMergeConflict Conflict = CreateConflict(
						TEXT("Node"),
						NodeName,
						TEXT("(not present)"),
						LocalNode->GetStringField(TEXT("NodeTitle")),
						RemoteNode->GetStringField(TEXT("NodeTitle")),
						DifferingFields,
						LocalNodeData, // Pass actual data
						RemoteNodeData // Pass actual data
					);
					Conflict.Severity = AnalyzeConflictSeverity(TEXT("Node"), DifferingFields);
					OutConflicts.Add(Conflict);
				}
			}
			else
			{
				// Nodes are completely identical - no operation needed
				UE_LOG(LogTemp, Log, TEXT("DiffEngine: Node '%s' added identically in both - skipping operation"), *NodeName);
			}
		}
		// Node removed
		else if (BaseNode.IsValid() && !LocalNode.IsValid() && !RemoteNode.IsValid())
		{
			const FString TargetId = BaseNode->GetStringField(TEXT("NodeGuid"));
			FMergeOperation Op = CreateOperation(EMergeOperationType::RemoveNode, GraphName, TargetId);
			OutOperations.Add(Op);
		}
		// Node exists in all three - check for modifications
		else if (BaseNode.IsValid() && LocalNode.IsValid() && RemoteNode.IsValid())
		{
			TArray<FString> LocalDiffs, RemoteDiffs;
			bool bLocalChanged = CompareJsonObjects(BaseNode, LocalNode, LocalDiffs);
			bool bRemoteChanged = CompareJsonObjects(BaseNode, RemoteNode, RemoteDiffs);

			if (bLocalChanged && bRemoteChanged)
			{
				// Both changed - check for conflicts
				TArray<FString> LocalRemoteDiffs;
				if (CompareJsonObjects(LocalNode, RemoteNode, LocalRemoteDiffs))
				{
					// Ignore GUID-only differences
					LocalRemoteDiffs.RemoveAll([](const FString& Field){ return Field.Equals(TEXT("NodeGuid"), ESearchCase::IgnoreCase); });
					if (LocalRemoteDiffs.Num() == 0)
					{
						// Treat as same changes after ignoring GUIDs
						const FString TargetId = BaseNode->GetStringField(TEXT("NodeGuid"));
						FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateNodeProperty, GraphName, TargetId);
						OutOperations.Add(Op);
						continue;
					}
					UE_LOG(LogTemp, Warning, TEXT("DiffEngine: Node '%s' in graph '%s' has conflicts in fields: %s"), 
						*NodeName, *GraphName, *FString::Join(LocalRemoteDiffs, TEXT(", ")));
					
					// Check if it's just a move operation
					if (IsMoveOperation(LocalNode, RemoteNode))
					{
						FMergeConflict Conflict = CreateConflict(
							TEXT("NodeMove"),
							NodeName,
							FString::Printf(TEXT("(%.0f,%.0f)"), BaseNode->GetNumberField(TEXT("NodePosX")), BaseNode->GetNumberField(TEXT("NodePosY"))),
							FString::Printf(TEXT("(%.0f,%.0f)"), LocalNode->GetNumberField(TEXT("NodePosX")), LocalNode->GetNumberField(TEXT("NodePosY"))),
							FString::Printf(TEXT("(%.0f,%.0f)"), RemoteNode->GetNumberField(TEXT("NodePosX")), RemoteNode->GetNumberField(TEXT("NodePosY"))),
							LocalRemoteDiffs
						);
						Conflict.Severity = EConflictSeverity::Low; // Move conflicts are low severity
						OutConflicts.Add(Conflict);
					}
					else
					{
						// Structural conflict
						FMergeConflict Conflict = CreateConflict(
							TEXT("Node"),
							NodeName,
							BaseNode->GetStringField(TEXT("NodeTitle")),
							LocalNode->GetStringField(TEXT("NodeTitle")),
							RemoteNode->GetStringField(TEXT("NodeTitle")),
							LocalRemoteDiffs
						);
						Conflict.Severity = AnalyzeConflictSeverity(TEXT("Node"), LocalRemoteDiffs);
						OutConflicts.Add(Conflict);
					}
				}
				else
				{
					// Same changes
					const FString TargetId = BaseNode->GetStringField(TEXT("NodeGuid"));
					FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateNodeProperty, GraphName, TargetId);
					OutOperations.Add(Op);
				}
			}
			else if (bLocalChanged)
			{
				// Only local changed
				const FString TargetId = BaseNode->GetStringField(TEXT("NodeGuid"));
				FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateNodeProperty, GraphName, TargetId);
				OutOperations.Add(Op);
			}
			else if (bRemoteChanged)
			{
				// Only remote changed
				const FString TargetId = BaseNode->GetStringField(TEXT("NodeGuid"));
				FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateNodeProperty, GraphName, TargetId);
				OutOperations.Add(Op);
			}
		}
	}
}

void FDiffEngine::DiffConnections(
	const FString& GraphName,
	const TArray<TSharedPtr<FJsonValue>>& BaseConnections,
	const TArray<TSharedPtr<FJsonValue>>& LocalConnections,
	const TArray<TSharedPtr<FJsonValue>>& RemoteConnections,
	TArray<FMergeOperation>& OutOperations,
	TArray<FMergeConflict>& OutConflicts)
{
	// Build sets for efficient comparison
	TSet<FString> BaseConnectionSet, LocalConnectionSet, RemoteConnectionSet;

	// Create connection identifiers
	for (const TSharedPtr<FJsonValue>& ConnValue : BaseConnections)
	{
		if (TSharedPtr<FJsonObject> ConnObj = ConnValue->AsObject())
		{
			FString ConnId = FString::Printf(TEXT("%s.%s->%s.%s"),
				*ConnObj->GetStringField(TEXT("SourceNodeGuid")),
				*ConnObj->GetStringField(TEXT("SourcePinName")),
				*ConnObj->GetStringField(TEXT("TargetNodeGuid")),
				*ConnObj->GetStringField(TEXT("TargetPinName")));
			BaseConnectionSet.Add(ConnId);
		}
	}

	for (const TSharedPtr<FJsonValue>& ConnValue : LocalConnections)
	{
		if (TSharedPtr<FJsonObject> ConnObj = ConnValue->AsObject())
		{
			FString ConnId = FString::Printf(TEXT("%s.%s->%s.%s"),
				*ConnObj->GetStringField(TEXT("SourceNodeGuid")),
				*ConnObj->GetStringField(TEXT("SourcePinName")),
				*ConnObj->GetStringField(TEXT("TargetNodeGuid")),
				*ConnObj->GetStringField(TEXT("TargetPinName")));
			LocalConnectionSet.Add(ConnId);
		}
	}

	for (const TSharedPtr<FJsonValue>& ConnValue : RemoteConnections)
	{
		if (TSharedPtr<FJsonObject> ConnObj = ConnValue->AsObject())
		{
			FString ConnId = FString::Printf(TEXT("%s.%s->%s.%s"),
				*ConnObj->GetStringField(TEXT("SourceNodeGuid")),
				*ConnObj->GetStringField(TEXT("SourcePinName")),
				*ConnObj->GetStringField(TEXT("TargetNodeGuid")),
				*ConnObj->GetStringField(TEXT("TargetPinName")));
			RemoteConnectionSet.Add(ConnId);
		}
	}

	// Get union of all connections
	TSet<FString> AllConnections = BaseConnectionSet;
	AllConnections.Append(LocalConnectionSet);
	AllConnections.Append(RemoteConnectionSet);

	for (const FString& ConnId : AllConnections)
	{
		bool bInBase = BaseConnectionSet.Contains(ConnId);
		bool bInLocal = LocalConnectionSet.Contains(ConnId);
		bool bInRemote = RemoteConnectionSet.Contains(ConnId);

		if (!bInBase && bInLocal && !bInRemote)
		{
			// Connection added locally only
			FMergeOperation Op = CreateOperation(EMergeOperationType::LinkPins, GraphName, ConnId);
			OutOperations.Add(Op);
		}
		else if (!bInBase && !bInLocal && bInRemote)
		{
			// Connection added remotely only
			FMergeOperation Op = CreateOperation(EMergeOperationType::LinkPins, GraphName, ConnId);
			OutOperations.Add(Op);
		}
		else if (!bInBase && bInLocal && bInRemote)
		{
			// Connection added in both - no conflict, just add once
			FMergeOperation Op = CreateOperation(EMergeOperationType::LinkPins, GraphName, ConnId);
			OutOperations.Add(Op);
		}
		else if (bInBase && !bInLocal && !bInRemote)
		{
			// Connection removed in both
			FMergeOperation Op = CreateOperation(EMergeOperationType::UnlinkPins, GraphName, ConnId);
			OutOperations.Add(Op);
		}
		else if (bInBase && bInLocal && !bInRemote)
		{
			// Connection removed remotely only
			FMergeOperation Op = CreateOperation(EMergeOperationType::UnlinkPins, GraphName, ConnId);
			Op.AdditionalData.Add(TEXT("Reason"), TEXT("RemoteRemoval"));
			OutOperations.Add(Op);
		}
		else if (bInBase && !bInLocal && bInRemote)
		{
			// Connection removed locally only
			FMergeOperation Op = CreateOperation(EMergeOperationType::UnlinkPins, GraphName, ConnId);
			Op.AdditionalData.Add(TEXT("Reason"), TEXT("LocalRemoval"));
			OutOperations.Add(Op);
		}
		// If connection exists in all three, no change needed
	}
}

void FDiffEngine::DiffComponents(
	const TArray<TSharedPtr<FJsonValue>>& BaseComponents,
	const TArray<TSharedPtr<FJsonValue>>& LocalComponents,
	const TArray<TSharedPtr<FJsonValue>>& RemoteComponents,
	TArray<FMergeOperation>& OutOperations,
	TArray<FMergeConflict>& OutConflicts)
{
	// Build lookup maps by component name
	TMap<FString, TSharedPtr<FJsonObject>> BaseCompMap, LocalCompMap, RemoteCompMap;
	BuildLookupMap(BaseComponents, TEXT("ComponentName"), BaseCompMap);
	BuildLookupMap(LocalComponents, TEXT("ComponentName"), LocalCompMap);
	BuildLookupMap(RemoteComponents, TEXT("ComponentName"), RemoteCompMap);

	// Get union of all component names
	TSet<FString> AllCompNames;
	BaseCompMap.GetKeys(AllCompNames);
	LocalCompMap.GetKeys(AllCompNames);
	RemoteCompMap.GetKeys(AllCompNames);

	for (const FString& CompName : AllCompNames)
	{
		TSharedPtr<FJsonObject> BaseComp = BaseCompMap.FindRef(CompName);
		TSharedPtr<FJsonObject> LocalComp = LocalCompMap.FindRef(CompName);
		TSharedPtr<FJsonObject> RemoteComp = RemoteCompMap.FindRef(CompName);

		// Component added in local only
		if (!BaseComp.IsValid() && LocalComp.IsValid() && !RemoteComp.IsValid())
		{
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddComponent, TEXT(""), CompName);
			OutOperations.Add(Op);
		}
		// Component added in remote only
		else if (!BaseComp.IsValid() && !LocalComp.IsValid() && RemoteComp.IsValid())
		{
			FMergeOperation Op = CreateOperation(EMergeOperationType::AddComponent, TEXT(""), CompName);
			OutOperations.Add(Op);
		}
		// Component added in both (conflict)
		else if (!BaseComp.IsValid() && LocalComp.IsValid() && RemoteComp.IsValid())
		{
			TArray<FString> DifferingFields;
			if (CompareJsonObjects(LocalComp, RemoteComp, DifferingFields))
			{
				FMergeConflict Conflict = CreateConflict(
					TEXT("Component"),
					CompName,
					TEXT("(not present)"),
					LocalComp->GetStringField(TEXT("ComponentClass")),
					RemoteComp->GetStringField(TEXT("ComponentClass")),
					DifferingFields
				);
				Conflict.Severity = AnalyzeConflictSeverity(TEXT("Component"), DifferingFields);
				OutConflicts.Add(Conflict);
			}
			else
			{
				// Same component added in both
				FMergeOperation Op = CreateOperation(EMergeOperationType::AddComponent, TEXT(""), CompName);
				OutOperations.Add(Op);
			}
		}
		// Component removed
		else if (BaseComp.IsValid() && !LocalComp.IsValid() && !RemoteComp.IsValid())
		{
			FMergeOperation Op = CreateOperation(EMergeOperationType::RemoveComponent, TEXT(""), CompName);
			OutOperations.Add(Op);
		}
		// Component exists in all three - check for modifications
		else if (BaseComp.IsValid() && LocalComp.IsValid() && RemoteComp.IsValid())
		{
			TArray<FString> LocalDiffs, RemoteDiffs;
			bool bLocalChanged = CompareJsonObjects(BaseComp, LocalComp, LocalDiffs);
			bool bRemoteChanged = CompareJsonObjects(BaseComp, RemoteComp, RemoteDiffs);

			if (bLocalChanged && bRemoteChanged)
			{
				TArray<FString> LocalRemoteDiffs;
				if (CompareJsonObjects(LocalComp, RemoteComp, LocalRemoteDiffs))
				{
					FMergeConflict Conflict = CreateConflict(
						TEXT("Component"),
						CompName,
						BaseComp->GetStringField(TEXT("ComponentClass")),
						LocalComp->GetStringField(TEXT("ComponentClass")),
						RemoteComp->GetStringField(TEXT("ComponentClass")),
						LocalRemoteDiffs
					);
					Conflict.Severity = AnalyzeConflictSeverity(TEXT("Component"), LocalRemoteDiffs);
					OutConflicts.Add(Conflict);
				}
				else
				{
					// Same changes
					FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateComponent, TEXT(""), CompName);
					OutOperations.Add(Op);
				}
			}
			else if (bLocalChanged)
			{
				FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateComponent, TEXT(""), CompName);
				OutOperations.Add(Op);
			}
			else if (bRemoteChanged)
			{
				FMergeOperation Op = CreateOperation(EMergeOperationType::UpdateComponent, TEXT(""), CompName);
				OutOperations.Add(Op);
			}
		}
	}
}

void FDiffEngine::BuildLookupMap(
	const TArray<TSharedPtr<FJsonValue>>& JsonArray,
	const FString& KeyField,
	TMap<FString, TSharedPtr<FJsonObject>>& OutMap)
{
	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		if (TSharedPtr<FJsonObject> Obj = Value->AsObject())
		{
			FString Key = Obj->GetStringField(KeyField);
			if (!Key.IsEmpty())
			{
				OutMap.Add(Key, Obj);
			}
		}
	}
}

FString FDiffEngine::GetObjectIdentifier(
	TSharedPtr<FJsonObject> JsonObject,
	const TArray<FString>& PreferredKeyFields)
{
	if (!JsonObject.IsValid())
	{
		return TEXT("");
	}

	for (const FString& KeyField : PreferredKeyFields)
	{
		FString Value = JsonObject->GetStringField(KeyField);
		if (!Value.IsEmpty())
		{
			return Value;
		}
	}

	return TEXT("Unknown");
}

FMergeOperation FDiffEngine::CreateOperation(
	EMergeOperationType OpType,
	const FString& TargetGraph,
	const FString& TargetId,
	const FString& PropertyName,
	const FString& OldValue,
	const FString& NewValue)
{
	FMergeOperation Op;
	Op.OperationType = OpType;
	Op.TargetGraph = TargetGraph;
	Op.TargetId = TargetId;
	Op.PropertyName = PropertyName;
	Op.OldValue = OldValue;
	Op.NewValue = NewValue;
	return Op;
}

FMergeConflict FDiffEngine::CreateConflict(
	const FString& ConflictType,
	const FString& ElementName,
	const FString& BaseValue,
	const FString& LocalValue,
	const FString& RemoteValue,
	const TArray<FString>& DifferingFields)
{
	FMergeConflict Conflict;
	Conflict.ConflictId = FGuid::NewGuid().ToString();
	Conflict.ConflictType = ConflictType;
	Conflict.ElementName = ElementName;
	Conflict.BaseValue = BaseValue;
	Conflict.LocalValue = LocalValue;
	Conflict.RemoteValue = RemoteValue;
	Conflict.DifferingFields = DifferingFields;
	return Conflict;
}

FMergeConflict FDiffEngine::CreateConflict(
	const FString& ConflictType,
	const FString& ElementName,
	const FString& BaseValue,
	const FString& LocalValue,
	const FString& RemoteValue,
	const TArray<FString>& DifferingFields,
	const FString& LocalData,
	const FString& RemoteData)
{
	FMergeConflict Conflict;
	Conflict.ConflictId = FGuid::NewGuid().ToString();
	Conflict.ConflictType = ConflictType;
	Conflict.ElementName = ElementName;
	Conflict.BaseValue = BaseValue;
	Conflict.LocalValue = LocalValue;
	Conflict.RemoteValue = RemoteValue;
	Conflict.DifferingFields = DifferingFields;
	Conflict.LocalData = LocalData;
	Conflict.RemoteData = RemoteData;
	return Conflict;
}

FString FDiffEngine::GenerateDiffSummary(const FDiffResult& DiffResult)
{
	FString Summary = FString::Printf(TEXT("Diff Summary: %d operations, %d conflicts\n\n"), 
		DiffResult.Operations.Num(), DiffResult.Conflicts.Num());

	// Categorize operations
	TMap<EMergeOperationType, int32> OpCounts;
	for (const FMergeOperation& Op : DiffResult.Operations)
	{
		OpCounts.FindOrAdd(Op.OperationType)++;
	}

	Summary += TEXT("Operations:\n");
	for (const auto& OpCount : OpCounts)
	{
		FString OpTypeName = TEXT("Unknown");
		switch (OpCount.Key)
		{
		case EMergeOperationType::AddNode: OpTypeName = TEXT("Add Node"); break;
		case EMergeOperationType::RemoveNode: OpTypeName = TEXT("Remove Node"); break;
		case EMergeOperationType::UpdateNodeProperty: OpTypeName = TEXT("Update Node"); break;
		case EMergeOperationType::AddVariable: OpTypeName = TEXT("Add Variable"); break;
		case EMergeOperationType::RemoveVariable: OpTypeName = TEXT("Remove Variable"); break;
		case EMergeOperationType::UpdateVariable: OpTypeName = TEXT("Update Variable"); break;
		case EMergeOperationType::LinkPins: OpTypeName = TEXT("Link Pins"); break;
		case EMergeOperationType::UnlinkPins: OpTypeName = TEXT("Unlink Pins"); break;
		case EMergeOperationType::AddComponent: OpTypeName = TEXT("Add Component"); break;
		case EMergeOperationType::RemoveComponent: OpTypeName = TEXT("Remove Component"); break;
		case EMergeOperationType::UpdateComponent: OpTypeName = TEXT("Update Component"); break;
		case EMergeOperationType::AddGraph: OpTypeName = TEXT("Add Graph"); break;
		case EMergeOperationType::RemoveGraph: OpTypeName = TEXT("Remove Graph"); break;
		}
		Summary += FString::Printf(TEXT("  %s: %d\n"), *OpTypeName, OpCount.Value);
	}

	if (DiffResult.Conflicts.Num() > 0)
	{
		Summary += TEXT("\nConflicts:\n");
		TMap<EConflictSeverity, int32> ConflictCounts;
		for (const FMergeConflict& Conflict : DiffResult.Conflicts)
		{
			ConflictCounts.FindOrAdd(Conflict.Severity)++;
		}

		for (const auto& ConflictCount : ConflictCounts)
		{
			FString SeverityName = TEXT("Unknown");
			switch (ConflictCount.Key)
			{
			case EConflictSeverity::Low: SeverityName = TEXT("Low"); break;
			case EConflictSeverity::Medium: SeverityName = TEXT("Medium"); break;
			case EConflictSeverity::High: SeverityName = TEXT("High"); break;
			case EConflictSeverity::Critical: SeverityName = TEXT("Critical"); break;
			}
			Summary += FString::Printf(TEXT("  %s Severity: %d\n"), *SeverityName, ConflictCount.Value);
		}
	}

	return Summary;
}

bool FDiffEngine::CompareJsonObjects(
	TSharedPtr<FJsonObject> ObjectA,
	TSharedPtr<FJsonObject> ObjectB,
	TArray<FString>& OutDifferingFields)
{
	if (!ObjectA.IsValid() && !ObjectB.IsValid())
	{
		return false; // Both null
	}

	if (!ObjectA.IsValid() || !ObjectB.IsValid())
	{
		OutDifferingFields.Add(TEXT("Existence"));
		return true; // One is null
	}

	// Get union of all field names
	TSet<FString> AllFields;
	for (const auto& Field : ObjectA->Values)
	{
		AllFields.Add(Field.Key);
	}
	for (const auto& Field : ObjectB->Values)
	{
		AllFields.Add(Field.Key);
	}

	bool bHasDifferences = false;
	for (const FString& FieldName : AllFields)
	{
		TSharedPtr<FJsonValue> ValueA = ObjectA->Values.FindRef(FieldName);
		TSharedPtr<FJsonValue> ValueB = ObjectB->Values.FindRef(FieldName);

		TArray<FString> FieldDiffs;
		if (CompareJsonValues(ValueA, ValueB, FieldDiffs))
		{
			OutDifferingFields.Add(FieldName);
			bHasDifferences = true;
		}
	}

	return bHasDifferences;
}

bool FDiffEngine::CompareJsonArrays(
	const TArray<TSharedPtr<FJsonValue>>& ArrayA,
	const TArray<TSharedPtr<FJsonValue>>& ArrayB,
	TArray<FString>& OutDifferingFields)
{
	if (ArrayA.Num() != ArrayB.Num())
	{
		OutDifferingFields.Add(TEXT("ArraySize"));
		return true;
	}

	for (int32 i = 0; i < ArrayA.Num(); i++)
	{
		TArray<FString> ElementDiffs;
		if (CompareJsonValues(ArrayA[i], ArrayB[i], ElementDiffs))
		{
			OutDifferingFields.Add(FString::Printf(TEXT("Element[%d]"), i));
			return true;
		}
	}

	return false;
}

bool FDiffEngine::IsMoveOperation(
	TSharedPtr<FJsonObject> BaseObj,
	TSharedPtr<FJsonObject> ModifiedObj)
{
	if (!BaseObj.IsValid() || !ModifiedObj.IsValid())
	{
		return false;
	}

	// Check if only position changed
	TArray<FString> DifferingFields;
	CompareJsonObjects(BaseObj, ModifiedObj, DifferingFields);

	// If only position fields differ, it's a move
	if (DifferingFields.Num() == 2 && 
		DifferingFields.Contains(TEXT("NodePosX")) && 
		DifferingFields.Contains(TEXT("NodePosY")))
	{
		return true;
	}

	return false;
}

bool FDiffEngine::AreVariableGuidsIdentical(
	TSharedPtr<FJsonObject> LocalVar,
	TSharedPtr<FJsonObject> RemoteVar)
{
	if (!LocalVar.IsValid() || !RemoteVar.IsValid())
	{
		return false;
	}

#if BPT_MERGE_USE_GUID_MATCHING
	// When GUID matching is enabled, compare GUIDs directly
	FString LocalGuid = LocalVar->GetStringField(TEXT("VariableGuid"));
	FString RemoteGuid = RemoteVar->GetStringField(TEXT("VariableGuid"));
	return (LocalGuid == RemoteGuid && !LocalGuid.IsEmpty());
#else
	// When GUID matching is disabled, always return true to skip GUID comparison
	// This allows matching by name/semantic keys instead
	return true;
#endif
}

bool FDiffEngine::CompareVariablesIgnoringGuid(
	TSharedPtr<FJsonObject> LocalVar,
	TSharedPtr<FJsonObject> RemoteVar,
	TArray<FString>& OutDifferingFields)
{
	if (!LocalVar.IsValid() || !RemoteVar.IsValid())
	{
		return false;
	}

	// Fields to ignore when comparing variables (these can be different without being a conflict)
	TArray<FString> IgnoredFields;
	
#if BPT_MERGE_USE_GUID_MATCHING
	// When GUID matching is enabled, we still compare GUIDs separately via AreVariableGuidsIdentical
	// So we don't ignore GUID in content comparison - it's handled separately
#else
	// When GUID matching is disabled, ignore GUID differences in content comparison
	// GUIDs will be different in different Blueprint instances when matching by name
	IgnoredFields.Add(TEXT("VariableGuid"));
#endif

	// Get union of all field names
	TSet<FString> AllFields;
	for (const auto& Field : LocalVar->Values)
	{
		AllFields.Add(Field.Key);
	}
	for (const auto& Field : RemoteVar->Values)
	{
		AllFields.Add(Field.Key);
	}

	bool bHasDifferences = false;
	for (const FString& FieldName : AllFields)
	{
		// Skip ignored fields
		if (IgnoredFields.Contains(FieldName))
		{
			continue;
		}

		TSharedPtr<FJsonValue> ValueA = LocalVar->Values.FindRef(FieldName);
		TSharedPtr<FJsonValue> ValueB = RemoteVar->Values.FindRef(FieldName);

		TArray<FString> FieldDiffs;
		if (CompareJsonValues(ValueA, ValueB, FieldDiffs))
		{
			OutDifferingFields.Add(FieldName);
			bHasDifferences = true;
		}
	}

	return bHasDifferences;
}

bool FDiffEngine::AreConnectionsEqual(
	TSharedPtr<FJsonObject> ConnA,
	TSharedPtr<FJsonObject> ConnB)
{
	if (!ConnA.IsValid() || !ConnB.IsValid())
	{
		return false;
	}

	return ConnA->GetStringField(TEXT("SourceNodeGuid")) == ConnB->GetStringField(TEXT("SourceNodeGuid")) &&
		   ConnA->GetStringField(TEXT("SourcePinName")) == ConnB->GetStringField(TEXT("SourcePinName")) &&
		   ConnA->GetStringField(TEXT("TargetNodeGuid")) == ConnB->GetStringField(TEXT("TargetNodeGuid")) &&
		   ConnA->GetStringField(TEXT("TargetPinName")) == ConnB->GetStringField(TEXT("TargetPinName"));
}
