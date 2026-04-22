#include "../Public/PerforceAdapter.h"
#include "SourceControlHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "SnapshotManager.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Engine/Blueprint.h"

bool FPerforceAdapter::IsPerforceAvailable()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	ISourceControlProvider& Provider = SourceControlModule.GetProvider();

	if (!Provider.IsEnabled())
	{
		return false;
	}

	// Check if the provider is Perforce
	FName ProviderName = Provider.GetName();
	return ProviderName == "Perforce" || ProviderName == "P4";
}

ISourceControlProvider* FPerforceAdapter::GetSourceControlProvider()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	ISourceControlProvider& Provider = SourceControlModule.GetProvider();
	if (Provider.IsEnabled())
	{
		return &Provider;
	}
	return nullptr;
}

bool FPerforceAdapter::IsFileManagedByPerforce(const FString& FilePath)
{
	if (!IsPerforceAvailable())
	{
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		return false;
	}

	// Get file state
	FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);
	if (!FileState.IsValid())
	{
		return false;
	}

	// File is managed if it's not unknown
	return FileState->IsSourceControlled();
}

bool FPerforceAdapter::HasPerforceConflicts(const FString& BlueprintPath, FString& OutConflictInfo)
{
	if (!IsPerforceAvailable())
	{
		OutConflictInfo = TEXT("Perforce is not available");
		return false;
	}

	FString FilePath = AssetPathToFilePath(BlueprintPath);
	if (FilePath.IsEmpty())
	{
		OutConflictInfo = FString::Printf(TEXT("Could not convert asset path to file path: %s"), *BlueprintPath);
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		OutConflictInfo = TEXT("Source control provider is not available");
		return false;
	}

	// Force a state refresh to get the latest Perforce status
	FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::ForceUpdate);
	if (!FileState.IsValid())
	{
		OutConflictInfo = TEXT("Could not get file state");
		return false;
	}

	bool bIsConflicted = FileState->IsConflicted();

	// Fallback: Check for conflict marker files (.BASE, .THEIRS, .orig)
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString BaseFilePath = FilePath + TEXT(".BASE");
	FString TheirsFilePath = FilePath + TEXT(".THEIRS");
	FString OrigFilePath = FilePath + TEXT(".orig");

	bool bHasConflictMarkers = PlatformFile.FileExists(*BaseFilePath) ||
		PlatformFile.FileExists(*TheirsFilePath) ||
		PlatformFile.FileExists(*OrigFilePath);

	if (bIsConflicted || bHasConflictMarkers)
	{
		if (bHasConflictMarkers && !bIsConflicted)
		{
			OutConflictInfo = FString::Printf(TEXT("File has conflict marker files but state doesn't report conflict"));
		}
		else
		{
			OutConflictInfo = FString::Printf(TEXT("File has conflicts in Perforce"));
		}
		return true;
	}

	return false;
}

bool FPerforceAdapter::GetFileState(const FString& FilePath, bool& OutIsConflicted, bool& OutIsCheckedOut)
{
	OutIsConflicted = false;
	OutIsCheckedOut = false;

	if (!IsPerforceAvailable())
	{
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		return false;
	}

	FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);
	if (!FileState.IsValid())
	{
		return false;
	}

	OutIsConflicted = FileState->IsConflicted();
	OutIsCheckedOut = FileState->IsCheckedOut();
	return true;
}

bool FPerforceAdapter::GetBaseVersion(const FString& BlueprintPath, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot)
{
	FString FilePath = AssetPathToFilePath(BlueprintPath);
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Could not convert asset path to file path: %s"), *BlueprintPath);
		return false;
	}

	FString Error;
	if (!LoadBlueprintFromVersion(FilePath, TEXT("BASE"), OutBlueprint, OutSnapshot, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Failed to load BASE version: %s"), *Error);
		return false;
	}

	return true;
}

bool FPerforceAdapter::GetLocalVersion(const FString& BlueprintPath, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot)
{
	FString FilePath = AssetPathToFilePath(BlueprintPath);
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Could not convert asset path to file path: %s"), *BlueprintPath);
		return false;
	}

	// Local version is just the current file
	OutBlueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath, nullptr, LOAD_NoWarn);
	if (!OutBlueprint || !IsValid(OutBlueprint))
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Failed to load LOCAL Blueprint: %s"), *BlueprintPath);
		return false;
	}

	if (!FSnapshotManager::CreateSnapshot(OutBlueprint, OutSnapshot))
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Failed to create LOCAL snapshot: %s"), *BlueprintPath);
		return false;
	}

	return true;
}

bool FPerforceAdapter::GetRemoteVersion(const FString& BlueprintPath, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot)
{
	FString FilePath = AssetPathToFilePath(BlueprintPath);
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Could not convert asset path to file path: %s"), *BlueprintPath);
		return false;
	}

	FString Error;
	if (!LoadBlueprintFromVersion(FilePath, TEXT("HEAD"), OutBlueprint, OutSnapshot, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Failed to load REMOTE version: %s"), *Error);
		return false;
	}

	return true;
}

bool FPerforceAdapter::CheckoutFile(const FString& FilePath, FString& OutError)
{
	if (!IsPerforceAvailable())
	{
		OutError = TEXT("Perforce is not available");
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		OutError = TEXT("Source control provider is not available");
		return false;
	}

	// Use provider's Execute method with CheckOut operation
	TSharedRef<FCheckOut, ESPMode::ThreadSafe> CheckOutOp = ISourceControlOperation::Create<FCheckOut>();
	ECommandResult::Type Result = Provider->Execute(CheckOutOp, TArray<FString>{FilePath}, EConcurrency::Synchronous);

	if (Result == ECommandResult::Succeeded)
	{
		return true;
	}

	OutError = TEXT("Checkout failed");
	return false;
}

bool FPerforceAdapter::ResolveConflict(const FString& FilePath, EPerforceResolveMethod ResolveMethod, FString& OutError)
{
	if (!IsPerforceAvailable())
	{
		OutError = TEXT("Perforce is not available");
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		OutError = TEXT("Source control provider is not available");
		return false;
	}

	// Use provider's Execute method with Resolve operation
	TSharedRef<FResolve, ESPMode::ThreadSafe> ResolveOp = ISourceControlOperation::Create<FResolve>();
	ECommandResult::Type Result = Provider->Execute(ResolveOp, TArray<FString>{FilePath}, EConcurrency::Synchronous);

	if (Result == ECommandResult::Succeeded)
	{
		return true;
	}

	OutError = TEXT("Resolve failed");
	return false;
}

