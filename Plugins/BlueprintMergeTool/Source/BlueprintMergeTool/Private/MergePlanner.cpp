#include "../Public/MergePlanner.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TSharedPtr<ILLMAdapter> FMergePlanner::CurrentLLMAdapter = nullptr;

bool FMergePlanner::CreateMergePlan(
	const FDiffResult& DiffResult,
	const FMergePlannerConfig& Config,
	FMergePlan& OutMergePlan)
{
	UE_LOG(LogTemp, Log, TEXT("MergePlanner: Creating merge plan from %d operations and %d conflicts"), 
		DiffResult.Operations.Num(), DiffResult.Conflicts.Num());

	// Clear previous results
	OutMergePlan.AutoResolvedOperations.Empty();
	OutMergePlan.UnresolvedConflicts.Empty();
	OutMergePlan.ManualReviewRequired.Empty();
	OutMergePlan.bRequiresManualReview = false;
	OutMergePlan.bRequiresLLMResolution = false;

	// Start with all non-conflicting operations
	OutMergePlan.AutoResolvedOperations = DiffResult.Operations;

	// Apply automatic resolution to conflicts
	TArray<FMergeOperation> ConflictResolutions;
	TArray<FMergeConflict> UnresolvedConflicts;
	
	ApplyAutomaticResolution(
		DiffResult.Conflicts,
		Config,
		ConflictResolutions,
		UnresolvedConflicts
	);

	// Add resolved operations to the plan
	OutMergePlan.AutoResolvedOperations.Append(ConflictResolutions);

	// Handle unresolved conflicts
	if (UnresolvedConflicts.Num() > 0)
	{
		if (Config.bEnableLLMResolution && CurrentLLMAdapter.IsValid() && CurrentLLMAdapter->IsAvailable())
		{
			// Try LLM resolution
			TArray<FMergeOperation> LLMResolvedOperations;
			TArray<FMergeConflict> LLMFailedConflicts;

			FString Context = GenerateLLMContext(UnresolvedConflicts, DiffResult);
			
			if (UseLLMResolution(UnresolvedConflicts, CurrentLLMAdapter.Get(), Context, LLMResolvedOperations, LLMFailedConflicts))
			{
				OutMergePlan.AutoResolvedOperations.Append(LLMResolvedOperations);
				OutMergePlan.UnresolvedConflicts = LLMFailedConflicts;
				OutMergePlan.bRequiresLLMResolution = true;
			}
			else
			{
				// LLM failed, mark for manual review
				OutMergePlan.ManualReviewRequired = UnresolvedConflicts;
				OutMergePlan.bRequiresManualReview = true;
			}
		}
		else
		{
			// No LLM available, categorize by severity
			for (const FMergeConflict& Conflict : UnresolvedConflicts)
			{
				if ((float)Conflict.Severity / 4.0f >= Config.ConflictThresholdForManualReview)
				{
					OutMergePlan.ManualReviewRequired.Add(Conflict);
					OutMergePlan.bRequiresManualReview = true;
				}
				else
				{
					OutMergePlan.UnresolvedConflicts.Add(Conflict);
				}
			}
		}
	}

	// Detect GUID remapping needs
	TArray<FMergeOperation> RemapOperations;
	DetectGuidRemappingNeeds(OutMergePlan.AutoResolvedOperations, RemapOperations);
	OutMergePlan.AutoResolvedOperations.Append(RemapOperations);

	// Optimize operation order
	TArray<FMergeOperation> OptimizedOperations;
	OptimizeOperationOrder(OutMergePlan.AutoResolvedOperations, OptimizedOperations);
	OutMergePlan.AutoResolvedOperations = OptimizedOperations;

	// Generate summary
	OutMergePlan.PlanSummary = GeneratePlanSummary(OutMergePlan);

	UE_LOG(LogTemp, Log, TEXT("MergePlanner: Created merge plan with %d auto-resolved operations, %d manual conflicts"),
		OutMergePlan.AutoResolvedOperations.Num(), OutMergePlan.ManualReviewRequired.Num());

	return true;
}

