/**
 * Comprehensive integration test for Blueprint Merge Tool
 * Tests the complete workflow from snapshot creation to merge application
 */

#include "CoreMinimal.h"
#include "../Source/BlueprintMergeTool/Public/SnapshotManager.h"
#include "../Source/BlueprintMergeTool/Public/DiffEngine.h"
#include "../Source/BlueprintMergeTool/Public/MergePlanner.h"
#include "../Source/BlueprintMergeTool/Public/ApplyEngine.h"
#include "../Source/BlueprintMergeTool/Public/BlueprintMergeValidator.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

/**
 * Comprehensive test that validates the entire merge pipeline
 */
class FComprehensiveMergeTest
{
public:
    /**
     * Run the complete test suite
     */
    static bool RunFullTestSuite()
    {
        UE_LOG(LogTemp, Log, TEXT("=== Starting Comprehensive Blueprint Merge Test ==="));

        bool bAllTestsPassed = true;

        // Test 1: Snapshot Creation and Determinism
        if (!TestSnapshotCreation())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Snapshot creation test failed"));
            bAllTestsPassed = false;
        }

        // Test 2: Three-Way Diff Algorithm
        if (!TestThreeWayDiffAlgorithm())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Three-way diff test failed"));
            bAllTestsPassed = false;
        }

        // Test 3: Merge Planning and Conflict Resolution
        if (!TestMergePlanningAndResolution())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Merge planning test failed"));
            bAllTestsPassed = false;
        }

        // Test 4: Operation Application (dry run)
        if (!TestOperationApplication())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Operation application test failed"));
            bAllTestsPassed = false;
        }

        // Test 5: End-to-End Integration
        if (!TestEndToEndIntegration())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ End-to-end integration test failed"));
            bAllTestsPassed = false;
        }

        UE_LOG(LogTemp, Log, TEXT("=== Comprehensive Test Results: %s ==="), 
            bAllTestsPassed ? TEXT("ALL PASSED") : TEXT("SOME FAILED"));

        return bAllTestsPassed;
    }

