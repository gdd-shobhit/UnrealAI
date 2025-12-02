#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "Engine/Blueprint.h"
#include "Dom/JsonObject.h"

/**
 * Perforce integration adapter for Blueprint Merge Tool
 * Handles:
 * - Detecting Perforce conflicts
 * - Fetching BASE, LOCAL, REMOTE versions
 * - Managing Perforce file operations (checkout, resolve, submit)
 * - Automatic merge triggering
 */
class BLUEPRINTMERGETOOL_API FPerforceAdapter
{
public:
	/**
	 * Check if Perforce is available and configured
	 * @return True if Perforce is available
	 */
	static bool IsPerforceAvailable();

	/**
	 * Get the current source control provider
	 * @return Source control provider, or nullptr if not available
	 */
	static ISourceControlProvider* GetSourceControlProvider();

	/**
	 * Check if a file is managed by Perforce
	 * @param FilePath Absolute file path to check
	 * @return True if file is managed by Perforce
	 */
	static bool IsFileManagedByPerforce(const FString& FilePath);

	/**
	 * Check if a Blueprint has conflicts in Perforce
	 * @param BlueprintPath Blueprint asset path (e.g., "/Game/Path/BP_MyBlueprint")
	 * @param OutConflictInfo Output conflict information
	 * @return True if conflicts detected
	 */
	static bool HasPerforceConflicts(const FString& BlueprintPath, FString& OutConflictInfo);

	/**
	 * Get the file state from Perforce
	 * @param FilePath Absolute file path
	 * @param OutIsConflicted Output whether file is conflicted
	 * @param OutIsCheckedOut Output whether file is checked out
	 * @return True if file state retrieved successfully
	 */
	static bool GetFileState(const FString& FilePath, bool& OutIsConflicted, bool& OutIsCheckedOut);

	/**
	 * Get BASE version of a Blueprint from Perforce
	 * @param BlueprintPath Blueprint asset path
	 * @param OutBlueprint Output Blueprint object
	 * @param OutSnapshot Output snapshot JSON
	 * @return True if BASE version loaded successfully
	 */
	static bool GetBaseVersion(const FString& BlueprintPath, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot);

	/**
	 * Get LOCAL version of a Blueprint (current working copy)
	 * @param BlueprintPath Blueprint asset path
	 * @param OutBlueprint Output Blueprint object
	 * @param OutSnapshot Output snapshot JSON
	 * @return True if LOCAL version loaded successfully
	 */
	static bool GetLocalVersion(const FString& BlueprintPath, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot);

	/**
	 * Get REMOTE version of a Blueprint from Perforce
	 * @param BlueprintPath Blueprint asset path
	 * @param OutBlueprint Output Blueprint object
	 * @param OutSnapshot Output snapshot JSON
	 * @return True if REMOTE version loaded successfully
	 */
	static bool GetRemoteVersion(const FString& BlueprintPath, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot);

	/**
	 * Checkout a file in Perforce
	 * @param FilePath Absolute file path
	 * @param OutError Error message if failed
	 * @return True if checkout successful
	 */
	static bool CheckoutFile(const FString& FilePath, FString& OutError);

	/**
	 * Resolve a conflict in Perforce
	 * @param FilePath Absolute file path
	 * @param ResolveMethod Resolution method (AcceptYours, AcceptTheirs, etc.)
	 * @param OutError Error message if failed
	 * @return True if resolve successful
	 */
	static bool ResolveConflict(const FString& FilePath, EPerforceResolveMethod ResolveMethod, FString& OutError);

	/**
	 * Submit changes to Perforce
	 * @param FilePaths Array of file paths to submit
	 * @param Description Submit description
	 * @param OutError Error message if failed
	 * @return True if submit successful
	 */
	static bool SubmitFiles(const TArray<FString>& FilePaths, const FString& Description, FString& OutError);

	/**
	 * Automatically detect and load conflicted Blueprints from Perforce
	 * @param OutConflictedBlueprints Output array of conflicted Blueprint paths
	 * @return Number of conflicts found
	 */
	static int32 DetectConflictedBlueprints(TArray<FString>& OutConflictedBlueprints);

	/**
	 * Load all three versions (BASE, LOCAL, REMOTE) for a conflicted Blueprint
	 * @param BlueprintPath Blueprint asset path
	 * @param OutBaseBlueprint Output BASE Blueprint
	 * @param OutLocalBlueprint Output LOCAL Blueprint
	 * @param OutRemoteBlueprint Output REMOTE Blueprint
	 * @param OutBaseSnapshot Output BASE snapshot
	 * @param OutLocalSnapshot Output LOCAL snapshot
	 * @param OutRemoteSnapshot Output REMOTE snapshot
	 * @param OutError Error message if failed
	 * @return True if all versions loaded successfully
	 */
	static bool LoadAllVersions(
		const FString& BlueprintPath,
		UBlueprint*& OutBaseBlueprint,
		UBlueprint*& OutLocalBlueprint,
		UBlueprint*& OutRemoteBlueprint,
		TSharedPtr<FJsonObject>& OutBaseSnapshot,
		TSharedPtr<FJsonObject>& OutLocalSnapshot,
		TSharedPtr<FJsonObject>& OutRemoteSnapshot,
		FString& OutError
	);

	/**
	 * Convert asset path to absolute file path
	 * @param AssetPath Asset path (e.g., "/Game/Path/BP_MyBlueprint")
	 * @return Absolute file path, or empty string if conversion failed
	 */
	static FString AssetPathToFilePath(const FString& AssetPath);

	/**
	 * Convert absolute file path to asset path
	 * @param FilePath Absolute file path
	 * @return Asset path, or empty string if conversion failed
	 */
	static FString FilePathToAssetPath(const FString& FilePath);

	/**
	 * Get Perforce depot path for a file
	 * @param FilePath Absolute file path
	 * @return Depot path, or empty string if not available
	 */
	static FString GetDepotPath(const FString& FilePath);

	/**
	 * Revert a file in Perforce (discard local changes)
	 * @param FilePath Absolute file path
	 * @param OutError Error message if failed
	 * @return True if revert successful
	 */
	static bool RevertFile(const FString& FilePath, FString& OutError);

private:
	/**
	 * Load a Blueprint from a specific Perforce version
	 * @param FilePath Absolute file path
	 * @param Version Perforce version string (e.g., "BASE", "HEAD", or revision number)
	 * @param OutBlueprint Output Blueprint object
	 * @param OutSnapshot Output snapshot JSON
	 * @param OutError Error message if failed
	 * @return True if loaded successfully
	 */
	static bool LoadBlueprintFromVersion(const FString& FilePath, const FString& Version, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot, FString& OutError);

	/**
	 * Get file path for a specific Perforce version
	 * @param FilePath Absolute file path
	 * @param Version Perforce version string
	 * @return Absolute file path to version file, or empty if failed
	 */
	static FString GetVersionFilePath(const FString& FilePath, const FString& Version);

	/**
	 * Check if a file is a Blueprint file
	 * @param FilePath Absolute file path
	 * @return True if file is a Blueprint (.uasset)
	 */
	static bool IsBlueprintFile(const FString& FilePath);

	/**
	 * Get temporary directory for storing version files
	 * @return Temporary directory path
	 */
	static FString GetTempDirectory();
};

/**
 * Perforce resolve method enumeration
 */
enum class EPerforceResolveMethod
{
	AcceptYours,      // Accept local changes
	AcceptTheirs,     // Accept remote changes
	AcceptMerge,      // Accept merged result (after manual merge)
	Ignore            // Ignore the conflict
};