void FMergePlanner::ApplyAutomaticResolution(
	const TArray<FMergeConflict>& Conflicts,
	const FMergePlannerConfig& Config,
	TArray<FMergeOperation>& OutResolvedOperations,
	TArray<FMergeConflict>& OutUnresolvedConflicts)
{
	for (const FMergeConflict& Conflict : Conflicts)
	{
		if (CanAutoResolve(Conflict, Config))
		{
			TArray<FMergeOperation> ResolutionOps;
			bool bResolved = false;

			// Try specific resolution strategies based on conflict type
			if (Conflict.ConflictType == TEXT("NodeMove") && Config.bAutoResolvePositionConflicts)
			{
				bResolved = ResolvePositionConflict(Conflict, ResolutionOps);
			}
			else if (Conflict.ConflictType == TEXT("Variable"))
			{
				bResolved = ResolveVariableConflict(Conflict, Config, ResolutionOps);
			}
			else if (Conflict.ConflictType == TEXT("Node"))
			{
				bResolved = ResolveNodeConflict(Conflict, Config, ResolutionOps);
			}
			else
			{
				// Try strategy-based resolution
				EResolutionStrategy Strategy = Config.PerTypeStrategies.FindRef(Conflict.ConflictType);
				if (Strategy == EResolutionStrategy::UseLocal) // Default value check
				{
					Strategy = Config.DefaultStrategy;
				}
				bResolved = ApplyStrategyResolution(Conflict, Strategy, ResolutionOps);
			}

			if (bResolved)
			{
				OutResolvedOperations.Append(ResolutionOps);
				UE_LOG(LogTemp, VeryVerbose, TEXT("Auto-resolved conflict: %s (%s)"), 
					*Conflict.ElementName, *Conflict.ConflictType);
			}
			else
			{
				OutUnresolvedConflicts.Add(Conflict);
			}
		}
		else
		{
			OutUnresolvedConflicts.Add(Conflict);
		}
	}
}

bool FMergePlanner::UseLLMResolution(
	const TArray<FMergeConflict>& Conflicts,
	ILLMAdapter* LLMAdapter,
	const FString& Context,
	TArray<FMergeOperation>& OutResolvedOperations,
	TArray<FMergeConflict>& OutFailedConflicts)
{
	if (!LLMAdapter || !LLMAdapter->IsAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("MergePlanner: LLM adapter not available"));
		OutFailedConflicts = Conflicts;
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("MergePlanner: Using LLM to resolve %d conflicts"), Conflicts.Num());

	// Try to resolve conflicts in batches
	const int32 BatchSize = 5; // Process conflicts in small batches
	for (int32 i = 0; i < Conflicts.Num(); i += BatchSize)
	{
		TArray<FMergeConflict> BatchConflicts;
		for (int32 j = i; j < FMath::Min(i + BatchSize, Conflicts.Num()); j++)
		{
			BatchConflicts.Add(Conflicts[j]);
		}

		TArray<FMergeOperation> BatchOperations;
		if (LLMAdapter->ResolveConflicts(BatchConflicts, Context, BatchOperations))
		{
			OutResolvedOperations.Append(BatchOperations);
		}
		else
		{
			// LLM failed for this batch
			OutFailedConflicts.Append(BatchConflicts);
		}
	}

	bool bSuccess = OutFailedConflicts.Num() < Conflicts.Num();
	UE_LOG(LogTemp, Log, TEXT("MergePlanner: LLM resolution completed. Resolved: %d, Failed: %d"), 
		OutResolvedOperations.Num(), OutFailedConflicts.Num());

	return bSuccess;
}

void FMergePlanner::SetLLMAdapter(TSharedPtr<ILLMAdapter> Adapter)
{
	CurrentLLMAdapter = Adapter;
}

TSharedPtr<ILLMAdapter> FMergePlanner::GetLLMAdapter()
{
	return CurrentLLMAdapter;
}

