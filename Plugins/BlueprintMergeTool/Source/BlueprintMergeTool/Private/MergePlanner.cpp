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

	// Detect and handle function-level conflicts (conflicts within functions)
	TArray<FMergeConflict> ProcessedConflicts;
	TSet<FString> FunctionsWithConflicts; // Track which functions have conflicts
	DetectFunctionLevelConflicts(DiffResult.Conflicts, DiffResult, ProcessedConflicts, FunctionsWithConflicts);
	
	// Filter out operations that target nodes in functions with function-level conflicts
	// These operations will fail because the nodes don't exist locally (different functions)
	OutMergePlan.AutoResolvedOperations.RemoveAll([&FunctionsWithConflicts](const FMergeOperation& Op)
	{
		// Check if this operation targets a node in a conflicted function
		if (Op.OperationType == EMergeOperationType::UpdateNodeProperty || 
		    Op.OperationType == EMergeOperationType::AddNode ||
		    Op.OperationType == EMergeOperationType::RemoveNode ||
		    Op.OperationType == EMergeOperationType::LinkPins ||
		    Op.OperationType == EMergeOperationType::UnlinkPins)
		{
			// Check if the target graph is a conflicted function
			if (FunctionsWithConflicts.Contains(Op.TargetGraph))
			{
				UE_LOG(LogTemp, Log, TEXT("MergePlanner: Filtering out operation targeting node %s in conflicted function '%s' (will be handled by function-level conflict resolution - copying entire function)"), 
					*Op.TargetId, *Op.TargetGraph);
				return true; // Remove this operation
			}
		}
		return false; // Keep this operation
	});

	// Apply automatic resolution to conflicts
	TArray<FMergeOperation> ConflictResolutions;
	TArray<FMergeConflict> UnresolvedConflicts;
	
	ApplyAutomaticResolution(
		ProcessedConflicts,
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
		UE_LOG(LogTemp, Log, TEXT("MergePlanner: Processing conflict - Type: %s, Element: %s, Strategy: %d"), 
			*Conflict.ConflictType, *Conflict.ElementName, (int32)Config.DefaultStrategy);
		
		if (CanAutoResolve(Conflict, Config))
		{
			TArray<FMergeOperation> ResolutionOps;
			bool bResolved = false;

			// Try specific resolution strategies based on conflict type
			if (Conflict.ConflictType == TEXT("NodeMove") && Config.bAutoResolvePositionConflicts)
			{
				UE_LOG(LogTemp, Log, TEXT("MergePlanner: Using ResolvePositionConflict for NodeMove"));
				bResolved = ResolvePositionConflict(Conflict, ResolutionOps);
			}
			else if (Conflict.ConflictType == TEXT("Variable"))
			{
				UE_LOG(LogTemp, Log, TEXT("MergePlanner: Using ResolveVariableConflict for Variable"));
				bResolved = ResolveVariableConflict(Conflict, Config, ResolutionOps);
			}
			else if (Conflict.ConflictType == TEXT("FunctionWithInternalConflicts"))
			{
				// Handle function-level conflicts: copy entire function from remote as a separate function with renamed version
				// This preserves the local function and creates a copy of the remote function with "_Conflict" suffix
				UE_LOG(LogTemp, Log, TEXT("MergePlanner: Resolving FunctionWithInternalConflicts for function '%s' - creating copy with renamed version"), *Conflict.ElementName);
				
				// Create an AddGraph operation to copy the remote function with a new name
				// We'll rename it to "conflict_OriginalName" to preserve both versions
				FMergeOperation Op = FDiffEngine::CreateOperation(EMergeOperationType::AddGraph, Conflict.GraphName, Conflict.GraphName);
				Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
				Op.AdditionalData.Add(TEXT("ConflictResolution"), TEXT("CopyFunctionWithConflicts"));
				Op.AdditionalData.Add(TEXT("MarkAsConflicted"), TEXT("true"));
				Op.AdditionalData.Add(TEXT("PreserveGuids"), TEXT("true"));
				Op.AdditionalData.Add(TEXT("CreateCopyWithRename"), TEXT("true")); // Flag to create a copy with renamed version
				Op.AdditionalData.Add(TEXT("RenameSuffix"), TEXT("_Conflict")); // Suffix for the conflicted copy
				Op.AdditionalData.Add(TEXT("OriginalFunctionName"), Conflict.ElementName); // Store original name
				
				// Use the remote graph data if available
				if (!Conflict.RemoteData.IsEmpty())
				{
					Op.AdditionalData.Add(TEXT("GraphData"), Conflict.RemoteData);
					UE_LOG(LogTemp, Log, TEXT("MergePlanner: FunctionWithInternalConflicts - creating renamed copy of remote function '%s' as 'conflict_%s'"), *Conflict.ElementName, *Conflict.ElementName);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("MergePlanner: FunctionWithInternalConflicts - no remote graph data in conflict for '%s'. The ApplyEngine will need to load it from the remote Blueprint."), *Conflict.ElementName);
					// Mark that we need to load the graph data from the remote Blueprint
					Op.AdditionalData.Add(TEXT("LoadFromRemoteBlueprint"), TEXT("true"));
					Op.AdditionalData.Add(TEXT("RemoteGraphName"), Conflict.ElementName);
				}
				
				// Store conflict node data for creating comment markers
				if (!Conflict.LocalData.IsEmpty())
				{
					Op.AdditionalData.Add(TEXT("ConflictNodeData"), Conflict.LocalData);
				}
				
				ResolutionOps.Add(Op);
				bResolved = true;
			}
			else if (Conflict.ConflictType == TEXT("Graph") || Conflict.ConflictType == TEXT("Function"))
			{
				// For Graph/Function conflicts with NonDestructive strategy, use strategy-based resolution
				UE_LOG(LogTemp, Log, TEXT("MergePlanner: Resolving %s conflict '%s' with strategy-based resolution"), *Conflict.ConflictType, *Conflict.ElementName);
				EResolutionStrategy Strategy = Config.PerTypeStrategies.FindRef(Conflict.ConflictType);
				if (Strategy == EResolutionStrategy::UseLocal) // Default value check
				{
					Strategy = Config.DefaultStrategy;
				}
				bResolved = ApplyStrategyResolution(Conflict, Strategy, Config, ResolutionOps);
			}
			else if (Conflict.ConflictType == TEXT("Node"))
			{
				// For Node/Graph/Function conflicts, use strategy-based resolution if NonDestructive is enabled
				// This allows the "Keep Both" logic to work properly
				if (Config.DefaultStrategy == EResolutionStrategy::NonDestructive)
				{
					UE_LOG(LogTemp, Log, TEXT("MergePlanner: Using strategy-based resolution for %s (NonDestructive)"), *Conflict.ConflictType);
					EResolutionStrategy Strategy = Config.PerTypeStrategies.FindRef(Conflict.ConflictType);
					if (Strategy == EResolutionStrategy::UseLocal) // Default value check
					{
						Strategy = Config.DefaultStrategy;
					}
					bResolved = ApplyStrategyResolution(Conflict, Strategy, Config, ResolutionOps);
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("MergePlanner: Using specific resolution for %s (other strategy)"), *Conflict.ConflictType);
					// Use the specific conflict resolution for other strategies
					if (Conflict.ConflictType == TEXT("Node"))
					{
						bResolved = ResolveNodeConflict(Conflict, Config, ResolutionOps);
					}
					else
					{
						// For Graph/Function conflicts, use strategy-based resolution as fallback
						EResolutionStrategy Strategy = Config.PerTypeStrategies.FindRef(Conflict.ConflictType);
						if (Strategy == EResolutionStrategy::UseLocal) // Default value check
						{
							Strategy = Config.DefaultStrategy;
						}
						bResolved = ApplyStrategyResolution(Conflict, Strategy, Config, ResolutionOps);
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("MergePlanner: Using strategy-based resolution for %s"), *Conflict.ConflictType);
				// Try strategy-based resolution
				EResolutionStrategy Strategy = Config.PerTypeStrategies.FindRef(Conflict.ConflictType);
				if (Strategy == EResolutionStrategy::UseLocal) // Default value check
				{
					Strategy = Config.DefaultStrategy;
				}
				bResolved = ApplyStrategyResolution(Conflict, Strategy, Config, ResolutionOps);
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
	const FMergePlannerConfig& Config,
	TArray<FMergeOperation>& OutOperations)
{
	// Non-destructive strategy: prefer additions over removals, combine when possible
	// When bKeepBothConflictingNodes is enabled, keep both versions of conflicting nodes
	
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
	else if (Conflict.ConflictType == TEXT("Node") || Conflict.ConflictType == TEXT("Graph") || Conflict.ConflictType == TEXT("Function"))
	{
		// For nodes/graphs/functions, implement "keep both" strategy - merge conflicting elements but leave them unconnected
		EMergeOperationType OpType = EMergeOperationType::AddNode;
		if (Conflict.ConflictType == TEXT("Graph") || Conflict.ConflictType == TEXT("Function"))
		{
			OpType = EMergeOperationType::AddGraph;
		}
		
		if (Conflict.LocalValue.Contains(TEXT("(not present)")))
		{
			// Local doesn't have it, add remote element
			FMergeOperation Op = FDiffEngine::CreateOperation(OpType, Conflict.ConflictId, Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
			Op.AdditionalData.Add(TEXT("KeepBoth"), TEXT("true"));
			
			// For Graph/Function conflicts, include GraphData if available
			if ((Conflict.ConflictType == TEXT("Graph") || Conflict.ConflictType == TEXT("Function")) && !Conflict.RemoteData.IsEmpty())
			{
				Op.AdditionalData.Add(TEXT("GraphData"), Conflict.RemoteData);
			}
			
			OutOperations.Add(Op);
			return true;
		}
		else if (Conflict.RemoteValue.Contains(TEXT("(not present)")))
		{
			// Remote doesn't have it, add local element
			FMergeOperation Op = FDiffEngine::CreateOperation(OpType, Conflict.ConflictId, Conflict.ConflictId);
			Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
			Op.AdditionalData.Add(TEXT("KeepBoth"), TEXT("true"));
			
			// For Graph/Function conflicts, include GraphData if available
			if ((Conflict.ConflictType == TEXT("Graph") || Conflict.ConflictType == TEXT("Function")) && !Conflict.LocalData.IsEmpty())
			{
				Op.AdditionalData.Add(TEXT("GraphData"), Conflict.LocalData);
			}
			
			OutOperations.Add(Op);
			return true;
		}
		else
		{
			// Both have the element but with different properties
			if (Config.bKeepBothConflictingNodes)
			{
				// For "Keep Both" strategy, we need to actually add the conflicting element to the graph
				// The issue is that we don't have the actual node/graph data in the conflict
				// So we'll create a special operation that the ApplyEngine can handle
				
				if (Conflict.ConflictType == TEXT("Graph") || Conflict.ConflictType == TEXT("Function"))
				{
					// For Graph/Function conflicts, we need to add the remote version of the graph
					// But we MUST have the graph data to do this
					UE_LOG(LogTemp, Log, TEXT("NonDestructive: Processing %s conflict '%s' - LocalValue: '%s', RemoteValue: '%s', RemoteData length: %d"), 
						*Conflict.ConflictType, *Conflict.ElementName, *Conflict.LocalValue, *Conflict.RemoteValue, Conflict.RemoteData.Len());
					
					if (Conflict.RemoteData.IsEmpty())
					{
						UE_LOG(LogTemp, Warning, TEXT("NonDestructive: Cannot add remote version of conflicting %s '%s' - RemoteData is missing (length: %d). Conflict will require manual resolution."), 
							*Conflict.ConflictType, *Conflict.ElementName, Conflict.RemoteData.Len());
						// Don't create the operation if we don't have the data - this will leave it as unresolved
						return false; // Let it be marked as unresolved for manual review
					}
					
					// Generate a new GUID for the remote version to avoid conflicts
					FString RemoteGraphId = FGuid::NewGuid().ToString();
					
					// Create an AddGraph operation for the remote version
					FMergeOperation RemoteGraphOp = FDiffEngine::CreateOperation(EMergeOperationType::AddGraph, RemoteGraphId, RemoteGraphId);
					RemoteGraphOp.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
					RemoteGraphOp.AdditionalData.Add(TEXT("KeepBoth"), TEXT("true"));
					RemoteGraphOp.AdditionalData.Add(TEXT("ConflictResolution"), TEXT("KeepBoth"));
					RemoteGraphOp.AdditionalData.Add(TEXT("OriginalConflictId"), Conflict.ConflictId);
					RemoteGraphOp.AdditionalData.Add(TEXT("ConflictType"), Conflict.ConflictType);
					RemoteGraphOp.AdditionalData.Add(TEXT("ElementName"), Conflict.ElementName);
					RemoteGraphOp.AdditionalData.Add(TEXT("RemoteValue"), Conflict.RemoteValue);
					RemoteGraphOp.AdditionalData.Add(TEXT("GraphData"), Conflict.RemoteData); // Always add GraphData since we checked it's not empty
					
					UE_LOG(LogTemp, Log, TEXT("NonDestructive: Adding remote version of conflicting %s '%s' with actual graph data (GUID: %s)"), *Conflict.ConflictType, *Conflict.ElementName, *RemoteGraphId);
					
					OutOperations.Add(RemoteGraphOp);
					return true;
				}
				else
				{
					// For Node conflicts, create an AddNode operation
					FString RemoteNodeId = FGuid::NewGuid().ToString();
					
					FMergeOperation RemoteNodeOp = FDiffEngine::CreateOperation(EMergeOperationType::AddNode, RemoteNodeId, RemoteNodeId);
					RemoteNodeOp.AdditionalData.Add(TEXT("Source"), TEXT("Remote"));
					RemoteNodeOp.AdditionalData.Add(TEXT("KeepBoth"), TEXT("true"));
					RemoteNodeOp.AdditionalData.Add(TEXT("ConflictResolution"), TEXT("KeepBoth"));
					RemoteNodeOp.AdditionalData.Add(TEXT("OriginalConflictId"), Conflict.ConflictId);
					RemoteNodeOp.AdditionalData.Add(TEXT("ConflictType"), Conflict.ConflictType);
					RemoteNodeOp.AdditionalData.Add(TEXT("ElementName"), Conflict.ElementName);
					RemoteNodeOp.AdditionalData.Add(TEXT("RemoteValue"), Conflict.RemoteValue);
					
					// Use the actual node data from the conflict
					if (!Conflict.RemoteData.IsEmpty())
					{
						RemoteNodeOp.AdditionalData.Add(TEXT("NodeData"), Conflict.RemoteData);
						UE_LOG(LogTemp, Log, TEXT("NonDestructive: Adding remote version of conflicting %s '%s' with actual node data (GUID: %s)"), *Conflict.ConflictType, *Conflict.ElementName, *RemoteNodeId);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("NonDestructive: No RemoteData available for conflicting %s '%s'"), *Conflict.ConflictType, *Conflict.ElementName);
					}
					
					OutOperations.Add(RemoteNodeOp);
					
					UE_LOG(LogTemp, Log, TEXT("NonDestructive: Adding remote version of conflicting %s '%s' with new GUID %s"), *Conflict.ConflictType, *Conflict.ElementName, *RemoteNodeId);
					return true;
				}
			}
			else
			{
				// Traditional non-destructive: prefer local by default
				FMergeOperation Op = FDiffEngine::CreateOperation(OpType, Conflict.ConflictId, Conflict.ConflictId);
				Op.AdditionalData.Add(TEXT("Source"), TEXT("Local"));
				Op.AdditionalData.Add(TEXT("Reason"), TEXT("NonDestructiveDefault"));
				OutOperations.Add(Op);
				return true;
			}
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
	const FMergePlannerConfig& Config,
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
		return ApplyNonDestructiveResolution(Conflict, Config, OutOperations);

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

void FMergePlanner::DetectFunctionLevelConflicts(
	const TArray<FMergeConflict>& InputConflicts,
	const FDiffResult& DiffResult,
	TArray<FMergeConflict>& OutProcessedConflicts,
	TSet<FString>& OutFunctionsWithConflicts)
{
	// Group Node conflicts by GraphName
	TMap<FString, TArray<const FMergeConflict*>> NodeConflictsByGraph;
	TSet<FString> ProcessedGraphs; // Track graphs that have been converted to function-level conflicts
	OutFunctionsWithConflicts.Empty(); // Clear output set
	
	for (const FMergeConflict& Conflict : InputConflicts)
	{
		// Only process Node conflicts (not NodeMove, as those are low severity)
		if (Conflict.ConflictType == TEXT("Node") && !Conflict.GraphName.IsEmpty())
		{
			// Check if this graph is a function (not EventGraph or ConstructionScript)
			FString GraphName = Conflict.GraphName;
			if (!GraphName.Contains(TEXT("EventGraph")) && 
				!GraphName.Contains(TEXT("ConstructionScript")) &&
				!GraphName.IsEmpty())
			{
				// This is likely a function graph
				if (!NodeConflictsByGraph.Contains(GraphName))
				{
					NodeConflictsByGraph.Add(GraphName, TArray<const FMergeConflict*>());
				}
				NodeConflictsByGraph[GraphName].Add(&Conflict);
			}
		}
	}
	
	// Process each graph with multiple node conflicts
	for (const auto& GraphConflicts : NodeConflictsByGraph)
	{
		const FString& GraphName = GraphConflicts.Key;
		const TArray<const FMergeConflict*>& Conflicts = GraphConflicts.Value;
		
		// Only create function-level conflict if there are multiple node conflicts
		// This indicates the function exists in both local and remote with internal conflicts
		if (Conflicts.Num() > 1)
		{
			UE_LOG(LogTemp, Log, TEXT("DetectFunctionLevelConflicts: Found %d node conflicts in function '%s', creating function-level conflict"), 
				Conflicts.Num(), *GraphName);
			
			// Mark this graph as processed
			ProcessedGraphs.Add(GraphName);
			OutFunctionsWithConflicts.Add(GraphName); // Track this function as having conflicts
			
			// Find the remote graph data from the diff result operations or conflicts
			// We need to get the remote graph data to copy it over
			FString RemoteGraphData;
			
			// First, look for Graph conflicts that might have the remote data
			for (const FMergeConflict& Conflict : InputConflicts)
			{
				if (Conflict.ConflictType == TEXT("Graph") && Conflict.ElementName == GraphName)
				{
					RemoteGraphData = Conflict.RemoteData;
					UE_LOG(LogTemp, Log, TEXT("DetectFunctionLevelConflicts: Found remote graph data in Graph conflict for '%s'"), *GraphName);
					break;
				}
			}
			
			// If not found in conflicts, look for AddGraph operations
			if (RemoteGraphData.IsEmpty())
			{
				for (const FMergeOperation& Op : DiffResult.Operations)
				{
					if (Op.OperationType == EMergeOperationType::AddGraph && 
						(Op.TargetGraph == GraphName || Op.TargetId == GraphName))
					{
						FString GraphData = Op.AdditionalData.FindRef(TEXT("GraphData"));
						if (!GraphData.IsEmpty())
						{
							RemoteGraphData = GraphData;
							UE_LOG(LogTemp, Log, TEXT("DetectFunctionLevelConflicts: Found remote graph data in AddGraph operation for '%s'"), *GraphName);
							break;
						}
					}
				}
			}
			
			// If still not found, try to extract from remote snapshot stored in DiffResult
			// The DiffResult should contain a reference to the remote snapshot
			if (RemoteGraphData.IsEmpty())
			{
				// Note: DiffResult doesn't store snapshots directly, but we can get remote graph data
				// from the operations or by reconstructing from conflict data
				// For now, we'll log and continue - the ApplyEngine will need to handle this case
				// by loading from the actual remote Blueprint
				UE_LOG(LogTemp, Warning, TEXT("DetectFunctionLevelConflicts: Could not find remote graph data for function '%s' in conflicts/operations. Will attempt to load from remote Blueprint during apply."), *GraphName);
			}
			
			// Create a FunctionWithInternalConflicts conflict
			FMergeConflict FunctionConflict;
			FunctionConflict.ConflictId = FGuid::NewGuid().ToString();
			FunctionConflict.ConflictType = TEXT("FunctionWithInternalConflicts");
			FunctionConflict.ElementName = GraphName;
			FunctionConflict.GraphName = GraphName;
			FunctionConflict.BaseValue = TEXT("Function exists in base");
			FunctionConflict.LocalValue = FString::Printf(TEXT("Function with %d conflicting nodes"), Conflicts.Num());
			FunctionConflict.RemoteValue = FString::Printf(TEXT("Function with %d conflicting nodes"), Conflicts.Num());
			FunctionConflict.Severity = EConflictSeverity::High; // Function-level conflicts are high severity
			FunctionConflict.DifferingFields.Add(TEXT("InternalNodes"));
			FunctionConflict.RemoteData = RemoteGraphData; // Store remote graph data for copying
			
			// Store information about which node conflicts are part of this function conflict
			// Format: "ConflictId1:NodePosX1:NodePosY1:NodeTitle1|ConflictId2:NodePosX2:NodePosY2:NodeTitle2|..."
			// Try to extract node positions from the remote graph data
			FString NodeConflictData;
			TSharedPtr<FJsonObject> RemoteGraphJson;
			
			// Try to parse remote graph data to extract node positions
			if (!RemoteGraphData.IsEmpty())
			{
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RemoteGraphData);
				FJsonSerializer::Deserialize(Reader, RemoteGraphJson);
			}
			
			for (int32 i = 0; i < Conflicts.Num(); i++)
			{
				if (i > 0) NodeConflictData += TEXT("|");
				
				const FMergeConflict* NodeConflict = Conflicts[i];
				FString ConflictId = NodeConflict->ConflictId;
				FString NodeTitle = NodeConflict->ElementName;
				FString NodePosX = TEXT("0");
				FString NodePosY = TEXT("0");
				
				// Try to extract position from remote graph data
				if (RemoteGraphJson.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
					if (RemoteGraphJson->TryGetArrayField(TEXT("Nodes"), NodesArray) && NodesArray)
					{
						for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
						{
							if (NodeValue.IsValid() && NodeValue->Type == EJson::Object)
							{
								TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
								FString NodeGuid = NodeObj->GetStringField(TEXT("NodeGuid"));
								FString ObjNodeTitle = NodeObj->GetStringField(TEXT("NodeTitle"));
								
								// Match by GUID or title
								if (NodeGuid == ConflictId || ObjNodeTitle == NodeTitle)
								{
									double PosX = NodeObj->GetNumberField(TEXT("NodePosX"));
									double PosY = NodeObj->GetNumberField(TEXT("NodePosY"));
									NodePosX = FString::Printf(TEXT("%.0f"), PosX);
									NodePosY = FString::Printf(TEXT("%.0f"), PosY);
									break;
								}
							}
						}
					}
				}
				
				NodeConflictData += FString::Printf(TEXT("%s:%s:%s:%s"), 
					*ConflictId, *NodePosX, *NodePosY, *NodeTitle);
			}
			// We'll use LocalData to store the node conflict information
			FunctionConflict.LocalData = NodeConflictData;
			
			OutProcessedConflicts.Add(FunctionConflict);
		}
	}
	
	// Add all other conflicts that weren't part of function-level conflicts
	for (const FMergeConflict& Conflict : InputConflicts)
	{
		// Skip Node conflicts that are part of a function-level conflict
		if (Conflict.ConflictType == TEXT("Node") && 
			!Conflict.GraphName.IsEmpty() && 
			ProcessedGraphs.Contains(Conflict.GraphName))
		{
			continue; // Skip this conflict, it's been replaced by a function-level conflict
		}
		
		// Add all other conflicts
		OutProcessedConflicts.Add(Conflict);
	}
	
	UE_LOG(LogTemp, Log, TEXT("DetectFunctionLevelConflicts: Processed %d conflicts, output %d conflicts (%d function-level conflicts created)"), 
		InputConflicts.Num(), OutProcessedConflicts.Num(), ProcessedGraphs.Num());
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