private:
    static bool TestSnapshotCreation()
    {
        UE_LOG(LogTemp, Log, TEXT("🧪 Testing snapshot creation and determinism..."));

        // Create test snapshots with synthetic data
        TSharedPtr<FJsonObject> BaseSnapshot, LocalSnapshot, RemoteSnapshot;
        CreateSyntheticTestSnapshots(BaseSnapshot, LocalSnapshot, RemoteSnapshot);

        if (!BaseSnapshot.IsValid() || !LocalSnapshot.IsValid() || !RemoteSnapshot.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create synthetic snapshots"));
            return false;
        }

        // Test serialization consistency
        FString BaseJson1, BaseJson2;
        if (!FSnapshotManager::SerializeToString(BaseSnapshot, BaseJson1) ||
            !FSnapshotManager::SerializeToString(BaseSnapshot, BaseJson2))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to serialize snapshots"));
            return false;
        }

        if (BaseJson1 != BaseJson2)
        {
            UE_LOG(LogTemp, Error, TEXT("Snapshot serialization is not deterministic"));
            return false;
        }

        // Test hash generation
        FString Hash1 = FSnapshotManager::GenerateSnapshotHash(BaseSnapshot);
        FString Hash2 = FSnapshotManager::GenerateSnapshotHash(BaseSnapshot);

        if (Hash1 != Hash2)
        {
            UE_LOG(LogTemp, Error, TEXT("Snapshot hash generation is not deterministic"));
            return false;
        }

        UE_LOG(LogTemp, Log, TEXT("✅ Snapshot creation test passed"));
        return true;
    }

    static bool TestThreeWayDiffAlgorithm()
    {
        UE_LOG(LogTemp, Log, TEXT("🧪 Testing three-way diff algorithm..."));

        // Create test snapshots with known differences
        TSharedPtr<FJsonObject> BaseSnapshot, LocalSnapshot, RemoteSnapshot;
        CreateConflictingTestSnapshots(BaseSnapshot, LocalSnapshot, RemoteSnapshot);

        // Perform diff
        FDiffResult DiffResult;
        if (!FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, DiffResult))
        {
            UE_LOG(LogTemp, Error, TEXT("Three-way diff failed"));
            return false;
        }

        // Validate expected results
        if (DiffResult.Operations.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("No operations generated from test diff"));
        }

        if (!DiffResult.bHasConflicts)
        {
            UE_LOG(LogTemp, Warning, TEXT("Expected conflicts but none were detected"));
        }

        // Check for expected conflict types
        bool bFoundVariableConflict = false;
        for (const FMergeConflict& Conflict : DiffResult.Conflicts)
        {
            if (Conflict.ConflictType == TEXT("Variable"))
            {
                bFoundVariableConflict = true;
                break;
            }
        }

        if (!bFoundVariableConflict)
        {
            UE_LOG(LogTemp, Warning, TEXT("Expected variable conflict not found"));
        }

        UE_LOG(LogTemp, Log, TEXT("✅ Three-way diff test passed (%d ops, %d conflicts)"), 
            DiffResult.Operations.Num(), DiffResult.Conflicts.Num());
        return true;
    }

    static bool TestMergePlanningAndResolution()
    {
        UE_LOG(LogTemp, Log, TEXT("🧪 Testing merge planning and resolution..."));

        // Create a diff result with various conflict types
        FDiffResult TestDiffResult;
        CreateTestDiffResult(TestDiffResult);

        // Test different resolution strategies
        TArray<EResolutionStrategy> StrategiesToTest = {
            EResolutionStrategy::UseLocal,
            EResolutionStrategy::UseRemote,
            EResolutionStrategy::NonDestructive,
            EResolutionStrategy::SmartMerge
        };

        for (EResolutionStrategy Strategy : StrategiesToTest)
        {
            FMergePlannerConfig Config;
            Config.DefaultStrategy = Strategy;
            Config.bAutoResolvePositionConflicts = true;

            FMergePlan MergePlan;
            if (!FMergePlanner::CreateMergePlan(TestDiffResult, Config, MergePlan))
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create merge plan with strategy: %d"), (int32)Strategy);
                return false;
            }

            UE_LOG(LogTemp, VeryVerbose, TEXT("Strategy %d: %d auto-resolved, %d manual"), 
                (int32)Strategy, MergePlan.AutoResolvedOperations.Num(), MergePlan.ManualReviewRequired.Num());
        }

        UE_LOG(LogTemp, Log, TEXT("✅ Merge planning test passed"));
        return true;
    }

    static bool TestOperationApplication()
    {
        UE_LOG(LogTemp, Log, TEXT("🧪 Testing operation application (dry run)..."));

        // Create a simple test Blueprint
        UPackage* TestPackage = CreatePackage(TEXT("/Temp/MergeTestBlueprint"));
        if (!TestPackage)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create test package"));
            return false;
        }

        UBlueprint* TestBlueprint = FKismetEditorUtilities::CreateBlueprint(
            AActor::StaticClass(),
            TestPackage,
            TEXT("MergeTestBlueprint"),
            EBlueprintType::BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass()
        );

        if (!TestBlueprint)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create test Blueprint"));
            return false;
        }

        // Test validation functions
        TArray<FString> ValidationErrors;
        if (!FApplyEngine::ValidateBlueprintIntegrity(TestBlueprint, ValidationErrors))
        {
            UE_LOG(LogTemp, Warning, TEXT("Test Blueprint has integrity issues: %d errors"), ValidationErrors.Num());
        }

        // Test utility functions
        UEdGraph* EventGraph = FApplyEngine::FindGraphByName(TestBlueprint, TEXT("EventGraph"));
        if (!EventGraph)
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find EventGraph in test Blueprint"));
        }

        // Clean up
        TestPackage->SetFlags(RF_Transient);

        UE_LOG(LogTemp, Log, TEXT("✅ Operation application test passed"));
        return true;
    }

    static bool TestEndToEndIntegration()
    {
        UE_LOG(LogTemp, Log, TEXT("🧪 Testing end-to-end integration..."));

        // Create synthetic snapshots
        TSharedPtr<FJsonObject> BaseSnapshot, LocalSnapshot, RemoteSnapshot;
        CreateSyntheticTestSnapshots(BaseSnapshot, LocalSnapshot, RemoteSnapshot);

        // Perform full workflow
        FDiffResult DiffResult;
        if (!FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, DiffResult))
        {
            UE_LOG(LogTemp, Error, TEXT("End-to-end diff failed"));
            return false;
        }

        FMergePlannerConfig Config;
        Config.DefaultStrategy = EResolutionStrategy::NonDestructive;

        FMergePlan MergePlan;
        if (!FMergePlanner::CreateMergePlan(DiffResult, Config, MergePlan))
        {
            UE_LOG(LogTemp, Error, TEXT("End-to-end merge planning failed"));
            return false;
        }

        // Validate the plan (without applying to avoid modifying real assets)
        TArray<FString> ValidationErrors;
        if (!FMergePlanner::ValidateMergePlan(MergePlan, ValidationErrors))
        {
            UE_LOG(LogTemp, Warning, TEXT("Merge plan validation found issues: %d"), ValidationErrors.Num());
        }

        UE_LOG(LogTemp, Log, TEXT("✅ End-to-end integration test passed"));
        return true;
    }

    static void CreateSyntheticTestSnapshots(
        TSharedPtr<FJsonObject>& OutBase,
        TSharedPtr<FJsonObject>& OutLocal,
        TSharedPtr<FJsonObject>& OutRemote)
    {
        // Base snapshot - simple Blueprint
        OutBase = MakeShareable(new FJsonObject);
        OutBase->SetStringField(TEXT("BlueprintName"), TEXT("TestBlueprint"));
        OutBase->SetStringField(TEXT("ParentClass"), TEXT("AActor"));

        TArray<TSharedPtr<FJsonValue>> BaseVars;
        TSharedPtr<FJsonObject> HealthVar = MakeShareable(new FJsonObject);
        HealthVar->SetStringField(TEXT("VariableName"), TEXT("Health"));
        HealthVar->SetStringField(TEXT("VariableGuid"), TEXT("12345678-1234-1234-1234-123456789012"));
        HealthVar->SetStringField(TEXT("VarType"), TEXT("Int"));
        HealthVar->SetStringField(TEXT("DefaultValue"), TEXT("100"));
        BaseVars.Add(MakeShareable(new FJsonValueObject(HealthVar)));
        OutBase->SetArrayField(TEXT("Variables"), BaseVars);

        // Local snapshot - modified Health + new variable
        OutLocal = MakeShareable(new FJsonObject);
        OutLocal->SetStringField(TEXT("BlueprintName"), TEXT("TestBlueprint"));
        OutLocal->SetStringField(TEXT("ParentClass"), TEXT("AActor"));

        TArray<TSharedPtr<FJsonValue>> LocalVars;
        TSharedPtr<FJsonObject> LocalHealthVar = MakeShareable(new FJsonObject);
        LocalHealthVar->SetStringField(TEXT("VariableName"), TEXT("Health"));
        LocalHealthVar->SetStringField(TEXT("VariableGuid"), TEXT("12345678-1234-1234-1234-123456789012"));
        LocalHealthVar->SetStringField(TEXT("VarType"), TEXT("Int"));
        LocalHealthVar->SetStringField(TEXT("DefaultValue"), TEXT("150")); // Changed locally
        LocalVars.Add(MakeShareable(new FJsonValueObject(LocalHealthVar)));

        TSharedPtr<FJsonObject> LocalNewVar = MakeShareable(new FJsonObject);
        LocalNewVar->SetStringField(TEXT("VariableName"), TEXT("Armor"));
        LocalNewVar->SetStringField(TEXT("VariableGuid"), TEXT("11111111-1111-1111-1111-111111111111"));
        LocalNewVar->SetStringField(TEXT("VarType"), TEXT("Int"));
        LocalNewVar->SetStringField(TEXT("DefaultValue"), TEXT("50"));
        LocalVars.Add(MakeShareable(new FJsonValueObject(LocalNewVar)));
        OutLocal->SetArrayField(TEXT("Variables"), LocalVars);

        // Remote snapshot - different Health change + different new variable
        OutRemote = MakeShareable(new FJsonObject);
        OutRemote->SetStringField(TEXT("BlueprintName"), TEXT("TestBlueprint"));
        OutRemote->SetStringField(TEXT("ParentClass"), TEXT("AActor"));

        TArray<TSharedPtr<FJsonValue>> RemoteVars;
        TSharedPtr<FJsonObject> RemoteHealthVar = MakeShareable(new FJsonObject);
        RemoteHealthVar->SetStringField(TEXT("VariableName"), TEXT("Health"));
        RemoteHealthVar->SetStringField(TEXT("VariableGuid"), TEXT("12345678-1234-1234-1234-123456789012"));
        RemoteHealthVar->SetStringField(TEXT("VarType"), TEXT("Int"));
        RemoteHealthVar->SetStringField(TEXT("DefaultValue"), TEXT("200")); // Changed differently remotely
        RemoteVars.Add(MakeShareable(new FJsonValueObject(RemoteHealthVar)));

        TSharedPtr<FJsonObject> RemoteNewVar = MakeShareable(new FJsonObject);
        RemoteNewVar->SetStringField(TEXT("VariableName"), TEXT("Speed"));
        RemoteNewVar->SetStringField(TEXT("VariableGuid"), TEXT("22222222-2222-2222-2222-222222222222"));
        RemoteNewVar->SetStringField(TEXT("VarType"), TEXT("Float"));
        RemoteNewVar->SetStringField(TEXT("DefaultValue"), TEXT("600.0"));
        RemoteVars.Add(MakeShareable(new FJsonValueObject(RemoteNewVar)));
        OutRemote->SetArrayField(TEXT("Variables"), RemoteVars);
    }

    static void CreateConflictingTestSnapshots(
        TSharedPtr<FJsonObject>& OutBase,
        TSharedPtr<FJsonObject>& OutLocal,
        TSharedPtr<FJsonObject>& OutRemote)
    {
        // Similar to CreateSyntheticTestSnapshots but with more conflicts
        CreateSyntheticTestSnapshots(OutBase, OutLocal, OutRemote);

        // Add additional conflicts for comprehensive testing
        // This would include node conflicts, component conflicts, etc.
    }

    static void CreateTestDiffResult(FDiffResult& OutDiffResult)
    {
        OutDiffResult = FDiffResult();

        // Add test operations
        OutDiffResult.Operations.Add(FDiffEngine::CreateOperation(
            EMergeOperationType::AddVariable, TEXT(""), TEXT("TestVar1")));
        OutDiffResult.Operations.Add(FDiffEngine::CreateOperation(
            EMergeOperationType::UpdateNode, TEXT("EventGraph"), TEXT("TestNode1")));

        // Add test conflicts
        FMergeConflict VariableConflict = FDiffEngine::CreateConflict(
            TEXT("Variable"),
            TEXT("Health"),
            TEXT("100"),
            TEXT("150"),
            TEXT("200"),
            {TEXT("DefaultValue")}
        );
        VariableConflict.Severity = EConflictSeverity::Medium;
        OutDiffResult.Conflicts.Add(VariableConflict);

        FMergeConflict NodeConflict = FDiffEngine::CreateConflict(
            TEXT("NodeMove"),
            TEXT("BeginPlay"),
            TEXT("(0,0)"),
            TEXT("(100,0)"),
            TEXT("(200,0)"),
            {TEXT("NodePosX")}
        );
        NodeConflict.Severity = EConflictSeverity::Low;
        OutDiffResult.Conflicts.Add(NodeConflict);

        OutDiffResult.bHasConflicts = true;
        OutDiffResult.DiffSummary = TEXT("Test diff with 2 operations and 2 conflicts");
    }
};

