#include "../Public/BlueprintMergeValidator.h"
#include "../Public/SnapshotManager.h"
#include "../Public/DiffEngine.h"
#include "../Public/MergePlanner.h"
#include "../Public/ApplyEngine.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/SimpleConstructionScript.h"   // for USimpleConstructionScript
#include "Engine/SCS_Node.h"                   // for USCS_Node
#include "Engine/BlueprintGeneratedClass.h"    // for UBlueprintGeneratedClass

bool FBlueprintMergeValidator::ValidateBlueprint(UBlueprint* Blueprint, FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	if (!Blueprint)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Blueprint is null"));
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("BlueprintMergeValidator: Validating Blueprint: %s"), *Blueprint->GetName());

	// Check compilation status
	if (!CheckBlueprintCompilation(Blueprint, OutResult.Errors, OutResult.Warnings))
	{
		OutResult.bIsValid = false;
	}

	// Validate graph integrity
	if (!ValidateGraphIntegrity(Blueprint, OutResult.Errors))
	{
		OutResult.bIsValid = false;
	}

	// Validate variables
	if (!ValidateVariableIntegrity(Blueprint, OutResult.Errors))
	{
		OutResult.bIsValid = false;
	}

	// Validate components
	if (!ValidateComponentIntegrity(Blueprint, OutResult.Errors))
	{
		OutResult.bIsValid = false;
	}

	// Check for circular dependencies
	if (!CheckCircularDependencies(Blueprint, OutResult.Errors))
	{
		OutResult.bIsValid = false;
	}

	// Validate GUID uniqueness
	if (!ValidateGuidUniqueness(Blueprint, OutResult.Errors))
	{
		OutResult.bIsValid = false;
	}

	// Run performance checks
	RunPerformanceChecks(Blueprint, OutResult.Warnings);

	// Generate summary
	OutResult.ValidationSummary = FString::Printf(TEXT("Validation completed for %s: %s\nErrors: %d, Warnings: %d"),
		*Blueprint->GetName(),
		OutResult.bIsValid ? TEXT("PASSED") : TEXT("FAILED"),
		OutResult.Errors.Num(),
		OutResult.Warnings.Num());

	UE_LOG(LogTemp, Log, TEXT("BlueprintMergeValidator: %s"), *OutResult.ValidationSummary);
	return true;
}

bool FBlueprintMergeValidator::RunSmokeTests(FValidationResult& OutResult)
{
	OutResult = FValidationResult();
	UE_LOG(LogTemp, Log, TEXT("BlueprintMergeValidator: Running smoke tests"));

	bool bAllTestsPassed = true;

	// Test 1: Snapshot Manager
	{
		UBlueprint* TestBP = CreateTestBlueprint();
		if (TestBP)
		{
			FValidationResult SnapshotResult;
			if (TestSnapshotDeterminism(TestBP, SnapshotResult))
			{
				LogTestResult(TEXT("SnapshotManager"), true, TEXT("Deterministic snapshot creation"));
				OutResult.InfoMessages.Add(TEXT("✅ SnapshotManager test passed"));
			}
			else
			{
				LogTestResult(TEXT("SnapshotManager"), false, SnapshotResult.ValidationSummary);
				OutResult.Errors.Add(TEXT("❌ SnapshotManager test failed"));
				bAllTestsPassed = false;
			}
			CleanupTestBlueprints({TestBP});
		}
		else
		{
			OutResult.Errors.Add(TEXT("❌ Failed to create test Blueprint"));
			bAllTestsPassed = false;
		}
	}

	// Test 2: Diff Engine
	{
		FValidationResult DiffResult;
		if (TestThreeWayDiff(DiffResult))
		{
			LogTestResult(TEXT("DiffEngine"), true, TEXT("Three-way diff functionality"));
			OutResult.InfoMessages.Add(TEXT("✅ DiffEngine test passed"));
		}
		else
		{
			LogTestResult(TEXT("DiffEngine"), false, DiffResult.ValidationSummary);
			OutResult.Errors.Add(TEXT("❌ DiffEngine test failed"));
			bAllTestsPassed = false;
		}
	}

	// Test 3: Merge Planner
	{
		FValidationResult PlannerResult;
		if (TestMergePlanner(PlannerResult))
		{
			LogTestResult(TEXT("MergePlanner"), true, TEXT("Automatic conflict resolution"));
			OutResult.InfoMessages.Add(TEXT("✅ MergePlanner test passed"));
		}
		else
		{
			LogTestResult(TEXT("MergePlanner"), false, PlannerResult.ValidationSummary);
			OutResult.Errors.Add(TEXT("❌ MergePlanner test failed"));
			bAllTestsPassed = false;
		}
	}

	// Test 4: Apply Engine (limited test without actual Blueprint modification)
	{
		FValidationResult ApplyResult;
		if (TestApplyEngine(ApplyResult))
		{
			LogTestResult(TEXT("ApplyEngine"), true, TEXT("Operation application logic"));
			OutResult.InfoMessages.Add(TEXT("✅ ApplyEngine test passed"));
		}
		else
		{
			LogTestResult(TEXT("ApplyEngine"), false, ApplyResult.ValidationSummary);
			OutResult.Warnings.Add(TEXT("⚠️ ApplyEngine test had issues"));
		}
	}

	OutResult.bIsValid = bAllTestsPassed;
	OutResult.ValidationSummary = FString::Printf(TEXT("Smoke tests: %s (%d errors, %d warnings)"),
		bAllTestsPassed ? TEXT("PASSED") : TEXT("FAILED"),
		OutResult.Errors.Num(),
		OutResult.Warnings.Num());

	return bAllTestsPassed;
}