bool FPerforceAdapter::SubmitFiles(const TArray<FString>& FilePaths, const FString& Description, FString& OutError)
{
	if (!IsPerforceAvailable())
	{
		OutError = TEXT("Perforce is not available");
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		OutError = TEXT("Source control provider is not available");
		return false;
	}

	// Use provider's Execute method with CheckIn operation
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOp = ISourceControlOperation::Create<FCheckIn>();
	CheckInOp->SetDescription(FText::FromString(Description));
	ECommandResult::Type Result = Provider->Execute(CheckInOp, FilePaths, EConcurrency::Synchronous);

	if (Result == ECommandResult::Succeeded)
	{
		return true;
	}

	OutError = TEXT("Submit failed");
	return false;
}

int32 FPerforceAdapter::DetectConflictedBlueprints(TArray<FString>& OutConflictedBlueprints)
{
	OutConflictedBlueprints.Empty();

	if (!IsPerforceAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: DetectConflictedBlueprints - Perforce is not available"));
		return 0;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: DetectConflictedBlueprints - Source control provider is not available"));
		return 0;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Starting conflict detection - querying Perforce directly for conflicted files..."));

	// Better approach: Scan Content directory for .uasset files and query Perforce for their status
	// This is more efficient than scanning all Blueprints through the asset registry
	FString ContentDir = FPaths::ProjectContentDir();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> UAssetFiles;

	// Recursively find all .uasset files in Content directory
	// Helper function to recursively search directories
	struct FDirectorySearcher
	{
		IPlatformFile& FileManager;
		TArray<FString>& OutFiles;

		FDirectorySearcher(IPlatformFile& InFileManager, TArray<FString>& InOutFiles)
			: FileManager(InFileManager), OutFiles(InOutFiles) {
		}

		void SearchDirectory(const FString& Directory)
		{
			FileManager.IterateDirectory(*Directory, [this](const TCHAR* Filename, bool bIsDirectory) -> bool
			{
				if (bIsDirectory)
				{
					// Recursively search subdirectories
					SearchDirectory(Filename);
				}
				else if (FString(Filename).EndsWith(TEXT(".uasset")))
				{
					OutFiles.Add(Filename);
				}
				return true; // Continue iteration
			});
		}
	};

	FDirectorySearcher Searcher(PlatformFile, UAssetFiles);
	Searcher.SearchDirectory(ContentDir);

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Found %d .uasset files in Content directory"), UAssetFiles.Num());

	int32 CheckedCount = 0;
	int32 ManagedCount = 0;
	int32 ConflictedCount = 0;

	// OPTIMIZATION: Use cached state first for fast filtering, only force update for potential conflicts
	// This is much faster than ForceUpdate for every file

	// First pass: Quick scan using cached state to find potential conflicts
	TArray<FString> PotentialConflicts;
	for (const FString& FilePath : UAssetFiles)
	{
		CheckedCount++;

		// Use cached state first - this is much faster
		FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);

		if (!FileState.IsValid())
		{
			continue; // Skip files that Perforce doesn't know about
		}

		// Check if file is managed by Perforce
		if (!FileState->IsSourceControlled())
		{
			continue; // Skip files not managed by Perforce
		}

		ManagedCount++;

		// Quick check: Use cached state to see if it might be conflicted
		bool bIsConflicted = FileState->IsConflicted();
		bool bIsCheckedOut = FileState->IsCheckedOut();

		// Also check for conflict marker files (fast file system check)
		bool bHasConflictMarkers = false;
		FString BaseFilePath = FilePath + TEXT(".BASE");
		FString TheirsFilePath = FilePath + TEXT(".THEIRS");
		FString OrigFilePath = FilePath + TEXT(".orig");

		if (PlatformFile.FileExists(*BaseFilePath) ||
			PlatformFile.FileExists(*TheirsFilePath) ||
			PlatformFile.FileExists(*OrigFilePath))
		{
			bHasConflictMarkers = true;
		}

		// Log every 10th file for debugging (to avoid spam)
		if (ManagedCount % 10 == 0)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("PerforceAdapter: Checking file %d/%d: %s (Conflicted: %s, CheckedOut: %s, HasMarkers: %s)"),
				ManagedCount, UAssetFiles.Num(), *FPaths::GetCleanFilename(FilePath),
				bIsConflicted ? TEXT("Yes") : TEXT("No"),
				bIsCheckedOut ? TEXT("Yes") : TEXT("No"),
				bHasConflictMarkers ? TEXT("Yes") : TEXT("No"));
		}

		// If cached state shows conflict or we found markers, add to potential conflicts
		if (bIsConflicted || bHasConflictMarkers)
		{
			PotentialConflicts.Add(FilePath);
			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Potential conflict found (cached): %s (IsConflicted: %s, HasMarkers: %s)"),
				*FilePath, bIsConflicted ? TEXT("Yes") : TEXT("No"), bHasConflictMarkers ? TEXT("Yes") : TEXT("No"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Quick scan found %d potential conflicts, verifying with fresh state..."), PotentialConflicts.Num());

	// If no potential conflicts found in cached state, do a force update scan on all managed files
	// This handles the case where cached state is stale
	if (PotentialConflicts.Num() == 0 && ManagedCount > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: No conflicts found in cached state, doing full force update scan on %d managed files..."), ManagedCount);

		// Re-scan all managed files with force update
		for (const FString& FilePath : UAssetFiles)
		{
			FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);
			if (!FileState.IsValid() || !FileState->IsSourceControlled())
			{
				continue;
			}

			// Force update to get latest state
			FileState = Provider->GetState(FilePath, EStateCacheUsage::ForceUpdate);
			if (!FileState.IsValid())
			{
				continue;
			}

			bool bIsConflicted = FileState->IsConflicted();
			bool bHasConflictMarkers = false;
			FString BaseFilePath = FilePath + TEXT(".BASE");
			FString TheirsFilePath = FilePath + TEXT(".THEIRS");
			FString OrigFilePath = FilePath + TEXT(".orig");

			if (PlatformFile.FileExists(*BaseFilePath) ||
				PlatformFile.FileExists(*TheirsFilePath) ||
				PlatformFile.FileExists(*OrigFilePath))
			{
				bHasConflictMarkers = true;
			}

			if (bIsConflicted || bHasConflictMarkers)
			{
				PotentialConflicts.Add(FilePath);
				UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Found conflict on force update: %s (IsConflicted: %s, HasMarkers: %s)"),
					*FilePath, bIsConflicted ? TEXT("Yes") : TEXT("No"), bHasConflictMarkers ? TEXT("Yes") : TEXT("No"));
			}
		}

		UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Force update scan found %d conflicts"), PotentialConflicts.Num());
	}

	// Second pass: Force update only for potential conflicts to get accurate state
	for (const FString& FilePath : PotentialConflicts)
	{
		// Now force update to get the latest accurate state
		FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::ForceUpdate);

		if (!FileState.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: File state invalid after force update: %s"), *FilePath);
			continue;
		}

		// Check for conflicts with fresh state
		bool bIsConflicted = FileState->IsConflicted();
		bool bIsCheckedOut = FileState->IsCheckedOut();

		// Re-check conflict markers
		bool bHasConflictMarkers = false;
		FString BaseFilePath = FilePath + TEXT(".BASE");
		FString TheirsFilePath = FilePath + TEXT(".THEIRS");
		FString OrigFilePath = FilePath + TEXT(".orig");

		if (PlatformFile.FileExists(*BaseFilePath) ||
			PlatformFile.FileExists(*TheirsFilePath) ||
			PlatformFile.FileExists(*OrigFilePath))
		{
			bHasConflictMarkers = true;
		}

		if (bIsConflicted || bHasConflictMarkers)
		{
			ConflictedCount++;

			// Convert file path back to asset path for the output
			FString AssetPath = FilePathToAssetPath(FilePath);
			if (AssetPath.IsEmpty())
			{
				// If conversion fails, use the file path as-is
				AssetPath = FilePath;
			}

			if (bHasConflictMarkers && !bIsConflicted)
			{
				UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: Found conflict marker files but IsConflicted() returned false for: %s"), *FilePath);
			}

			if (!bIsCheckedOut)
			{
				UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: Found conflicted file that is NOT checked out: %s"), *FilePath);
			}

			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: ✓ Detected conflicted Blueprint: %s (Checked Out: %s, Has Markers: %s)"),
				*AssetPath,
				bIsCheckedOut ? TEXT("Yes") : TEXT("No"),
				bHasConflictMarkers ? TEXT("Yes") : TEXT("No"));

			OutConflictedBlueprints.Add(AssetPath);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Conflict detection complete - Scanned: %d files, Managed by Perforce: %d, Conflicted: %d"),
		CheckedCount, ManagedCount, ConflictedCount);

	if (ConflictedCount == 0 && ManagedCount > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: No conflicts detected, but %d files are managed by Perforce."), ManagedCount);
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: This might mean:"), ManagedCount);
		UE_LOG(LogTemp, Warning, TEXT("  1. Conflicts have already been resolved"));
		UE_LOG(LogTemp, Warning, TEXT("  2. Perforce state cache is stale (try refreshing source control)"));
		UE_LOG(LogTemp, Warning, TEXT("  3. Files are not actually in a conflicted state"));
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: Check Output Log for detailed file-by-file status."));
	}

	return OutConflictedBlueprints.Num();
}