bool FMergePlanner::ValidateMergePlan(
	const FMergePlan& MergePlan,
	TArray<FString>& OutValidationErrors)
{
	OutValidationErrors.Empty();

	// Check for conflicting operations
	TMap<FString, TArray<int32>> TargetOperations; // Map target ID to operation indices

	for (int32 i = 0; i < MergePlan.AutoResolvedOperations.Num(); i++)
	{
		const FMergeOperation& Op = MergePlan.AutoResolvedOperations[i];
		FString TargetKey = FString::Printf(TEXT("%s_%s"), *Op.TargetGraph, *Op.TargetId);
		
		if (!TargetOperations.Contains(TargetKey))
		{
			TargetOperations.Add(TargetKey, TArray<int32>());
		}
		TargetOperations[TargetKey].Add(i);
	}

	// Check for conflicting operations on the same target
	for (const auto& TargetOps : TargetOperations)
	{
		if (TargetOps.Value.Num() > 1)
		{
			// Multiple operations on same target - check compatibility
			TArray<EMergeOperationType> OpTypes;
			for (int32 OpIndex : TargetOps.Value)
			{
				OpTypes.Add(MergePlan.AutoResolvedOperations[OpIndex].OperationType);
			}

			// Check for incompatible operations
			if (OpTypes.Contains(EMergeOperationType::AddNode) && OpTypes.Contains(EMergeOperationType::RemoveNode))
			{
				OutValidationErrors.Add(FString::Printf(TEXT("Conflicting Add/Remove operations on target: %s"), *TargetOps.Key));
			}
			if (OpTypes.Contains(EMergeOperationType::AddVariable) && OpTypes.Contains(EMergeOperationType::RemoveVariable))
			{
				OutValidationErrors.Add(FString::Printf(TEXT("Conflicting Add/Remove variable operations on: %s"), *TargetOps.Key));
			}
		}
	}

	// Check for dependency issues
	// TODO: Implement dependency validation (e.g., removing a node that other nodes depend on)

	return OutValidationErrors.Num() == 0;
}

bool FMergePlanner::ApplyNonDestructiveResolution(
	const FMergeConflict& Conflict,
	TArray<FMergeOperation>& OutOperations)
{
	// Non-destructive strategy: prefer additions over removals, combine when possible
	
	if (Conflict.ConflictType == TEXT("Variable"))
	{
		// For variables, prefer the version with more information
		if (Conflict.LocalValue.Contains(TEXT("(not present)")))
		{
			// Local doesn't have it, use remote
			FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::AddVariable, TEXT(""), Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
			OutOperations.Add(Op);
			return true;
		}
		else if (Conflict.RemoteValue.Contains(TEXT("(not present)")))
		{
			// Remote doesn't have it, use local
			FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::AddVariable, TEXT(""), Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
			OutOperations.Add(Op);
			return true;
		}
		else
		{
			// Both have it but different - prefer local by default
			FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
			Op.AdditionalData.Add(TEXT("Reason"), TEXT("NonDestructiveDefault"));
			OutOperations.Add(Op);
			return true;
		}
	}
	else if (Conflict.ConflictType == TEXT("Node"))
	{
		// For nodes, prefer keeping both if possible (rename if needed)
		if (Conflict.LocalValue.Contains(TEXT("(not present)")))
		{
			FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::AddNode, Conflict.ConflictId, Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
			OutOperations.Add(Op);
			return true;
		}
		else if (Conflict.RemoteValue.Contains(TEXT("(not present)")))
		{
			FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::AddNode, Conflict.ConflictId, Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
			OutOperations.Add(Op);
			return true;
		}
	}
	else if (Conflict.ConflictType == TEXT("NodeMove"))
	{
		// For position conflicts, use a smart positioning strategy
		return ResolvePositionConflict(Conflict, OutOperations);
	}

	return false;
}