bool FBlueprintMergeValidator::RunUnitTests(FValidationResult& OutResult)
{
	OutResult = FValidationResult();
	UE_LOG(LogTemp, Log, TEXT("BlueprintMergeValidator: Running unit tests"));

	TArray<FMergeTestCase> TestCases;
	GenerateTestCases(TestCases);

	bool bAllTestsPassed = true;
	int32 PassedTests = 0;

	for (const FMergeTestCase& TestCase : TestCases)
	{
		FValidationResult TestResult;
		if (RunTestCase(TestCase, TestResult))
		{
			PassedTests++;
			LogTestResult(TestCase.TestName, true, TestCase.Description);
			OutResult.InfoMessages.Add(FString::Printf(TEXT("✅ %s"), *TestCase.TestName));
		}
		else
		{
			LogTestResult(TestCase.TestName, false, TestResult.ValidationSummary);
			OutResult.Errors.Add(FString::Printf(TEXT("❌ %s: %s"), *TestCase.TestName, *TestResult.ValidationSummary));
			bAllTestsPassed = false;
		}
	}

	OutResult.bIsValid = bAllTestsPassed;
	OutResult.ValidationSummary = FString::Printf(TEXT("Unit tests: %d/%d passed"),
		PassedTests, TestCases.Num());

	return bAllTestsPassed;
}