bool FPerforceAdapter::LoadAllVersions(
	const FString& BlueprintPath,
	UBlueprint*& OutBaseBlueprint,
	UBlueprint*& OutLocalBlueprint,
	UBlueprint*& OutRemoteBlueprint,
	TSharedPtr<FJsonObject>& OutBaseSnapshot,
	TSharedPtr<FJsonObject>& OutLocalSnapshot,
	TSharedPtr<FJsonObject>& OutRemoteSnapshot,
	FString& OutError)
{
	OutBaseBlueprint = nullptr;
	OutLocalBlueprint = nullptr;
	OutRemoteBlueprint = nullptr;

	// Load BASE version
	if (!GetBaseVersion(BlueprintPath, OutBaseBlueprint, OutBaseSnapshot))
	{
		OutError = TEXT("Failed to load BASE version");
		return false;
	}

	// Load LOCAL version
	if (!GetLocalVersion(BlueprintPath, OutLocalBlueprint, OutLocalSnapshot))
	{
		OutError = TEXT("Failed to load LOCAL version");
		return false;
	}

	// Load REMOTE version
	if (!GetRemoteVersion(BlueprintPath, OutRemoteBlueprint, OutRemoteSnapshot))
	{
		OutError = TEXT("Failed to load REMOTE version");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Successfully loaded all versions for: %s"), *BlueprintPath);
	return true;
}

FString FPerforceAdapter::AssetPathToFilePath(const FString& AssetPath)
{
	// AssetPath should be a package name like "/Game/Path/BP_MyBlueprint"
	// Package names in Unreal should start with "/" (long package name format)
	FString PackageName = AssetPath;

	// Ensure it starts with "/" for long package name format
	if (!PackageName.StartsWith(TEXT("/")))
	{
		PackageName = TEXT("/") + PackageName;
	}

	// Validate it's a long package name
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("PerforceAdapter: Invalid package name format: %s"), *AssetPath);
		return FString();
	}

	// Try to convert package name to file path
	FString FilePath;
	if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, FilePath))
	{
		// Add .uasset extension if not present
		if (!FilePath.EndsWith(TEXT(".uasset")))
		{
			FilePath += TEXT(".uasset");
		}
		return FilePath;
	}

	// Fallback: Try DoesPackageExist
	if (FPackageName::DoesPackageExist(PackageName, &FilePath))
	{
		return FilePath;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("PerforceAdapter: Could not convert package name to file path: %s"), *PackageName);
	return FString();
}

FString FPerforceAdapter::FilePathToAssetPath(const FString& FilePath)
{
	// Convert file path to asset path
	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(FilePath, PackageName))
	{
		return PackageName;
	}

	return FString();
}

FString FPerforceAdapter::GetDepotPath(const FString& FilePath)
{
	if (!IsPerforceAvailable())
	{
		return FString();
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		return FString();
	}

	FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);
	if (!FileState.IsValid())
	{
		return FString();
	}

	// Get depot path from file state if available
	// Note: This may not be directly available, depends on source control implementation
	return FilePath; // Fallback to file path
}

