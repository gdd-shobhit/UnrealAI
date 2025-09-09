#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "Engine/Blueprint.h"
#include "DiffEngine.h"
#include "MergePlanner.h"
#include "BlueprintMergeValidator.generated.h"

/**
 * Validation result for Blueprint integrity checks
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FValidationResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsValid;

	UPROPERTY()
	TArray<FString> Errors;

	UPROPERTY()
	TArray<FString> Warnings;

	UPROPERTY()
	TArray<FString> InfoMessages;

	UPROPERTY()
	bool bCompilesSuccessfully;

	UPROPERTY()
	FString ValidationSummary;

	FValidationResult()
		: bIsValid(true)
		, bCompilesSuccessfully(false)
	{
	}
};

/**
 * Test case for Blueprint merge operations
 */
USTRUCT(BlueprintType)
struct BLUEPRINTMERGETOOL_API FMergeTestCase
{
	GENERATED_BODY()

	UPROPERTY()
	FString TestName;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	FString BaseSnapshotJson;

	UPROPERTY()
	FString LocalSnapshotJson;

	UPROPERTY()
	FString RemoteSnapshotJson;

	UPROPERTY()
	FString ExpectedResult;

	UPROPERTY()
	bool bShouldHaveConflicts;

	UPROPERTY()
	int32 ExpectedOperationCount;

	UPROPERTY()
	int32 ExpectedConflictCount;
};

/**
 * Comprehensive validation and testing for Blueprint merge operations
 * Includes integrity checks, smoke tests, and unit tests
 */
class BLUEPRINTMERGETOOL_API FBlueprintMergeValidator
{
public:
	/**
	 * Run comprehensive validation on a Blueprint
	 * @param Blueprint Blueprint to validate
	 * @param OutResult Validation result
	 * @return True if validation completed (check bIsValid for actual result)
	 */
	static bool ValidateBlueprint(UBlueprint* Blueprint, FValidationResult& OutResult);

	/**
	 * Run smoke tests on Blueprint merge functionality
	 * @param OutResult Test results
	 * @return True if all smoke tests passed
	 */
	static bool RunSmokeTests(FValidationResult& OutResult);

	/**
	 * Run unit tests for merge components
	 * @param OutResult Test results
	 * @return True if all unit tests passed
	 */
	static bool RunUnitTests(FValidationResult& OutResult);

	/**
	 * Validate a merge plan before execution
	 * @param MergePlan Plan to validate
	 * @param TargetBlueprint Blueprint the plan will be applied to
	 * @param OutResult Validation result
	 * @return True if validation completed
	 */
	static bool ValidateMergePlan(
		const FMergePlan& MergePlan, 
		UBlueprint* TargetBlueprint,
		FValidationResult& OutResult
	);

	/**
	 * Test snapshot creation and deterministic ordering
	 * @param TestBlueprint Blueprint to test with
	 * @param OutResult Test result
	 * @return True if test passed
	 */
	static bool TestSnapshotDeterminism(UBlueprint* TestBlueprint, FValidationResult& OutResult);

	/**
	 * Test three-way diff functionality
	 * @param OutResult Test result
	 * @return True if test passed
	 */
	static bool TestThreeWayDiff(FValidationResult& OutResult);

	/**
	 * Test merge planner with various conflict scenarios
	 * @param OutResult Test result
	 * @return True if test passed
	 */
	static bool TestMergePlanner(FValidationResult& OutResult);

	/**
	 * Test apply engine operations
	 * @param OutResult Test result
	 * @return True if test passed
	 */
	static bool TestApplyEngine(FValidationResult& OutResult);

	/**
	 * Run a specific test case
	 * @param TestCase Test case to run
	 * @param OutResult Test result
	 * @return True if test passed
	 */
	static bool RunTestCase(const FMergeTestCase& TestCase, FValidationResult& OutResult);