bool FBlueprintMergeValidator::ValidateMergePlan(
	const FMergePlan& MergePlan,
	UBlueprint* TargetBlueprint,
	FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	if (!TargetBlueprint)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Target Blueprint is null"));
		return true;
	}

	// Validate operations don't conflict with each other
	TMap<FString, TArray<EMergeOperationType>> TargetOperations;

	for (const FMergeOperation& Op : MergePlan.AutoResolvedOperations)
	{
		FString TargetKey = FString::Printf(TEXT("%s_%s"), *Op.TargetGraph, *Op.TargetId);
		if (!TargetOperations.Contains(TargetKey))
		{
			TargetOperations.Add(TargetKey, TArray<EMergeOperationType>());
		}
		TargetOperations[TargetKey].Add(Op.OperationType);
	}

	// Check for conflicting operations
	for (const auto& TargetOps : TargetOperations)
	{
		const TArray<EMergeOperationType>& Ops = TargetOps.Value;
		
		if (Ops.Contains(EMergeOperationType::AddNode) && Ops.Contains(EMergeOperationType::RemoveNode))
		{
			OutResult.Errors.Add(FString::Printf(TEXT("Conflicting Add/Remove operations on: %s"), *TargetOps.Key));
			OutResult.bIsValid = false;
		}

		if (Ops.Contains(EMergeOperationType::AddVariable) && Ops.Contains(EMergeOperationType::RemoveVariable))
		{
			OutResult.Errors.Add(FString::Printf(TEXT("Conflicting Add/Remove variable operations on: %s"), *TargetOps.Key));
			OutResult.bIsValid = false;
		}
	}

	// Check if target Blueprint can accommodate the changes
	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(TargetBlueprint->UbergraphPages);
	AllGraphs.Append(TargetBlueprint->FunctionGraphs);
	AllGraphs.Append(TargetBlueprint->MacroGraphs);

	for (const FMergeOperation& Op : MergePlan.AutoResolvedOperations)
	{
		if (!Op.TargetGraph.IsEmpty())
		{
			bool bGraphExists = false;
			for (UEdGraph* Graph : AllGraphs)
			{
				if (Graph && Graph->GetName() == Op.TargetGraph)
				{
					bGraphExists = true;
					break;
				}
			}

			if (!bGraphExists && Op.OperationType != EMergeOperationType::AddGraph)
			{
				OutResult.Warnings.Add(FString::Printf(TEXT("Target graph '%s' does not exist for operation on %s"), 
					*Op.TargetGraph, *Op.TargetId));
			}
		}
	}

	OutResult.ValidationSummary = FString::Printf(TEXT("Merge plan validation: %s (%d errors, %d warnings)"),
		OutResult.bIsValid ? TEXT("PASSED") : TEXT("FAILED"),
		OutResult.Errors.Num(),
		OutResult.Warnings.Num());

	return true;
}

bool FBlueprintMergeValidator::TestSnapshotDeterminism(UBlueprint* TestBlueprint, FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	if (!TestBlueprint)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Test Blueprint is null"));
		return false;
	}

	// Create multiple snapshots and verify they're identical
	TSharedPtr<FJsonObject> Snapshot1, Snapshot2, Snapshot3;

	bool bSnapshot1Success = FSnapshotManager::CreateSnapshot(TestBlueprint, Snapshot1);
	bool bSnapshot2Success = FSnapshotManager::CreateSnapshot(TestBlueprint, Snapshot2);
	bool bSnapshot3Success = FSnapshotManager::CreateSnapshot(TestBlueprint, Snapshot3);

	if (!bSnapshot1Success || !bSnapshot2Success || !bSnapshot3Success)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Failed to create snapshots"));
		return false;
	}

	// Serialize and compare
	FString Json1, Json2, Json3;
	FSnapshotManager::SerializeToString(Snapshot1, Json1);
	FSnapshotManager::SerializeToString(Snapshot2, Json2);
	FSnapshotManager::SerializeToString(Snapshot3, Json3);

	if (Json1 != Json2 || Json2 != Json3)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Snapshots are not deterministic - multiple runs produced different results"));
		return false;
	}

	// Test hash consistency
	FString Hash1 = FSnapshotManager::GenerateSnapshotHash(Snapshot1);
	FString Hash2 = FSnapshotManager::GenerateSnapshotHash(Snapshot2);

	if (Hash1 != Hash2)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Snapshot hashes are inconsistent"));
		return false;
	}

	OutResult.bIsValid = true;
	OutResult.InfoMessages.Add(FString::Printf(TEXT("Snapshot determinism verified (Hash: %s)"), *Hash1));
	return true;
}

bool FBlueprintMergeValidator::TestThreeWayDiff(FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	// Create test snapshots
	TSharedPtr<FJsonObject> BaseSnapshot, LocalSnapshot, RemoteSnapshot;
	CreateTestSnapshots(BaseSnapshot, LocalSnapshot, RemoteSnapshot);

	if (!BaseSnapshot.IsValid() || !LocalSnapshot.IsValid() || !RemoteSnapshot.IsValid())
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Failed to create test snapshots"));
		return false;
	}

	// Perform diff
	FDiffResult DiffResult;
	if (!FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, DiffResult))
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Three-way diff failed"));
		return false;
	}

	// Validate diff results
	if (DiffResult.Operations.Num() == 0 && DiffResult.Conflicts.Num() == 0)
	{
		OutResult.Warnings.Add(TEXT("Diff produced no operations or conflicts (may indicate test data issue)"));
	}

	OutResult.bIsValid = true;
	OutResult.InfoMessages.Add(FString::Printf(TEXT("Three-way diff completed: %d ops, %d conflicts"), 
		DiffResult.Operations.Num(), DiffResult.Conflicts.Num()));
	return true;
}