bool FPerforceAdapter::RevertFile(const FString& FilePath, FString& OutError)
{
	if (!IsPerforceAvailable())
	{
		OutError = TEXT("Perforce is not available");
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		OutError = TEXT("Source control provider is not available");
		return false;
	}

	// Use provider's Execute method with Revert operation
	TSharedRef<FRevert, ESPMode::ThreadSafe> RevertOp = ISourceControlOperation::Create<FRevert>();
	ECommandResult::Type Result = Provider->Execute(RevertOp, TArray<FString>{FilePath}, EConcurrency::Synchronous);

	if (Result == ECommandResult::Succeeded)
	{
		return true;
	}

	OutError = TEXT("Revert failed");
	return false;
}

bool FPerforceAdapter::LoadBlueprintFromVersion(const FString& FilePath, const FString& Version, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot, FString& OutError)
{
	// Get the version-specific file path (BASE/REMOTE are always fetched from Perforce into temp)
	FString VersionFilePath = GetVersionFilePath(FilePath, Version);
	if (VersionFilePath.IsEmpty())
	{
		if (Version == TEXT("BASE"))
		{
			OutError = FString::Printf(TEXT("Could not fetch BASE version from Perforce.\n\nFile: %s"), *FilePath);
		}
		else if (Version == TEXT("HEAD") || Version == TEXT("THEIRS") || Version == TEXT("REMOTE"))
		{
			OutError = FString::Printf(TEXT("Could not fetch REMOTE version from Perforce.\n\nFile: %s"), *FilePath);
		}
		else
		{
			OutError = FString::Printf(TEXT("Could not get file path for version %s of: %s"), *Version, *FilePath);
		}
		return false;
	}

	// Check if the version file actually exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*VersionFilePath))
	{
		OutError = FString::Printf(TEXT("Version file does not exist: %s"), *VersionFilePath);
		return false;
	}

	// If version file is in our temp folder (Saved/...), copy to a loadable path under Content so LoadObject can resolve it
	FString PathToLoad = VersionFilePath;
	FString TempDir = GetTempDirectory();
	if (VersionFilePath.StartsWith(TempDir))
	{
		FString LoadablePath, CopyError;
		if (!CopyTempVersionToLoadablePath(VersionFilePath, Version, LoadablePath, CopyError))
		{
			OutError = FString::Printf(TEXT("Could not prepare temp version for loading: %s"), *CopyError);
			return false;
		}
		PathToLoad = LoadablePath;

		// So Unreal recognizes the new file: scan using package path (ScanPathsSynchronous expects /Game/..., not file paths)
		FString PackagePathForScan = FilePathToAssetPath(LoadablePath);
		if (!PackagePathForScan.IsEmpty())
		{
			int32 LastSlash;
			if (PackagePathForScan.FindLastChar(TEXT('/'), LastSlash))
			{
				FString PackageDir = PackagePathForScan.Left(LastSlash);
				if (!PackageDir.IsEmpty())
				{
					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
					IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
					TArray<FString> PathsToScan;
					PathsToScan.Add(PackageDir);
					AssetRegistry.ScanPathsSynchronous(PathsToScan, /* bForceRescan */ true);
				}
			}
		}
	}

	// Convert file path to asset path and load (use standard path so TryConvertFilenameToLongPackageName can resolve it)
	FString PathToLoadNormalized = FPaths::ConvertRelativePathToFull(PathToLoad);
	FString AssetPath = FilePathToAssetPath(PathToLoadNormalized);
	if (AssetPath.IsEmpty() && PathToLoadNormalized.Contains(TEXT("BlueprintMergeToolTemp")))
	{
		// Fallback: we copied to Content/BlueprintMergeToolTemp; build package path manually
		FString BaseName = FPaths::GetBaseFilename(PathToLoadNormalized, false);
		AssetPath = FString::Printf(TEXT("/Game/BlueprintMergeToolTemp/%s"), *BaseName);
	}
	if (AssetPath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not convert version file path to asset path: %s"), *PathToLoad);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Loading Blueprint from version %s: %s (file: %s)"), *Version, *AssetPath, *PathToLoad);

	OutBlueprint = LoadObject<UBlueprint>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);
	if (!OutBlueprint || !IsValid(OutBlueprint))
	{
		// The .uasset from Perforce was saved with an internal package path (e.g. /Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter).
		// When we place it at BlueprintMergeToolTemp and load by that path, the engine may register the package under the internal name,
		// so LoadObject returns nullptr. Load the package and find the Blueprint inside the loaded package.
		UPackage* Pkg = LoadPackage(nullptr, *AssetPath, LOAD_None);
		if (!Pkg && PathToLoadNormalized.Contains(TEXT("BlueprintMergeToolTemp")))
		{
			// Asset registry may have registered the file under its internal name; try loading by file path so the engine finds our file
			Pkg = LoadPackage(nullptr, *PathToLoadNormalized, LOAD_None);
		}
		if (Pkg)
		{
			TArray<UObject*> Objs;
			GetObjectsWithOuter(Pkg, Objs, /* bIncludeNestedObjects */ false);
			for (UObject* Obj : Objs)
			{
				UBlueprint* BP = Cast<UBlueprint>(Obj);
				if (BP && IsValid(BP))
				{
					OutBlueprint = BP;
					break;
				}
			}
		}
	}
	if (!OutBlueprint || !IsValid(OutBlueprint))
	{
		OutError = FString::Printf(TEXT("Failed to load Blueprint from version %s.\n\nAsset Path: %s\nFile Path: %s\n\nThis may indicate the file is corrupted or not a valid Blueprint."), *Version, *AssetPath, *PathToLoad);
		return false;
	}

	if (!FSnapshotManager::CreateSnapshot(OutBlueprint, OutSnapshot))
	{
		OutError = FString::Printf(TEXT("Failed to create snapshot for version %s of: %s"), *Version, *AssetPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Successfully loaded %s version: %s"), *Version, *AssetPath);
	return true;
}

bool FPerforceAdapter::FetchVersionFromPerforce(const FString& FilePath, const FString& Version, FString& OutVersionFilePath, FString& OutError)
{
	// This function fetches a specific version from Perforce and saves it to a temp file
	if (!IsPerforceAvailable())
	{
		OutError = TEXT("Perforce is not available");
		return false;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		OutError = TEXT("Source control provider is not available");
		return false;
	}

	// Get Perforce settings once and reuse for all commands
	// Prefer Perforce plugin section (has the actual workspace Unreal connected with, e.g. Blueprint_new)
	FString P4Port, P4User, P4Client;

	const TCHAR* PerforceSection = TEXT("PerforceSourceControl.PerforceSourceControlSettings");
	// Same config dir as editor (e.g. Saved/Config/WindowsEditor/) so we find SourceControlSettings.ini
	FString SourceControlIni = FPaths::GetPath(GEditorPerProjectIni) / TEXT("SourceControlSettings.ini");

	GConfig->GetString(PerforceSection, TEXT("Port"), P4Port, *SourceControlIni);
	GConfig->GetString(PerforceSection, TEXT("UserName"), P4User, *SourceControlIni);
	GConfig->GetString(PerforceSection, TEXT("Workspace"), P4Client, *SourceControlIni);

	// Port from Perforce section may be just the number (e.g. 1666); p4 accepts that for localhost

	// Fallback: legacy /Script/SourceControl.SourceControlSettings (P4Port, P4User, P4Client)
	if (P4Port.IsEmpty())
	{
		GConfig->GetString(TEXT("/Script/SourceControl.SourceControlSettings"), TEXT("P4Port"), P4Port, GEditorPerProjectIni);
	}
	if (P4User.IsEmpty())
	{
		GConfig->GetString(TEXT("/Script/SourceControl.SourceControlSettings"), TEXT("P4User"), P4User, GEditorPerProjectIni);
	}
	if (P4Client.IsEmpty())
	{
		GConfig->GetString(TEXT("/Script/SourceControl.SourceControlSettings"), TEXT("P4Client"), P4Client, GEditorPerProjectIni);
	}
	if (P4Port.IsEmpty())
	{
		GConfig->GetString(TEXT("SourceControl.SourceControlSettings"), TEXT("P4Port"), P4Port, GEditorPerProjectIni);
	}
	if (P4User.IsEmpty())
	{
		GConfig->GetString(TEXT("SourceControl.SourceControlSettings"), TEXT("P4User"), P4User, GEditorPerProjectIni);
	}
	if (P4Client.IsEmpty())
	{
		GConfig->GetString(TEXT("SourceControl.SourceControlSettings"), TEXT("P4Client"), P4Client, GEditorPerProjectIni);
	}

	if (!P4Client.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Using workspace from config (PerforceSourceControl/Workspace): '%s'"), *P4Client);
	}

	// Save current environment variables (we'll restore them before return)
	FString OldP4Port = FPlatformMisc::GetEnvironmentVariable(TEXT("P4PORT"));
	FString OldP4User = FPlatformMisc::GetEnvironmentVariable(TEXT("P4USER"));
	FString OldP4Client = FPlatformMisc::GetEnvironmentVariable(TEXT("P4CLIENT"));

	// Set only P4PORT and P4USER from config before p4 info. Do NOT set P4CLIENT yet:
	// p4 info will then use the process's current P4CLIENT (the one Unreal set when it connected, e.g. Blueprint_new).
	if (!P4Port.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4PORT"), *P4Port);
	}
	if (!P4User.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4USER"), *P4User);
	}

	// Run p4 info to get the actual client Unreal is using (from process env P4CLIENT, not config).
	// This avoids using a stale/wrong client from config (e.g. DESKTOP-5F7UU9L) when Unreal connected as Blueprint_new.
	{
		FString InfoArgs;
		if (!P4Port.IsEmpty() && !P4User.IsEmpty())
		{
			InfoArgs = FString::Printf(TEXT("-p %s -u %s info"), *P4Port, *P4User);
		}
		else
		{
			InfoArgs = TEXT("info");
		}
		FString InfoOutput, InfoError;
		int32 InfoReturnCode = 0;
		bool bInfoSuccess = FPlatformProcess::ExecProcess(TEXT("p4"), *InfoArgs, &InfoReturnCode, &InfoOutput, &InfoError);

		if (bInfoSuccess && InfoReturnCode == 0 && !InfoOutput.IsEmpty())
		{
			TArray<FString> InfoLines;
			InfoOutput.ParseIntoArrayLines(InfoLines);
			for (const FString& Line : InfoLines)
			{
				if (P4Port.IsEmpty() && Line.StartsWith(TEXT("Server address:")))
				{
					P4Port = Line.Mid(15).TrimStartAndEnd();
				}
				else if (P4User.IsEmpty() && Line.StartsWith(TEXT("User name:")))
				{
					P4User = Line.Mid(10).TrimStartAndEnd();
				}
				else if (P4Client.IsEmpty() && Line.StartsWith(TEXT("Client name:")))
				{
					FString InfoClient = Line.Mid(12).TrimStartAndEnd();
					if (!InfoClient.IsEmpty())
					{
						P4Client = InfoClient;
						UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Using client from p4 info: '%s'"), *P4Client);
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: p4 info - P4Port: '%s', P4User: '%s', P4Client: '%s'"),
				*P4Port, *P4User, *P4Client);
		}
	}

	// Ensure environment variables are set for all subsequent p4 commands
	if (!P4Port.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4PORT"), *P4Port);
	}
	if (!P4User.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4USER"), *P4User);
	}
	if (!P4Client.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4CLIENT"), *P4Client);
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Final settings - P4Port: '%s', P4User: '%s', P4Client: '%s'"),
		*P4Port, *P4User, *P4Client);

	// Create temp directory if it doesn't exist
	FString TempDir = GetTempDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*TempDir))
	{
		PlatformFile.CreateDirectoryTree(*TempDir);
	}

	// Generate temp file path
	FString FileName = FPaths::GetCleanFilename(FilePath);
	FString TempFilePath = TempDir / FString::Printf(TEXT("%s_%s"), *FileName, *Version);

	// Get depot path and revisions via p4 fstat -Ot (tagged output; -Ot = tagged, file path must follow)
	FString FstatDepotFile, FstatHaveRev, FstatHeadRev;
	FString LastP4Error;  // used to detect "client unknown" / "must create client" for a clear message
	{
		FString FstatCommand;
		if (!P4Port.IsEmpty() && !P4User.IsEmpty() && !P4Client.IsEmpty())
		{
			FstatCommand = FString::Printf(TEXT("-p %s -u %s -c %s fstat -Ot \"%s\""),
				*P4Port, *P4User, *P4Client, *FilePath);
		}
		else if (!P4Port.IsEmpty() && !P4User.IsEmpty())
		{
			FstatCommand = FString::Printf(TEXT("-p %s -u %s fstat -Ot \"%s\""),
				*P4Port, *P4User, *FilePath);
		}
		else
		{
			FstatCommand = FString::Printf(TEXT("fstat -Ot \"%s\""), *FilePath);
		}
		FString FstatOutput, FstatError;
		int32 FstatReturnCode = 0;
		UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Getting file status with: p4 %s"), *FstatCommand);
		bool bFstatSuccess = FPlatformProcess::ExecProcess(TEXT("p4"), *FstatCommand, &FstatReturnCode, &FstatOutput, &FstatError);
		if (bFstatSuccess && FstatReturnCode == 0 && !FstatOutput.IsEmpty())
		{
			TArray<FString> FstatLines;
			FstatOutput.ParseIntoArrayLines(FstatLines);
			for (const FString& Line : FstatLines)
			{
				FString Trimmed = Line.TrimStartAndEnd();
				// Tagged format uses "fieldName\tvalue" or "fieldName value"
				auto ParseTaggedValue = [&Trimmed](const TCHAR* Prefix, int32 PrefixLen) -> FString
				{
					if (!Trimmed.StartsWith(Prefix)) return FString();
					int32 ValueStart = PrefixLen;
					while (ValueStart < Trimmed.Len() && (Trimmed[ValueStart] == ' ' || Trimmed[ValueStart] == '\t'))
					{
						ValueStart++;
					}
					return Trimmed.Mid(ValueStart).TrimStartAndEnd();
				};
				if (Trimmed.StartsWith(TEXT("depotFile")))
				{
					FstatDepotFile = ParseTaggedValue(TEXT("depotFile"), 9);
				}
				else if (Trimmed.StartsWith(TEXT("haveRev")))
				{
					FstatHaveRev = ParseTaggedValue(TEXT("haveRev"), 7);
				}
				else if (Trimmed.StartsWith(TEXT("headRev")))
				{
					FstatHeadRev = ParseTaggedValue(TEXT("headRev"), 7);
				}
			}
			if (!FstatDepotFile.IsEmpty())
			{
				UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: fstat depotFile=%s haveRev=%s headRev=%s"), *FstatDepotFile, *FstatHaveRev, *FstatHeadRev);
			}
		}
		else
		{
			LastP4Error = FstatError;
			UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: p4 fstat failed (code=%d). StdErr: %s"), FstatReturnCode, *FstatError);
		}
	}

	FString FileSpec;

	if (Version == TEXT("BASE"))
	{
		FString DepotPath = FstatDepotFile;
		FString RevisionNumber = FstatHaveRev;

		if (DepotPath.IsEmpty() || RevisionNumber.IsEmpty())
		{
			FString WhereCommand;
			if (!P4Port.IsEmpty() && !P4User.IsEmpty() && !P4Client.IsEmpty())
			{
				WhereCommand = FString::Printf(TEXT("-p %s -u %s -c %s where \"%s\""),
					*P4Port, *P4User, *P4Client, *FilePath);
			}
			else if (!P4Port.IsEmpty() && !P4User.IsEmpty())
			{
				WhereCommand = FString::Printf(TEXT("-p %s -u %s where \"%s\""),
					*P4Port, *P4User, *FilePath);
			}
			else
			{
				WhereCommand = FString::Printf(TEXT("where \"%s\""), *FilePath);
			}
			FString WhereOutput, WhereError;
			int32 WhereReturnCode = 0;
			bool bWhereSuccess = FPlatformProcess::ExecProcess(TEXT("p4"), *WhereCommand, &WhereReturnCode, &WhereOutput, &WhereError);
			if (bWhereSuccess && WhereReturnCode == 0 && !WhereOutput.IsEmpty())
			{
				// p4 where returns: depotPath clientPath localPath (separator can be " - ", tab, or space)
				FString FirstPath;
				int32 Sep = WhereOutput.Find(TEXT(" - "));
				if (Sep == INDEX_NONE)
				{
					Sep = WhereOutput.Find(TEXT("\t"));
				}
				if (Sep == INDEX_NONE)
				{
					Sep = WhereOutput.Find(TEXT(" "));
				}
				FirstPath = (Sep != INDEX_NONE) ? WhereOutput.Left(Sep).TrimStartAndEnd() : WhereOutput.TrimStartAndEnd();
				if (FirstPath.StartsWith(TEXT("//")))
				{
					DepotPath = FirstPath;
					UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Got depot path from where: %s"), *DepotPath);
				}
			}
			else
			{
				LastP4Error = WhereError;
				UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: p4 where failed (code=%d). StdErr: %s"), WhereReturnCode, *WhereError);
			}

			if (DepotPath.IsEmpty())
			{
				FString FilesCommand;
				if (!P4Port.IsEmpty() && !P4User.IsEmpty() && !P4Client.IsEmpty())
				{
					FilesCommand = FString::Printf(TEXT("-p %s -u %s -c %s files \"%s\""),
						*P4Port, *P4User, *P4Client, *FilePath);
				}
				else if (!P4Port.IsEmpty() && !P4User.IsEmpty())
				{
					FilesCommand = FString::Printf(TEXT("-p %s -u %s files \"%s\""),
						*P4Port, *P4User, *FilePath);
				}
				else
				{
					FilesCommand = FString::Printf(TEXT("files \"%s\""), *FilePath);
				}
				FString FilesOutput, FilesError;
				int32 FilesReturnCode = 0;
				bool bFilesSuccess = FPlatformProcess::ExecProcess(TEXT("p4"), *FilesCommand, &FilesReturnCode, &FilesOutput, &FilesError);
				if (bFilesSuccess && FilesReturnCode == 0 && !FilesOutput.IsEmpty())
				{
					int32 HashIndex = FilesOutput.Find(TEXT("#"));
					if (HashIndex != INDEX_NONE)
					{
						DepotPath = FilesOutput.Left(HashIndex).TrimStartAndEnd();
						int32 SpaceIndex = FilesOutput.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, HashIndex);
						if (SpaceIndex != INDEX_NONE)
						{
							RevisionNumber = FilesOutput.Mid(HashIndex + 1, SpaceIndex - HashIndex - 1).TrimStartAndEnd();
						}
					}
					if (!DepotPath.IsEmpty())
					{
						UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Got depot path from files: %s"), *DepotPath);
					}
				}
			}

			if (RevisionNumber.IsEmpty() && !DepotPath.IsEmpty())
			{
				FString FilesCommand;
				if (!P4Port.IsEmpty() && !P4User.IsEmpty() && !P4Client.IsEmpty())
				{
					FilesCommand = FString::Printf(TEXT("-p %s -u %s -c %s files \"%s\""),
						*P4Port, *P4User, *P4Client, *DepotPath);
				}
				else if (!P4Port.IsEmpty() && !P4User.IsEmpty())
				{
					FilesCommand = FString::Printf(TEXT("-p %s -u %s files \"%s\""),
						*P4Port, *P4User, *DepotPath);
				}
				else
				{
					FilesCommand = FString::Printf(TEXT("files \"%s\""), *DepotPath);
				}
				FString FilesOutput, FilesError;
				int32 FilesReturnCode = 0;
				FPlatformProcess::ExecProcess(TEXT("p4"), *FilesCommand, &FilesReturnCode, &FilesOutput, &FilesError);
				if (FilesReturnCode == 0 && !FilesOutput.IsEmpty())
				{
					int32 HashIndex = FilesOutput.Find(TEXT("#"));
					if (HashIndex != INDEX_NONE)
					{
						int32 SpaceIndex = FilesOutput.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, HashIndex);
						if (SpaceIndex != INDEX_NONE)
						{
							RevisionNumber = FilesOutput.Mid(HashIndex + 1, SpaceIndex - HashIndex - 1).TrimStartAndEnd();
						}
					}
				}
			}
		}

		if (DepotPath.IsEmpty())
		{
			if ((LastP4Error.Contains(TEXT("unknown")) && LastP4Error.Contains(TEXT("client"))) ||
				LastP4Error.Contains(TEXT("must create client")))
			{
				OutError = FString::Printf(TEXT(
					"The Perforce client workspace '%s' does not exist on the server.\n\n"
					"Create it first:\n"
					"  1. Open a terminal and run:  p4 client %s\n"
					"  2. Edit the client spec (Root, View) and save.\n"
					"Or in P4V: Connection → Edit Current Workspace → create/set workspace '%s'.\n\n"
					"Then retry loading from Perforce."),
					*P4Client, *P4Client, *P4Client);
			}
			else
			{
				OutError = FString::Printf(TEXT("Could not determine depot path for: %s\n\np4 fstat/where/files failed. Check Perforce connection and that the file is in the client view."), *FilePath);
			}
			UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: %s"), *OutError);
			return false;
		}

		if (!RevisionNumber.IsEmpty())
		{
			FileSpec = FString::Printf(TEXT("%s#%s"), *DepotPath, *RevisionNumber);
			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: BASE - using %s"), *FileSpec);
		}
		else
		{
			FileSpec = DepotPath;
			UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: BASE revision unknown, using depot path: %s"), *DepotPath);
		}
	}
	else if (Version == TEXT("HEAD") || Version == TEXT("THEIRS") || Version == TEXT("REMOTE"))
	{
		// REMOTE = head revision in depot. Must use depot path, not local path.
		FString DepotPath = FstatDepotFile;
		if (DepotPath.IsEmpty())
		{
			FString WhereCommand;
			if (!P4Port.IsEmpty() && !P4User.IsEmpty() && !P4Client.IsEmpty())
			{
				WhereCommand = FString::Printf(TEXT("-p %s -u %s -c %s where \"%s\""),
					*P4Port, *P4User, *P4Client, *FilePath);
			}
			else if (!P4Port.IsEmpty() && !P4User.IsEmpty())
			{
				WhereCommand = FString::Printf(TEXT("-p %s -u %s where \"%s\""),
					*P4Port, *P4User, *FilePath);
			}
			else
			{
				WhereCommand = FString::Printf(TEXT("where \"%s\""), *FilePath);
			}
			FString WhereOutput, WhereError;
			int32 WhereReturnCode = 0;
			FPlatformProcess::ExecProcess(TEXT("p4"), *WhereCommand, &WhereReturnCode, &WhereOutput, &WhereError);
			if (WhereReturnCode == 0 && !WhereOutput.IsEmpty())
			{
				int32 Sep = WhereOutput.Find(TEXT(" - "));
				if (Sep == INDEX_NONE)
				{
					Sep = WhereOutput.Find(TEXT("\t"));
				}
				if (Sep == INDEX_NONE)
				{
					Sep = WhereOutput.Find(TEXT(" "));
				}
				FString FirstPath = (Sep != INDEX_NONE) ? WhereOutput.Left(Sep).TrimStartAndEnd() : WhereOutput.TrimStartAndEnd();
				if (FirstPath.StartsWith(TEXT("//")))
				{
					DepotPath = FirstPath;
				}
			}
			else
			{
				LastP4Error = WhereError;
			}
		}
		if (DepotPath.IsEmpty())
		{
			if ((LastP4Error.Contains(TEXT("unknown")) && LastP4Error.Contains(TEXT("client"))) ||
				LastP4Error.Contains(TEXT("must create client")))
			{
				OutError = FString::Printf(TEXT(
					"The Perforce client workspace '%s' does not exist on the server.\n\n"
					"Create it: run  p4 client %s  then edit Root/View and save.\n"
					"Or in P4V: Connection → Edit Current Workspace → create workspace '%s'."),
					*P4Client, *P4Client, *P4Client);
			}
			else
			{
				OutError = FString::Printf(TEXT("Could not determine depot path for REMOTE: %s"), *FilePath);
			}
			UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: %s"), *OutError);
			return false;
		}
		FileSpec = DepotPath + TEXT("#head");
		UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: REMOTE - using %s"), *FileSpec);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown version type: %s"), *Version);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Attempting to fetch %s version from Perforce for: %s"), *Version, *FilePath);
	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Using file spec: %s"), *FileSpec);

	// Settings are already retrieved above, reuse them for p4 print

	// Execute p4 print command with connection flags if available
	// p4 print -o <output_file> <file_spec>
	FString P4CommandArgs;
	if (!P4Port.IsEmpty() && !P4User.IsEmpty() && !P4Client.IsEmpty())
	{
		// Use p4 -p -u -c flags to specify connection info directly
		P4CommandArgs = FString::Printf(TEXT("-p %s -u %s -c %s print -o \"%s\" \"%s\""),
			*P4Port, *P4User, *P4Client, *TempFilePath, *FileSpec);
		UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Using p4 print with explicit connection flags"));
	}
	else
	{
		// Fallback: use environment variables
		P4CommandArgs = FString::Printf(TEXT("print -o \"%s\" \"%s\""), *TempFilePath, *FileSpec);
		UE_LOG(LogTemp, Warning, TEXT("PerforceAdapter: Perforce config incomplete for print, using environment variables"));
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Executing: p4 %s"), *P4CommandArgs);

	// Execute the command
	int32 ReturnCode = 0;
	FString StdOut, StdErr;
	bool bSuccess = FPlatformProcess::ExecProcess(
		TEXT("p4"),
		*P4CommandArgs,
		&ReturnCode,
		&StdOut,
		&StdErr
	);

	// Restore environment variables before returning (whether success or failure)
	if (!OldP4Port.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4PORT"), *OldP4Port);
	}
	else if (!P4Port.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4PORT"), TEXT(""));
	}
	if (!OldP4User.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4USER"), *OldP4User);
	}
	else if (!P4User.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4USER"), TEXT(""));
	}
	if (!OldP4Client.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4CLIENT"), *OldP4Client);
	}
	else if (!P4Client.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("P4CLIENT"), TEXT(""));
	}

	if (!bSuccess || ReturnCode != 0)
	{
		OutError = FString::Printf(TEXT("Failed to execute p4 print command.\nReturn code: %d\nError: %s\n\nCommand: p4 %s"),
			ReturnCode, *StdErr, *P4CommandArgs);
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: %s"), *OutError);
		return false;
	}

	// Check if the file was created
	if (!PlatformFile.FileExists(*TempFilePath))
	{
		OutError = FString::Printf(TEXT("p4 print command succeeded but output file was not created: %s\n\nStdOut: %s\nStdErr: %s"),
			*TempFilePath, *StdOut, *StdErr);
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: %s"), *OutError);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Successfully fetched %s version to: %s"), *Version, *TempFilePath);
	OutVersionFilePath = TempFilePath;
	return true;
}