bool FMergePlanner::ApplyStrategyResolution(
	const FMergeConflict& Conflict,
	EResolutionStrategy Strategy,
	TArray<FMergeOperation>& OutOperations)
{
	switch (Strategy)
	{
	case EResolutionStrategy::UseLocal:
		{
			// Use local version
			EMergeOperationType OpType = EMergeOperationType::UpdateNodeProperty;
			if (Conflict.ConflictType == TEXT("Variable"))
			{
				OpType = EMergeOperationType::UpdateVariable;
			}
			else if (Conflict.ConflictType == TEXT("Component"))
			{
				OpType = EMergeOperationType::UpdateComponent;
			}

			FMergeOperation Op = FDiffEngine::CreateOperation(OpType, TEXT(""), Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
			Op.AdditionalData.Add(TEXT("Strategy"), TEXT("UseLocal"));
			OutOperations.Add(Op);
			return true;
		}

	case EResolutionStrategy::UseRemote:
		{
			// Use remote version
			EMergeOperationType OpType = EMergeOperationType::UpdateNodeProperty;
			if (Conflict.ConflictType == TEXT("Variable"))
			{
				OpType = EMergeOperationType::UpdateVariable;
			}
			else if (Conflict.ConflictType == TEXT("Component"))
			{
				OpType = EMergeOperationType::UpdateComponent;
			}

			FMergeOperation Op = FDiffEngine::CreateOperation(OpType, TEXT(""), Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
			Op.AdditionalData.Add(TEXT("Strategy"), TEXT("UseRemote"));
			OutOperations.Add(Op);
			return true;
		}

	case EResolutionStrategy::UseBase:
		{
			// Revert to base version (effectively ignore both changes)
			// This might involve removing additions or reverting modifications
			UE_LOG(LogTemp, VeryVerbose, TEXT("Using base version for conflict: %s"), *Conflict.ElementName);
			// No operation needed - keeping base means no changes
			return true;
		}

	case EResolutionStrategy::NonDestructive:
		return ApplyNonDestructiveResolution(Conflict, OutOperations);

	case EResolutionStrategy::SmartMerge:
		// Try to intelligently combine changes
		if (Conflict.ConflictType == TEXT("Variable") && Conflict.DifferingFields.Num() == 1)
		{
			// Single field difference might be resolvable
			if (Conflict.DifferingFields[0] == TEXT("DefaultValue"))
			{
				// For default values, prefer non-empty values
				FString ValueToUse = !Conflict.LocalValue.IsEmpty() ? Conflict.LocalValue : Conflict.RemoteValue;
				FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), Conflict.ConflictId);
				Op.NewValue = ValueToUse;
				Op.AdditionalData.Add(TEXT("Strategy"), TEXT("SmartMerge"));
				OutOperations.Add(Op);
				return true;
			}
		}
		break;

	default:
		break;
	}

	return false;
}

bool FMergePlanner::CanAutoResolve(
	const FMergeConflict& Conflict,
	const FMergePlannerConfig& Config)
{
	// Low severity conflicts can usually be auto-resolved
	if (Conflict.Severity == EConflictSeverity::Low)
	{
		return true;
	}

	// Position conflicts can be auto-resolved if enabled
	if (Conflict.ConflictType == TEXT("NodeMove") && Config.bAutoResolvePositionConflicts)
	{
		return true;
	}

	// Variable conflicts with local preference
	if (Conflict.ConflictType == TEXT("Variable") && Config.bPreferLocalForVariables)
	{
		return true;
	}

	// Node conflicts with remote preference
	if (Conflict.ConflictType == TEXT("Node") && Config.bPreferRemoteForNodes)
	{
		return true;
	}

	// Check if there's a specific strategy for this conflict type
	if (Config.PerTypeStrategies.Contains(Conflict.ConflictType))
	{
		EResolutionStrategy Strategy = Config.PerTypeStrategies[Conflict.ConflictType];
		return Strategy != EResolutionStrategy::ManualResolve;
	}

	// Medium severity conflicts might be auto-resolvable with the right strategy
	if (Conflict.Severity == EConflictSeverity::Medium && Config.DefaultStrategy != EResolutionStrategy::ManualResolve)
	{
		return true;
	}

	return false;
}