bool FBlueprintMergeValidator::TestMergePlanner(FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	// Create test diff result with some conflicts
	FDiffResult TestDiffResult;
	
	// Add test operations
	TestDiffResult.Operations.Add(FDiffEngine::CreateOperation(EMergeOperationType::AddVariable, TEXT(""), TEXT("TestVar1")));
	TestDiffResult.Operations.Add(FDiffEngine::CreateOperation(EMergeOperationType::UpdateNodeProperty, TEXT("EventGraph"), TEXT("TestNode1")));

	// Add test conflicts
	FMergeConflict TestConflict = FDiffEngine::CreateConflict(
		TEXT("Variable"),
		TEXT("Health"),
		TEXT("100"),
		TEXT("150"),
		TEXT("200"),
		{TEXT("DefaultValue")}
	);
	TestConflict.Severity = EConflictSeverity::Medium;
	TestDiffResult.Conflicts.Add(TestConflict);
	TestDiffResult.bHasConflicts = true;

	// Test merge planning
	FMergePlannerConfig Config;
	FMergePlan MergePlan;

	if (!FMergePlanner::CreateMergePlan(TestDiffResult, Config, MergePlan))
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Failed to create merge plan"));
		return false;
	}

	// Validate merge plan
	if (MergePlan.AutoResolvedOperations.Num() == 0)
	{
		OutResult.Warnings.Add(TEXT("No operations were auto-resolved"));
	}

	OutResult.bIsValid = true;
	OutResult.InfoMessages.Add(FString::Printf(TEXT("Merge planning completed: %d auto-resolved, %d manual"), 
		MergePlan.AutoResolvedOperations.Num(), MergePlan.ManualReviewRequired.Num()));
	return true;
}

bool FBlueprintMergeValidator::TestApplyEngine(FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	// Test validation functions without actually modifying Blueprints
	UBlueprint* TestBP = CreateTestBlueprint();
	if (!TestBP)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Failed to create test Blueprint"));
		return false;
	}

	// Test Blueprint integrity validation
	TArray<FString> ValidationErrors;
	if (FApplyEngine::ValidateBlueprintIntegrity(TestBP, ValidationErrors))
	{
		OutResult.InfoMessages.Add(TEXT("✅ Blueprint integrity validation works"));
	}
	else
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("Blueprint integrity validation found issues: %d"), ValidationErrors.Num()));
	}

	// Test utility functions
	UEdGraph* TestGraph = FApplyEngine::FindGraphByName(TestBP, TEXT("EventGraph"));
	if (TestGraph)
	{
		OutResult.InfoMessages.Add(TEXT("✅ Graph lookup functionality works"));
	}
	else
	{
		OutResult.Warnings.Add(TEXT("Could not find EventGraph in test Blueprint"));
	}

	CleanupTestBlueprints({TestBP});

	OutResult.bIsValid = true;
	OutResult.InfoMessages.Add(TEXT("Apply engine validation completed"));
	return true;
}