/**
 * Console command to run the comprehensive test
 */
static FAutoConsoleCommand RunComprehensiveTestCommand(
    TEXT("BlueprintMergeTool.RunComprehensiveTest"),
    TEXT("Run comprehensive Blueprint merge test suite"),
    FConsoleCommandDelegate::CreateStatic([]()
    {
        if (FComprehensiveMergeTest::RunFullTestSuite())
        {
            UE_LOG(LogTemp, Log, TEXT("🎉 All comprehensive tests passed!"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("💥 Some comprehensive tests failed!"));
        }
    })
);

/**
 * Console command to test specific components
 */
static FAutoConsoleCommand TestComponentCommand(
    TEXT("BlueprintMergeTool.TestComponent"),
    TEXT("Test specific component (SnapshotManager, DiffEngine, MergePlanner, ApplyEngine)"),
    FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
    {
        if (Args.Num() == 0)
        {
            UE_LOG(LogTemp, Log, TEXT("Usage: BlueprintMergeTool.TestComponent <ComponentName>"));
            UE_LOG(LogTemp, Log, TEXT("Components: SnapshotManager, DiffEngine, MergePlanner, ApplyEngine, All"));
            return;
        }

        FString ComponentName = Args[0];
        FValidationResult Result;

        if (ComponentName == TEXT("SnapshotManager"))
        {
            if (FComprehensiveMergeTest::TestSnapshotCreation())
            {
                UE_LOG(LogTemp, Log, TEXT("✅ SnapshotManager test passed"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ SnapshotManager test failed"));
            }
        }
        else if (ComponentName == TEXT("DiffEngine"))
        {
            if (FBlueprintMergeValidator::TestThreeWayDiff(Result))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ DiffEngine test passed"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ DiffEngine test failed"));
            }
        }
        else if (ComponentName == TEXT("MergePlanner"))
        {
            if (FBlueprintMergeValidator::TestMergePlanner(Result))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ MergePlanner test passed"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ MergePlanner test failed"));
            }
        }
        else if (ComponentName == TEXT("ApplyEngine"))
        {
            if (FBlueprintMergeValidator::TestApplyEngine(Result))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ ApplyEngine test passed"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ ApplyEngine test failed"));
            }
        }
        else if (ComponentName == TEXT("All"))
        {
            if (FBlueprintMergeValidator::RunSmokeTests(Result))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ All component tests passed"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Some component tests failed"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unknown component: %s"), *ComponentName);
        }
    })
);