bool FMergePlanner::ResolvePositionConflict(
	const FMergeConflict& Conflict,
	TArray<FMergeOperation>& OutOperations)
{
	// For position conflicts, use a smart positioning strategy
	// Could be: average positions, prefer local, prefer remote, or avoid overlaps
	
	// For now, prefer local positions
	FMergeOperation Op = FDiffEngine::CreateOperation(
		EMergeOperationType::MoveNode, 
		TEXT(""), // Will be filled from conflict context
		Conflict.ConflictId,
		TEXT("Position"),
		Conflict.BaseValue,
		Conflict.LocalValue
	);
	Op.AdditionalData.Add(TEXT("ResolutionReason"), TEXT("PreferLocal"));
	OutOperations.Add(Op);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Resolved position conflict for %s: using local position"), *Conflict.ElementName);
	return true;
}

bool FMergePlanner::ResolveVariableConflict(
	const FMergeConflict& Conflict,
	const FMergePlannerConfig& Config,
	TArray<FMergeOperation>& OutOperations)
{
	// Smart variable conflict resolution
	if (Config.bPreferLocalForVariables)
	{
		FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), Conflict.ConflictId);
		Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
		Op.AdditionalData.Add(TEXT("Reason"), TEXT("PreferLocalForVariables"));
		OutOperations.Add(Op);
		return true;
	}

	// Check if it's a safe merge (only cosmetic differences)
	bool bOnlyCosmeticDiffs = true;
	for (const FString& Field : Conflict.DifferingFields)
	{
		if (Field != TEXT("Category") && Field != TEXT("FriendlyName") && Field != TEXT("VarTooltip"))
		{
			bOnlyCosmeticDiffs = false;
			break;
		}
	}

	if (bOnlyCosmeticDiffs)
	{
		// Merge cosmetic fields by combining information
		FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::UpdateVariable, TEXT(""), Conflict.ConflictId);
		Op.AdditionalData.Add(TEXT("Source"), TEXT("Merged"));
		Op.AdditionalData.Add(TEXT("Reason"), TEXT("CosmeticMerge"));
		OutOperations.Add(Op);
		return true;
	}

	return false;
}

bool FMergePlanner::ResolveNodeConflict(
	const FMergeConflict& Conflict,
	const FMergePlannerConfig& Config,
	TArray<FMergeOperation>& OutOperations)
{
	// Smart node conflict resolution
	if (Config.bPreferRemoteForNodes)
	{
		FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::UpdateNodeProperty, TEXT(""), Conflict.ConflictId);
		Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
		Op.AdditionalData.Add(TEXT("Reason"), TEXT("PreferRemoteForNodes"));
		OutOperations.Add(Op);
		return true;
	}

	// Check if it's only a title/comment change
	if (Conflict.DifferingFields.Num() == 1 && 
		(Conflict.DifferingFields[0] == TEXT("NodeTitle") || Conflict.DifferingFields[0] == TEXT("NodeComment")))
	{
		// Combine titles/comments
		FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::UpdateNodeProperty, TEXT(""), Conflict.ConflictId);
		Op.PropertyName = Conflict.DifferingFields[0];
		Op.NewValue = FString::Printf(TEXT("%s | %s"), *Conflict.LocalValue, *Conflict.RemoteValue);
		Op.AdditionalData.Add(TEXT("Reason"), TEXT("CombinedTitles"));
		OutOperations.Add(Op);
		return true;
	}

	return false;
}