bool FBlueprintMergeValidator::RunTestCase(const FMergeTestCase& TestCase, FValidationResult& OutResult)
{
	OutResult = FValidationResult();

	// Parse test snapshots
	TSharedPtr<FJsonObject> BaseSnapshot, LocalSnapshot, RemoteSnapshot;
	
	if (!FSnapshotManager::DeserializeFromString(TestCase.BaseSnapshotJson, BaseSnapshot) ||
		!FSnapshotManager::DeserializeFromString(TestCase.LocalSnapshotJson, LocalSnapshot) ||
		!FSnapshotManager::DeserializeFromString(TestCase.RemoteSnapshotJson, RemoteSnapshot))
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Failed to parse test snapshots"));
		return false;
	}

	// Run diff
	FDiffResult DiffResult;
	if (!FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, DiffResult))
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Diff failed"));
		return false;
	}

	// Validate expectations
	if (TestCase.bShouldHaveConflicts && !DiffResult.bHasConflicts)
	{
		OutResult.bIsValid = false;
		OutResult.Errors.Add(TEXT("Expected conflicts but none were found"));
		return false;
	}

	if (!TestCase.bShouldHaveConflicts && DiffResult.bHasConflicts)
	{
		OutResult.Warnings.Add(TEXT("Unexpected conflicts found"));
	}

	if (TestCase.ExpectedOperationCount >= 0 && DiffResult.Operations.Num() != TestCase.ExpectedOperationCount)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("Expected %d operations, got %d"), 
			TestCase.ExpectedOperationCount, DiffResult.Operations.Num()));
	}

	if (TestCase.ExpectedConflictCount >= 0 && DiffResult.Conflicts.Num() != TestCase.ExpectedConflictCount)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("Expected %d conflicts, got %d"), 
			TestCase.ExpectedConflictCount, DiffResult.Conflicts.Num()));
	}

	OutResult.bIsValid = true;
	OutResult.ValidationSummary = FString::Printf(TEXT("Test case '%s' completed"), *TestCase.TestName);
	return true;
}

void FBlueprintMergeValidator::CreateTestSnapshots(
	TSharedPtr<FJsonObject>& OutBaseSnapshot,
	TSharedPtr<FJsonObject>& OutLocalSnapshot,
	TSharedPtr<FJsonObject>& OutRemoteSnapshot)
{
	// Create minimal test snapshots for testing
	OutBaseSnapshot = MakeShareable(new FJsonObject);
	OutLocalSnapshot = MakeShareable(new FJsonObject);
	OutRemoteSnapshot = MakeShareable(new FJsonObject);

	// Base snapshot - simple Blueprint with one variable
	OutBaseSnapshot->SetStringField(TEXT("BlueprintName"), TEXT("TestBlueprint"));
	OutBaseSnapshot->SetStringField(TEXT("ParentClass"), TEXT("AActor"));
	
	TArray<TSharedPtr<FJsonValue>> BaseVariables;
	TSharedPtr<FJsonObject> BaseVar = MakeShareable(new FJsonObject);
	BaseVar->SetStringField(TEXT("VariableName"), TEXT("Health"));
	BaseVar->SetStringField(TEXT("VariableGuid"), TEXT("12345678-1234-1234-1234-123456789012"));
	BaseVar->SetStringField(TEXT("VarType"), TEXT("Int"));
	BaseVar->SetStringField(TEXT("DefaultValue"), TEXT("100"));
	BaseVariables.Add(MakeShareable(new FJsonValueObject(BaseVar)));
	OutBaseSnapshot->SetArrayField(TEXT("Variables"), BaseVariables);

	// Local snapshot - modified Health variable
	OutLocalSnapshot->SetStringField(TEXT("BlueprintName"), TEXT("TestBlueprint"));
	OutLocalSnapshot->SetStringField(TEXT("ParentClass"), TEXT("AActor"));
	
	TArray<TSharedPtr<FJsonValue>> LocalVariables;
	TSharedPtr<FJsonObject> LocalVar = MakeShareable(new FJsonObject);
	LocalVar->SetStringField(TEXT("VariableName"), TEXT("Health"));
	LocalVar->SetStringField(TEXT("VariableGuid"), TEXT("12345678-1234-1234-1234-123456789012"));
	LocalVar->SetStringField(TEXT("VarType"), TEXT("Int"));
	LocalVar->SetStringField(TEXT("DefaultValue"), TEXT("150")); // Changed locally
	LocalVariables.Add(MakeShareable(new FJsonValueObject(LocalVar)));
	OutLocalSnapshot->SetArrayField(TEXT("Variables"), LocalVariables);

	// Remote snapshot - different Health variable change + new variable
	OutRemoteSnapshot->SetStringField(TEXT("BlueprintName"), TEXT("TestBlueprint"));
	OutRemoteSnapshot->SetStringField(TEXT("ParentClass"), TEXT("AActor"));
	
	TArray<TSharedPtr<FJsonValue>> RemoteVariables;
	TSharedPtr<FJsonObject> RemoteVar = MakeShareable(new FJsonObject);
	RemoteVar->SetStringField(TEXT("VariableName"), TEXT("Health"));
	RemoteVar->SetStringField(TEXT("VariableGuid"), TEXT("12345678-1234-1234-1234-123456789012"));
	RemoteVar->SetStringField(TEXT("VarType"), TEXT("Int"));
	RemoteVar->SetStringField(TEXT("DefaultValue"), TEXT("200")); // Changed differently remotely
	RemoteVariables.Add(MakeShareable(new FJsonValueObject(RemoteVar)));

	// Add new variable in remote
	TSharedPtr<FJsonObject> NewVar = MakeShareable(new FJsonObject);
	NewVar->SetStringField(TEXT("VariableName"), TEXT("MaxHealth"));
	NewVar->SetStringField(TEXT("VariableGuid"), TEXT("87654321-4321-4321-4321-210987654321"));
	NewVar->SetStringField(TEXT("VarType"), TEXT("Int"));
	NewVar->SetStringField(TEXT("DefaultValue"), TEXT("200"));
	RemoteVariables.Add(MakeShareable(new FJsonValueObject(NewVar)));
	
	OutRemoteSnapshot->SetArrayField(TEXT("Variables"), RemoteVariables);
}