FString FPerforceAdapter::GetVersionFilePath(const FString& FilePath, const FString& Version)
{
	// Always fetch BASE and REMOTE from Perforce and store in temp folder, then run merge locally.
	// We no longer use .BASE/.THEIRS next to the file — we get base and remote from the server into temp.

	if (Version == TEXT("BASE"))
	{
		FString FetchedPath, Error;
		if (FetchVersionFromPerforce(FilePath, Version, FetchedPath, Error))
		{
			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Fetched BASE version from Perforce to temp: %s"), *FetchedPath);
			return FetchedPath;
		}
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Could not fetch BASE version: %s"), *Error);
		return FString();
	}
	else if (Version == TEXT("HEAD") || Version == TEXT("THEIRS") || Version == TEXT("REMOTE"))
	{
		FString FetchedPath, Error;
		if (FetchVersionFromPerforce(FilePath, Version, FetchedPath, Error))
		{
			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Fetched REMOTE version from Perforce to temp: %s"), *FetchedPath);
			return FetchedPath;
		}
		UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Could not fetch REMOTE version: %s"), *Error);
		return FString();
	}

	// For LOCAL version, return the original file path
	// This is your working copy (with your local changes)
	if (Version == TEXT("LOCAL") || Version.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Using LOCAL version from: %s"), *FilePath);
		return FilePath;
	}

	// Fallback: return empty string if version file not found
	UE_LOG(LogTemp, Error, TEXT("PerforceAdapter: Could not find version file for version '%s' of: %s"), *Version, *FilePath);
	return FString();
}

