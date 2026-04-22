/**
 * Example test demonstrating Blueprint Merge Tool functionality
 * This file shows how to use the merge tool programmatically
 */

#include "CoreMinimal.h"
#include "../Source/BlueprintMergeTool/Public/SnapshotManager.h"
#include "../Source/BlueprintMergeTool/Public/DiffEngine.h"
#include "../Source/BlueprintMergeTool/Public/MergePlanner.h"
#include "../Source/BlueprintMergeTool/Public/ApplyEngine.h"
#include "../Source/BlueprintMergeTool/Public/BlueprintMergeValidator.h"
#include "Engine/Blueprint.h"

/**
 * Example function showing basic merge workflow
 */
void RunBlueprintMergeExample()
{
    UE_LOG(LogTemp, Log, TEXT("=== Blueprint Merge Tool Example ==="));

    // Step 1: Load three Blueprint versions
    UBlueprint* BaseBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BaseVersion"));
    UBlueprint* LocalBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/LocalVersion"));
    UBlueprint* RemoteBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/RemoteVersion"));

    if (!BaseBlueprint || !LocalBlueprint || !RemoteBlueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load test Blueprints"));
        return;
    }

    // Step 2: Create snapshots
    TSharedPtr<FJsonObject> BaseSnapshot, LocalSnapshot, RemoteSnapshot;
    
    if (!FSnapshotManager::CreateSnapshot(BaseBlueprint, BaseSnapshot) ||
        !FSnapshotManager::CreateSnapshot(LocalBlueprint, LocalSnapshot) ||
        !FSnapshotManager::CreateSnapshot(RemoteBlueprint, RemoteSnapshot))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create snapshots"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Snapshots created successfully"));

    // Step 3: Perform three-way diff
    FDiffResult DiffResult;
    if (!FDiffEngine::PerformThreeWayDiff(BaseSnapshot, LocalSnapshot, RemoteSnapshot, DiffResult))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to perform three-way diff"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Diff completed: %d operations, %d conflicts"), 
        DiffResult.Operations.Num(), DiffResult.Conflicts.Num());

    // Step 4: Create merge plan
    FMergePlannerConfig Config;
    Config.DefaultStrategy = EResolutionStrategy::NonDestructive;
    Config.bAutoResolvePositionConflicts = true;

    FMergePlan MergePlan;
    if (!FMergePlanner::CreateMergePlan(DiffResult, Config, MergePlan))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create merge plan"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Merge plan created: %d auto-resolved, %d manual"), 
        MergePlan.AutoResolvedOperations.Num(), MergePlan.ManualReviewRequired.Num());

    // Step 5: Validate merge plan
    FValidationResult ValidationResult;
    if (!FBlueprintMergeValidator::ValidateMergePlan(MergePlan, LocalBlueprint, ValidationResult))
    {
        UE_LOG(LogTemp, Warning, TEXT("Merge plan validation had issues: %s"), *ValidationResult.ValidationSummary);
    }

    // Step 6: Apply merge (to local Blueprint)
    FApplyResult ApplyResult;
    if (FApplyEngine::ApplyMergePlan(LocalBlueprint, MergePlan, ApplyResult))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Merge applied successfully: %d operations"), 
            ApplyResult.AppliedOperations.Num());

        // Step 7: Validate final result
        FValidationResult FinalValidation;
        if (FBlueprintMergeValidator::ValidateBlueprint(LocalBlueprint, FinalValidation))
        {
            UE_LOG(LogTemp, Log, TEXT("✅ Final Blueprint validation passed"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Final Blueprint validation failed: %s"), 
                *FinalValidation.ValidationSummary);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Merge failed: %s"), *ApplyResult.ErrorMessage);
    }

    UE_LOG(LogTemp, Log, TEXT("=== Blueprint Merge Example Complete ==="));
}

/**
 * Example function showing snapshot comparison
 */
void RunSnapshotComparisonExample()
{
    UE_LOG(LogTemp, Log, TEXT("=== Snapshot Comparison Example ==="));

    // Load a Blueprint
    UBlueprint* TestBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/TestBlueprint"));
    if (!TestBlueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load test Blueprint"));
        return;
    }

    // Create snapshot
    TSharedPtr<FJsonObject> Snapshot1;
    if (!FSnapshotManager::CreateSnapshot(TestBlueprint, Snapshot1))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create snapshot"));
        return;
    }

    // Serialize to string
    FString JsonString;
    if (FSnapshotManager::SerializeToString(Snapshot1, JsonString))
    {
        UE_LOG(LogTemp, Log, TEXT("Snapshot JSON (%d chars):"), JsonString.Len());
        UE_LOG(LogTemp, Log, TEXT("%s"), *JsonString.Left(500)); // First 500 chars

        // Test hash generation
        FString Hash = FSnapshotManager::GenerateSnapshotHash(Snapshot1);
        UE_LOG(LogTemp, Log, TEXT("Snapshot hash: %s"), *Hash);
    }

    // Create a second snapshot and verify determinism
    TSharedPtr<FJsonObject> Snapshot2;
    if (FSnapshotManager::CreateSnapshot(TestBlueprint, Snapshot2))
    {
        FString JsonString2;
        FSnapshotManager::SerializeToString(Snapshot2, JsonString2);
        
        if (JsonString == JsonString2)
        {
            UE_LOG(LogTemp, Log, TEXT("✅ Snapshots are deterministic"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ Snapshots are not deterministic"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("=== Snapshot Comparison Complete ==="));
}

/**
 * Example function showing validation and testing
 */
void RunValidationExample()
{
    UE_LOG(LogTemp, Log, TEXT("=== Validation Example ==="));

    // Run smoke tests
    FValidationResult SmokeTestResult;
    if (FBlueprintMergeValidator::RunSmokeTests(SmokeTestResult))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Smoke tests passed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Smoke tests failed: %s"), *SmokeTestResult.ValidationSummary);
        
        for (const FString& Error : SmokeTestResult.Errors)
        {
            UE_LOG(LogTemp, Error, TEXT("  Error: %s"), *Error);
        }
    }

    // Run unit tests
    FValidationResult UnitTestResult;
    if (FBlueprintMergeValidator::RunUnitTests(UnitTestResult))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Unit tests passed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Unit tests failed: %s"), *UnitTestResult.ValidationSummary);
    }

    UE_LOG(LogTemp, Log, TEXT("=== Validation Example Complete ==="));
}

/**
 * Console command to run the examples
 */
static FAutoConsoleCommand RunMergeExampleCommand(
    TEXT("BlueprintMergeTool.RunExample"),
    TEXT("Run Blueprint Merge Tool examples"),
    FConsoleCommandDelegate::CreateStatic([]()
    {
        RunBlueprintMergeExample();
        RunSnapshotComparisonExample();
        RunValidationExample();
    })
);

/**
 * Console command to run just smoke tests
 */
static FAutoConsoleCommand RunSmokeTestsCommand(
    TEXT("BlueprintMergeTool.RunSmokeTests"),
    TEXT("Run Blueprint Merge Tool smoke tests"),
    FConsoleCommandDelegate::CreateStatic([]()
    {
        FValidationResult Result;
        if (FBlueprintMergeValidator::RunSmokeTests(Result))
        {
            UE_LOG(LogTemp, Log, TEXT("All smoke tests passed!"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Smoke tests failed: %s"), *Result.ValidationSummary);
        }
    })
);