FString FMergePlanner::GenerateLLMContext(
	const TArray<FMergeConflict>& Conflicts,
	const FDiffResult& DiffResult)
{
	FString Context = TEXT("Blueprint Merge Context:\n\n");
	
	Context += FString::Printf(TEXT("Total Operations: %d\n"), DiffResult.Operations.Num());
	Context += FString::Printf(TEXT("Total Conflicts: %d\n"), DiffResult.Conflicts.Num());
	Context += FString::Printf(TEXT("Unresolved Conflicts: %d\n\n"), Conflicts.Num());

	Context += TEXT("Conflict Summary:\n");
	TMap<FString, int32> ConflictTypeCounts;
	for (const FMergeConflict& Conflict : Conflicts)
	{
		ConflictTypeCounts.FindOrAdd(Conflict.ConflictType)++;
	}

	for (const auto& TypeCount : ConflictTypeCounts)
	{
		Context += FString::Printf(TEXT("  %s: %d\n"), *TypeCount.Key, TypeCount.Value);
	}

	Context += TEXT("\nDiff Summary:\n");
	Context += DiffResult.DiffSummary;

	return Context;
}

FString FMergePlanner::BuildLLMResolutionPrompt(
	const TArray<FMergeConflict>& Conflicts,
	const FString& Context)
{
	FString Prompt = TEXT("You are an expert Unreal Engine Blueprint merge resolver. ");
	Prompt += TEXT("Analyze these Blueprint merge conflicts and provide resolution decisions.\n\n");

	Prompt += TEXT("CONTEXT:\n");
	Prompt += Context;
	Prompt += TEXT("\n\nCONFLICTS TO RESOLVE:\n");

	for (int32 i = 0; i < Conflicts.Num(); i++)
	{
		const FMergeConflict& Conflict = Conflicts[i];
		Prompt += FString::Printf(TEXT("\nConflict %d:\n"), i + 1);
		Prompt += FString::Printf(TEXT("  Type: %s\n"), *Conflict.ConflictType);
		Prompt += FString::Printf(TEXT("  Element: %s\n"), *Conflict.ElementName);
		Prompt += FString::Printf(TEXT("  Severity: %s\n"), 
			Conflict.Severity == EConflictSeverity::Low ? TEXT("Low") :
			Conflict.Severity == EConflictSeverity::Medium ? TEXT("Medium") :
			Conflict.Severity == EConflictSeverity::High ? TEXT("High") : TEXT("Critical"));
		Prompt += FString::Printf(TEXT("  Base: %s\n"), *Conflict.BaseValue);
		Prompt += FString::Printf(TEXT("  Local: %s\n"), *Conflict.LocalValue);
		Prompt += FString::Printf(TEXT("  Remote: %s\n"), *Conflict.RemoteValue);
		Prompt += FString::Printf(TEXT("  Differing Fields: %s\n"), *FString::Join(Conflict.DifferingFields, TEXT(", ")));
	}

	Prompt += TEXT("\n\nRESOLUTION RULES:\n");
	Prompt += TEXT("1. Prefer non-destructive solutions when possible\n");
	Prompt += TEXT("2. For position conflicts, choose positions that avoid overlaps\n");
	Prompt += TEXT("3. For variable conflicts, preserve type safety\n");
	Prompt += TEXT("4. For node conflicts, maintain execution flow integrity\n");
	Prompt += TEXT("5. When in doubt, prefer local changes over remote\n\n");

	Prompt += TEXT("OUTPUT FORMAT:\n");
	Prompt += TEXT("Respond with a JSON object containing 'merged_operations' array:\n");
	Prompt += TEXT("{\n");
	Prompt += TEXT("  \"merged_operations\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"operation_type\": \"UpdateVariable\" | \"UpdateNode\" | \"UseLocal\" | \"UseRemote\",\n");
	Prompt += TEXT("      \"target_id\": \"conflict_id_or_element_identifier\",\n");
	Prompt += TEXT("      \"resolution\": \"local\" | \"remote\" | \"base\" | \"custom\",\n");
	Prompt += TEXT("      \"reason\": \"explanation_of_choice\",\n");
	Prompt += TEXT("      \"custom_value\": \"only_if_resolution_is_custom\"\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ]\n");
	Prompt += TEXT("}\n\n");

	Prompt += TEXT("Provide ONLY the JSON response, no additional text.");

	return Prompt;
}