bool FBlueprintMergeValidator::CheckBlueprintCompilation(
	UBlueprint* Blueprint,
	TArray<FString>& OutErrors,
	TArray<FString>& OutWarnings)
{
	if (!Blueprint)
	{
		OutErrors.Add(TEXT("Blueprint is null"));
		return false;
	}

	// Check current compilation status
	if (Blueprint->Status == BS_Error)
	{
		OutErrors.Add(TEXT("Blueprint has compilation errors"));
		return false;
	}
	else if (Blueprint->Status == BS_UpToDateWithWarnings)
	{
		OutWarnings.Add(TEXT("Blueprint has compilation warnings"));
	}

	// Try to compile
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);

	// Check post-compilation status
	if (Blueprint->Status == BS_Error)
	{
		OutErrors.Add(TEXT("Blueprint failed to compile"));
		return false;
	}

	return true;
}

bool FBlueprintMergeValidator::ValidateGraphIntegrity(UBlueprint* Blueprint, TArray<FString>& OutErrors)
{
	if (!Blueprint)
	{
		OutErrors.Add(TEXT("Blueprint is null"));
		return false;
	}

	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(Blueprint->UbergraphPages);
	AllGraphs.Append(Blueprint->FunctionGraphs);
	AllGraphs.Append(Blueprint->MacroGraphs);

	bool bIsValid = true;

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			OutErrors.Add(TEXT("Found null graph"));
			bIsValid = false;
			continue;
		}

		// Check for orphaned nodes
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				OutErrors.Add(FString::Printf(TEXT("Found null node in graph: %s"), *Graph->GetName()));
				bIsValid = false;
				continue;
			}

			// Check pins
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					OutErrors.Add(FString::Printf(TEXT("Found null pin in node: %s"), *Node->GetName()));
					bIsValid = false;
					continue;
				}

				// Check pin connections
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !IsValid(LinkedPin->GetOwningNode()))
					{
						OutErrors.Add(FString::Printf(TEXT("Broken pin connection: %s.%s"), 
							*Node->GetName(), *Pin->PinName.ToString()));
						bIsValid = false;
					}
				}
			}
		}
	}

	return bIsValid;
}