	/**
	 * Create test snapshots for testing purposes
	 * @param OutBaseSnapshot Base test snapshot
	 * @param OutLocalSnapshot Local test snapshot
	 * @param OutRemoteSnapshot Remote test snapshot
	 */
	static void CreateTestSnapshots(
		TSharedPtr<FJsonObject>& OutBaseSnapshot,
		TSharedPtr<FJsonObject>& OutLocalSnapshot,
		TSharedPtr<FJsonObject>& OutRemoteSnapshot
	);

private:
	/**
	 * Check Blueprint compilation status
	 * @param Blueprint Blueprint to check
	 * @param OutErrors Compilation errors
	 * @param OutWarnings Compilation warnings
	 * @return True if compiles without errors
	 */
	static bool CheckBlueprintCompilation(
		UBlueprint* Blueprint,
		TArray<FString>& OutErrors,
		TArray<FString>& OutWarnings
	);

	/**
	 * Validate Blueprint graph integrity
	 * @param Blueprint Blueprint to validate
	 * @param OutErrors Graph integrity errors
	 * @return True if graphs are valid
	 */
	static bool ValidateGraphIntegrity(UBlueprint* Blueprint, TArray<FString>& OutErrors);

	/**
	 * Validate variable integrity
	 * @param Blueprint Blueprint to validate
	 * @param OutErrors Variable errors
	 * @return True if variables are valid
	 */
	static bool ValidateVariableIntegrity(UBlueprint* Blueprint, TArray<FString>& OutErrors);

	/**
	 * Validate component hierarchy integrity
	 * @param Blueprint Blueprint to validate
	 * @param OutErrors Component errors
	 * @return True if components are valid
	 */
	static bool ValidateComponentIntegrity(UBlueprint* Blueprint, TArray<FString>& OutErrors);

	/**
	 * Check for circular dependencies in graphs
	 * @param Blueprint Blueprint to check
	 * @param OutErrors Dependency errors
	 * @return True if no circular dependencies found
	 */
	static bool CheckCircularDependencies(UBlueprint* Blueprint, TArray<FString>& OutErrors);

	/**
	 * Validate GUID uniqueness across the Blueprint
	 * @param Blueprint Blueprint to validate
	 * @param OutErrors GUID errors
	 * @return True if all GUIDs are unique
	 */
	static bool ValidateGuidUniqueness(UBlueprint* Blueprint, TArray<FString>& OutErrors);

	/**
	 * Run performance checks on the Blueprint
	 * @param Blueprint Blueprint to check
	 * @param OutWarnings Performance warnings
	 * @return True if performance is acceptable
	 */
	static bool RunPerformanceChecks(UBlueprint* Blueprint, TArray<FString>& OutWarnings);

	/**
	 * Create a simple test Blueprint for testing
	 * @return Test Blueprint or nullptr if creation failed
	 */
	static UBlueprint* CreateTestBlueprint();

	/**
	 * Clean up test Blueprints
	 * @param TestBlueprints Blueprints to clean up
	 */
	static void CleanupTestBlueprints(const TArray<UBlueprint*>& TestBlueprints);

	/**
	 * Generate test data for merge operations
	 * @param OutTestCases Generated test cases
	 */
	static void GenerateTestCases(TArray<FMergeTestCase>& OutTestCases);

	/**
	 * Compare actual vs expected results
	 * @param ActualResult Actual test result
	 * @param ExpectedResult Expected test result
	 * @param OutDifferences Differences found
	 * @return True if results match
	 */
	static bool CompareTestResults(
		const FString& ActualResult,
		const FString& ExpectedResult,
		TArray<FString>& OutDifferences
	);

	/**
	 * Log test results in a formatted way
	 * @param TestName Name of the test
	 * @param bPassed Whether the test passed
	 * @param Details Additional test details
	 */
	static void LogTestResult(const FString& TestName, bool bPassed, const FString& Details = TEXT(""));
};