bool FMergePlanner::ParseLLMResponse(
	const FString& LLMResponse,
	TArray<FMergeOperation>& OutOperations)
{
	TSharedPtr<FJsonObject> ResponseObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LLMResponse);

	if (!FJsonSerializer::Deserialize(Reader, ResponseObject) || !ResponseObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("MergePlanner: Failed to parse LLM response as JSON"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
	if (!ResponseObject->TryGetArrayField(TEXT("merged_operations"), OperationsArray))
	{
		UE_LOG(LogTemp, Error, TEXT("MergePlanner: No 'merged_operations' field in LLM response"));
		return false;
	}

	for (const TSharedPtr<FJsonValue>& OpValue : *OperationsArray)
	{
		TSharedPtr<FJsonObject> OpObject = OpValue->AsObject();
		if (!OpObject.IsValid())
		{
			continue;
		}

		FMergeOperation Op;
		
		// Parse operation type
		FString OpTypeStr = OpObject->GetStringField(TEXT("operation_type"));
		if (OpTypeStr == TEXT("UpdateVariable"))
		{
			Op.OperationType = EMergeOperationType::UpdateVariable;
		}
		else if (OpTypeStr == TEXT("UpdateNode"))
		{
			Op.OperationType = EMergeOperationType::UpdateNodeProperty;
		}
		else if (OpTypeStr == TEXT("UpdateComponent"))
		{
			Op.OperationType = EMergeOperationType::UpdateComponent;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Unknown operation type from LLM: %s"), *OpTypeStr);
			continue;
		}

		Op.TargetId = OpObject->GetStringField(TEXT("target_id"));
		FString Resolution = OpObject->GetStringField(TEXT("resolution"));
		FString Reason = OpObject->GetStringField(TEXT("reason"));
		FString CustomValue = OpObject->GetStringField(TEXT("custom_value"));

		Op.AdditionalData.Add(TEXT("Resolution"), Resolution);
		Op.AdditionalData.Add(TEXT("Reason"), Reason);
		Op.AdditionalData.Add(TEXT("LLMResolved"), TEXT("true"));

		if (!CustomValue.IsEmpty())
		{
			Op.NewValue = CustomValue;
		}

		OutOperations.Add(Op);
	}

	UE_LOG(LogTemp, Log, TEXT("MergePlanner: Parsed %d operations from LLM response"), OutOperations.Num());
	return true;
}

void FMergePlanner::DetectGuidRemappingNeeds(
	const TArray<FMergeOperation>& Operations,
	TArray<FMergeOperation>& OutRemapOperations)
{
	// Look for operations that might need GUID remapping
	TSet<FString> AddedVariables, AddedNodes;

	for (const FMergeOperation& Op : Operations)
	{
		if (Op.OperationType == EMergeOperationType::AddVariable)
		{
			AddedVariables.Add(Op.TargetId);
		}
		else if (Op.OperationType == EMergeOperationType::AddNode)
		{
			AddedNodes.Add(Op.TargetId);
		}
	}

	// Check for potential GUID conflicts (same name variables/nodes being added)
	TMap<FString, TArray<FString>> NameToGuids;
	for (const FMergeOperation& Op : Operations)
	{
		if (Op.OperationType == EMergeOperationType::AddVariable || Op.OperationType == EMergeOperationType::AddNode)
		{
			FString ElementName = Op.AdditionalData.FindRef(TEXT("ElementName"));
			if (!ElementName.IsEmpty())
			{
				if (!NameToGuids.Contains(ElementName))
				{
					NameToGuids.Add(ElementName, TArray<FString>());
				}
				NameToGuids[ElementName].Add(Op.TargetId);
			}
		}
	}

	// Generate remap operations for duplicates
	for (const auto& NameGuids : NameToGuids)
	{
		if (NameGuids.Value.Num() > 1)
		{
			// Multiple GUIDs for same name - need remapping
			for (int32 i = 1; i < NameGuids.Value.Num(); i++) // Keep first, remap others
			{
				FMergeOperation RemapOp = FDiffEngine::CreateOperation(
					EMergeOperationType::RemapVariableGuid,
					TEXT(""),
					NameGuids.Value[i],
					TEXT("Guid"),
					NameGuids.Value[i],
					FGuid::NewGuid().ToString()
				);
				RemapOp.AdditionalData.Add(TEXT("OriginalName"), NameGuids.Key);
				RemapOp.AdditionalData.Add(TEXT("Reason"), TEXT("AvoidGuidConflict"));
				OutRemapOperations.Add(RemapOp);
			}
		}
	}
}

void FMergePlanner::OptimizeOperationOrder(
	const TArray<FMergeOperation>& Operations,
	TArray<FMergeOperation>& OutOptimizedOperations)
{
	// Create dependency-aware ordering
	TArray<FMergeOperation> RemapOps, AddOps, UpdateOps, LinkOps, RemoveOps;

	// Categorize operations
	for (const FMergeOperation& Op : Operations)
	{
		switch (Op.OperationType)
		{
		case EMergeOperationType::RemapVariableGuid:
			RemapOps.Add(Op);
			break;
		case EMergeOperationType::AddVariable:
		case EMergeOperationType::AddNode:
		case EMergeOperationType::AddComponent:
		case EMergeOperationType::AddGraph:
			AddOps.Add(Op);
			break;
		case EMergeOperationType::UpdateVariable:
		case EMergeOperationType::UpdateNodeProperty:
		case EMergeOperationType::UpdateComponent:
		case EMergeOperationType::MoveNode:
			UpdateOps.Add(Op);
			break;
		case EMergeOperationType::LinkPins:
			LinkOps.Add(Op);
			break;
		case EMergeOperationType::RemoveVariable:
		case EMergeOperationType::RemoveNode:
		case EMergeOperationType::RemoveComponent:
		case EMergeOperationType::UnlinkPins:
			RemoveOps.Add(Op);
			break;
		default:
			UpdateOps.Add(Op); // Default to update category
			break;
		}
	}

	// Optimal order: Remap -> Add -> Update -> Link -> Remove
	OutOptimizedOperations.Empty();
	OutOptimizedOperations.Append(RemapOps);
	OutOptimizedOperations.Append(AddOps);
	OutOptimizedOperations.Append(UpdateOps);
	OutOptimizedOperations.Append(LinkOps);
	OutOptimizedOperations.Append(RemoveOps);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Optimized operation order: %d total (%d remap, %d add, %d update, %d link, %d remove)"),
		OutOptimizedOperations.Num(), RemapOps.Num(), AddOps.Num(), UpdateOps.Num(), LinkOps.Num(), RemoveOps.Num());
}