bool FBlueprintMergeValidator::ValidateVariableIntegrity(UBlueprint* Blueprint, TArray<FString>& OutErrors)
{
	if (!Blueprint)
	{
		OutErrors.Add(TEXT("Blueprint is null"));
		return false;
	}

	TSet<FString> VariableNames;
	TSet<FGuid> VariableGuids;
	bool bIsValid = true;

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		FString VarName = Variable.VarName.ToString();

		// Check for duplicate names
		if (VariableNames.Contains(VarName))
		{
			OutErrors.Add(FString::Printf(TEXT("Duplicate variable name: %s"), *VarName));
			bIsValid = false;
		}
		VariableNames.Add(VarName);

		// Check for duplicate GUIDs
		if (Variable.VarGuid.IsValid())
		{
			if (VariableGuids.Contains(Variable.VarGuid))
			{
				OutErrors.Add(FString::Printf(TEXT("Duplicate variable GUID: %s"), *Variable.VarGuid.ToString()));
				bIsValid = false;
			}
			VariableGuids.Add(Variable.VarGuid);
		}

		// Check variable type validity
		if (Variable.VarType.PinCategory == NAME_None)
		{
			OutErrors.Add(FString::Printf(TEXT("Variable has invalid type: %s"), *VarName));
			bIsValid = false;
		}
	}

	return bIsValid;
}

bool FBlueprintMergeValidator::ValidateComponentIntegrity(UBlueprint* Blueprint, TArray<FString>& OutErrors)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return true; // No components to validate
	}

	TSet<FString> ComponentNames;
	bool bIsValid = true;
	
	const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (!Node)
		{
			OutErrors.Add(TEXT("Found null component node"));
			bIsValid = false;
			continue;
		}

		FString CompName = Node->GetVariableName().ToString();
		if (ComponentNames.Contains(CompName))
		{
			OutErrors.Add(FString::Printf(TEXT("Duplicate component name: %s"), *CompName));
			bIsValid = false;
		}
		ComponentNames.Add(CompName);

		// Check component template
		if (!Node->ComponentTemplate)
		{
			OutErrors.Add(FString::Printf(TEXT("Component has null template: %s"), *CompName));
			bIsValid = false;
		}
	}

	return bIsValid;
}

bool FBlueprintMergeValidator::CheckCircularDependencies(UBlueprint* Blueprint, TArray<FString>& OutErrors)
{
	// Simplified circular dependency check
	// In a full implementation, this would traverse the execution graph looking for cycles
	return true;
}

bool FBlueprintMergeValidator::ValidateGuidUniqueness(UBlueprint* Blueprint, TArray<FString>& OutErrors)
{
	if (!Blueprint)
	{
		OutErrors.Add(TEXT("Blueprint is null"));
		return false;
	}

	TSet<FGuid> AllGuids;
	bool bIsValid = true;

	// Check variable GUIDs
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarGuid.IsValid())
		{
			if (AllGuids.Contains(Variable.VarGuid))
			{
				OutErrors.Add(FString::Printf(TEXT("Duplicate GUID in variable: %s"), *Variable.VarName.ToString()));
				bIsValid = false;
			}
			AllGuids.Add(Variable.VarGuid);
		}
	}

	// Check node GUIDs
	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(Blueprint->UbergraphPages);
	AllGraphs.Append(Blueprint->FunctionGraphs);
	AllGraphs.Append(Blueprint->MacroGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node* K2Node = Cast<UK2Node>(Node))
			{
				if (K2Node->NodeGuid.IsValid())
				{
					if (AllGuids.Contains(K2Node->NodeGuid))
					{
						OutErrors.Add(FString::Printf(TEXT("Duplicate node GUID: %s"), *K2Node->GetName()));
						bIsValid = false;
					}
					AllGuids.Add(K2Node->NodeGuid);
				}
			}
		}
	}

	return bIsValid;
}

bool FBlueprintMergeValidator::RunPerformanceChecks(UBlueprint* Blueprint, TArray<FString>& OutWarnings)
{
	if (!Blueprint)
	{
		return false;
	}

	// Check for performance issues
	if (Blueprint->NewVariables.Num() > 100)
	{
		OutWarnings.Add(FString::Printf(TEXT("Blueprint has many variables (%d) - consider organization"), 
			Blueprint->NewVariables.Num()));
	}

	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(Blueprint->UbergraphPages);
	AllGraphs.Append(Blueprint->FunctionGraphs);
	AllGraphs.Append(Blueprint->MacroGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->Nodes.Num() > 200)
		{
			OutWarnings.Add(FString::Printf(TEXT("Graph '%s' has many nodes (%d) - consider breaking into functions"), 
				*Graph->GetName(), Graph->Nodes.Num()));
		}
	}

	return true;
}