bool FPerforceAdapter::IsBlueprintFile(const FString& FilePath)
{
	return FilePath.EndsWith(TEXT(".uasset"));
}

FString FPerforceAdapter::GetTempDirectory()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintMergeTool") / TEXT("TempVersions");
}

bool FPerforceAdapter::CopyTempVersionToLoadablePath(const FString& TempVersionFilePath, const FString& Version, FString& OutLoadableFilePath, FString& OutError)
{
	// Copy temp file to a path under Content. Use original Blueprint name + unique suffix so we never
	// overwrite a file that may be loaded (and locked) by the engine.
	FString LoadableDir = FPaths::ProjectContentDir() / TEXT("BlueprintMergeToolTemp");
	FString TempFileName = FPaths::GetCleanFilename(TempVersionFilePath);
	FString BaseName = TempFileName;
	if (BaseName.RemoveFromEnd(TEXT("_BASE")) || BaseName.RemoveFromEnd(TEXT("_REMOTE")))
	{
		// BaseName is now "BP_ThirdPersonCharacter.uasset"
	}
	FString AssetBaseName = FPaths::GetBaseFilename(BaseName, false);
	if (AssetBaseName.IsEmpty())
	{
		AssetBaseName = TEXT("MergeBlueprint");
	}
	// Unique suffix so we don't overwrite a locked file from a previous load
	FString UniqueSuffix = FGuid::NewGuid().ToString().Left(8);
	FString LoadableFileName = AssetBaseName + (Version == TEXT("BASE") ? TEXT("_Base_") : TEXT("_Remote_")) + UniqueSuffix + TEXT(".uasset");
	OutLoadableFilePath = LoadableDir / LoadableFileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LoadableDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*LoadableDir))
		{
			OutError = FString::Printf(TEXT("Could not create loadable directory: %s"), *LoadableDir);
			return false;
		}
	}

	// No need to delete first - we use a unique filename each time
	if (!PlatformFile.CopyFile(*OutLoadableFilePath, *TempVersionFilePath, EPlatformFileRead::None, EPlatformFileWrite::None))
	{
		OutError = FString::Printf(TEXT("Could not copy temp version to loadable path. From: %s To: %s"), *TempVersionFilePath, *OutLoadableFilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Copied %s version from temp to loadable path: %s"), *Version, *OutLoadableFilePath);
	return true;
}