FString FMergePlanner::GeneratePlanSummary(const FMergePlan& MergePlan)
{
	FString Summary = FString::Printf(TEXT("Merge Plan Summary:\n"));
	Summary += FString::Printf(TEXT("  Auto-resolved operations: %d\n"), MergePlan.AutoResolvedOperations.Num());
	Summary += FString::Printf(TEXT("  Unresolved conflicts: %d\n"), MergePlan.UnresolvedConflicts.Num());
	Summary += FString::Printf(TEXT("  Manual review required: %d\n"), MergePlan.ManualReviewRequired.Num());
	Summary += FString::Printf(TEXT("  Requires manual review: %s\n"), MergePlan.bRequiresManualReview ? TEXT("Yes") : TEXT("No"));
	Summary += FString::Printf(TEXT("  Requires LLM resolution: %s\n"), MergePlan.bRequiresLLMResolution ? TEXT("Yes") : TEXT("No"));

	if (MergePlan.ManualReviewRequired.Num() > 0)
	{
		Summary += TEXT("\nManual Review Required For:\n");
		for (const FMergeConflict& Conflict : MergePlan.ManualReviewRequired)
		{
			Summary += FString::Printf(TEXT("  - %s (%s)\n"), *Conflict.ElementName, *Conflict.ConflictType);
		}
	}

	return Summary;
}