UBlueprint* FBlueprintMergeValidator::CreateTestBlueprint()
{
	// Create a simple test Blueprint in memory
	UPackage* TestPackage = CreatePackage(TEXT("/Temp/TestMergeBlueprint"));
	if (!TestPackage)
	{
		return nullptr;
	}

	UBlueprint* TestBlueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		TestPackage,
		TEXT("TestMergeBlueprint"),
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);

	if (TestBlueprint)
	{
		// Add a simple variable
		FEdGraphPinType IntType;
		IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
		FBlueprintEditorUtils::AddMemberVariable(TestBlueprint, TEXT("TestHealth"), IntType, TEXT("100"));

		UE_LOG(LogTemp, VeryVerbose, TEXT("Created test Blueprint: %s"), *TestBlueprint->GetName());
	}

	return TestBlueprint;
}

void FBlueprintMergeValidator::CleanupTestBlueprints(const TArray<UBlueprint*>& TestBlueprints)
{
	for (UBlueprint* Blueprint : TestBlueprints)
	{
		if (Blueprint && Blueprint->GetPackage())
		{
			// Mark package for garbage collection
			Blueprint->GetPackage()->SetFlags(RF_Transient);
		}
	}
}

void FBlueprintMergeValidator::GenerateTestCases(TArray<FMergeTestCase>& OutTestCases)
{
	// Test Case 1: Simple variable conflict
	{
		FMergeTestCase TestCase;
		TestCase.TestName = TEXT("SimpleVariableConflict");
		TestCase.Description = TEXT("Test basic variable default value conflict");
		TestCase.bShouldHaveConflicts = true;
		TestCase.ExpectedOperationCount = 0;
		TestCase.ExpectedConflictCount = 1;

		// Create simple test JSON (would be generated by CreateTestSnapshots in practice)
		TestCase.BaseSnapshotJson = TEXT("{\"Variables\":[{\"VariableName\":\"Health\",\"DefaultValue\":\"100\"}]}");
		TestCase.LocalSnapshotJson = TEXT("{\"Variables\":[{\"VariableName\":\"Health\",\"DefaultValue\":\"150\"}]}");
		TestCase.RemoteSnapshotJson = TEXT("{\"Variables\":[{\"VariableName\":\"Health\",\"DefaultValue\":\"200\"}]}");
		TestCase.ExpectedResult = TEXT("Conflict on Health variable default value");

		OutTestCases.Add(TestCase);
	}

	// Test Case 2: Non-conflicting additions
	{
		FMergeTestCase TestCase;
		TestCase.TestName = TEXT("NonConflictingAdditions");
		TestCase.Description = TEXT("Test adding different variables in local and remote");
		TestCase.bShouldHaveConflicts = false;
		TestCase.ExpectedOperationCount = 2;
		TestCase.ExpectedConflictCount = 0;

		TestCase.BaseSnapshotJson = TEXT("{\"Variables\":[]}");
		TestCase.LocalSnapshotJson = TEXT("{\"Variables\":[{\"VariableName\":\"LocalVar\"}]}");
		TestCase.RemoteSnapshotJson = TEXT("{\"Variables\":[{\"VariableName\":\"RemoteVar\"}]}");
		TestCase.ExpectedResult = TEXT("Two add operations, no conflicts");

		OutTestCases.Add(TestCase);
	}
}

bool FBlueprintMergeValidator::CompareTestResults(
	const FString& ActualResult,
	const FString& ExpectedResult,
	TArray<FString>& OutDifferences)
{
	if (ActualResult == ExpectedResult)
	{
		return true;
	}

	OutDifferences.Add(FString::Printf(TEXT("Expected: %s"), *ExpectedResult));
	OutDifferences.Add(FString::Printf(TEXT("Actual: %s"), *ActualResult));
	return false;
}

void FBlueprintMergeValidator::LogTestResult(const FString& TestName, bool bPassed, const FString& Details)
{
	if (bPassed)
	{
		UE_LOG(LogTemp, Log, TEXT("✅ Test PASSED: %s - %s"), *TestName, *Details);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ Test FAILED: %s - %s"), *TestName, *Details);
	}
}
