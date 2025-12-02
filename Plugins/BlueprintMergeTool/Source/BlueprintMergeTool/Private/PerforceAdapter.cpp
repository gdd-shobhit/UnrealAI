#include "../Public/PerforceAdapter.h"
#include "SourceControlHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "SnapshotManager.h"

bool FPerforceAdapter::IsPerforceAvailable()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	ISourceControlProvider* Provider = SourceControlModule.GetProvider();
	
	if (!Provider)
	{
		return false;
	}

	// Check if the provider is Perforce
	FName ProviderName = Provider->GetName();
	return ProviderName == "Perforce" || ProviderName == "P4";
}

ISourceControlProvider* FPerforceAdapter::GetSourceControlProvider()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	return SourceControlModule.GetProvider();
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

	FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);
	if (!FileState.IsValid())
	{
		OutConflictInfo = TEXT("Could not get file state");
		return false;
	}

	if (FileState->IsConflicted())
	{
		OutConflictInfo = FString::Printf(TEXT("File has conflicts in Perforce. State: %s"), *FileState->ToString());
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

	TSharedRef<FCheckOut, ESPMode::ThreadSafe> CheckOutOperation = ISourceControlOperation::Create<FCheckOut>();
	CheckOutOperation->SetFiles({ FilePath });
	
	ECommandResult::Type Result = Provider->Execute(CheckOutOperation, EConcurrency::Asynchronous);
	
	if (Result != ECommandResult::Succeeded)
	{
		OutError = FString::Printf(TEXT("Checkout failed: %s"), *CheckOutOperation->GetResultInfo());
		return false;
	}

	return true;
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

	TSharedRef<FResolve, ESPMode::ThreadSafe> ResolveOperation = ISourceControlOperation::Create<FResolve>();
	ResolveOperation->SetFiles({ FilePath });

	// Set resolve method
	switch (ResolveMethod)
	{
	case EPerforceResolveMethod::AcceptYours:
		ResolveOperation->SetResolutionMethod(EResolveMethod::AcceptYours);
		break;
	case EPerforceResolveMethod::AcceptTheirs:
		ResolveOperation->SetResolutionMethod(EResolveMethod::AcceptTheirs);
		break;
	case EPerforceResolveMethod::AcceptMerge:
		ResolveOperation->SetResolutionMethod(EResolveMethod::AcceptMerge);
		break;
	case EPerforceResolveMethod::Ignore:
		ResolveOperation->SetResolutionMethod(EResolveMethod::Ignore);
		break;
	}

	ECommandResult::Type Result = Provider->Execute(ResolveOperation, EConcurrency::Asynchronous);
	
	if (Result != ECommandResult::Succeeded)
	{
		OutError = FString::Printf(TEXT("Resolve failed: %s"), *ResolveOperation->GetResultInfo());
		return false;
	}

	return true;
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

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
	CheckInOperation->SetFiles(FilePaths);
	CheckInOperation->SetDescription(FText::FromString(Description));

	ECommandResult::Type Result = Provider->Execute(CheckInOperation, EConcurrency::Asynchronous);
	
	if (Result != ECommandResult::Succeeded)
	{
		OutError = FString::Printf(TEXT("Submit failed: %s"), *CheckInOperation->GetResultInfo());
		return false;
	}

	return true;
}

int32 FPerforceAdapter::DetectConflictedBlueprints(TArray<FString>& OutConflictedBlueprints)
{
	OutConflictedBlueprints.Empty();

	if (!IsPerforceAvailable())
	{
		return 0;
	}

	ISourceControlProvider* Provider = GetSourceControlProvider();
	if (!Provider)
	{
		return 0;
	}

	// Get all Blueprint files in the project
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets);

	for (const FAssetData& AssetData : BlueprintAssets)
	{
		FString AssetPath = AssetData.ObjectPath.ToString();
		FString FilePath = AssetPathToFilePath(AssetPath);

		if (FilePath.IsEmpty() || !IsBlueprintFile(FilePath))
		{
			continue;
		}

		FSourceControlStatePtr FileState = Provider->GetState(FilePath, EStateCacheUsage::Use);
		if (FileState.IsValid() && FileState->IsConflicted())
		{
			OutConflictedBlueprints.Add(AssetPath);
			UE_LOG(LogTemp, Log, TEXT("PerforceAdapter: Detected conflicted Blueprint: %s"), *AssetPath);
		}
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
	// Convert asset path to package name (remove leading slash if present)
	FString PackageName = AssetPath;
	if (PackageName.StartsWith(TEXT("/")))
	{
		PackageName = PackageName.Mid(1);
	}

	// Convert to long package name
	FString LongPackageName = FPackageName::ConvertToLongPackageName(PackageName);
	if (LongPackageName.IsEmpty())
	{
		return FString();
	}

	// Get the file path
	FString FilePath;
	if (FPackageName::DoesPackageExist(LongPackageName, &FilePath))
	{
		return FilePath;
	}

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

	TSharedRef<FRevert, ESPMode::ThreadSafe> RevertOperation = ISourceControlOperation::Create<FRevert>();
	RevertOperation->SetFiles({ FilePath });

	ECommandResult::Type Result = Provider->Execute(RevertOperation, EConcurrency::Asynchronous);
	
	if (Result != ECommandResult::Succeeded)
	{
		OutError = FString::Printf(TEXT("Revert failed: %s"), *RevertOperation->GetResultInfo());
		return false;
	}

	return true;
}

bool FPerforceAdapter::LoadBlueprintFromVersion(const FString& FilePath, const FString& Version, UBlueprint*& OutBlueprint, TSharedPtr<FJsonObject>& OutSnapshot, FString& OutError)
{
	// Get the version-specific file path
	FString VersionFilePath = GetVersionFilePath(FilePath, Version);
	if (VersionFilePath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not get file path for version %s"), *Version);
		return false;
	}

	// For BASE and REMOTE versions, we need to get them from Perforce
	// For now, we'll try to load from the file system
	// In a full implementation, you might need to:
	// 1. Use Perforce commands to get the version
	// 2. Save it to a temporary location
	// 3. Load it from there

	// Try to load the asset from the version file path
	FString AssetPath = FilePathToAssetPath(VersionFilePath);
	if (AssetPath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not convert version file path to asset path: %s"), *VersionFilePath);
		return false;
	}

	OutBlueprint = LoadObject<UBlueprint>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);
	if (!OutBlueprint || !IsValid(OutBlueprint))
	{
		OutError = FString::Printf(TEXT("Failed to load Blueprint from version %s: %s"), *Version, *AssetPath);
		return false;
	}

	if (!FSnapshotManager::CreateSnapshot(OutBlueprint, OutSnapshot))
	{
		OutError = FString::Printf(TEXT("Failed to create snapshot for version %s"), *Version);
		return false;
	}

	return true;
}

FString FPerforceAdapter::GetVersionFilePath(const FString& FilePath, const FString& Version)
{
	// For BASE version in Perforce conflicts, the file is typically named with .BASE extension
	// For HEAD/REMOTE version, we'd need to fetch it from Perforce
	
	if (Version == TEXT("BASE"))
	{
		// Perforce creates a .BASE file in the same directory when there's a conflict
		FString BaseFilePath = FilePath + TEXT(".BASE");
		
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*BaseFilePath))
		{
			return BaseFilePath;
		}
		
		// If .BASE doesn't exist, try .orig or other common suffixes
		BaseFilePath = FilePath + TEXT(".orig");
		if (PlatformFile.FileExists(*BaseFilePath))
		{
			return BaseFilePath;
		}
	}
	else if (Version == TEXT("HEAD") || Version == TEXT("THEIRS"))
	{
		// Perforce creates a .THEIRS file during conflict resolution
		FString TheirsFilePath = FilePath + TEXT(".THEIRS");
		
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*TheirsFilePath))
		{
			return TheirsFilePath;
		}
	}

	// For LOCAL version, return the original file path
	if (Version == TEXT("LOCAL") || Version.IsEmpty())
	{
		return FilePath;
	}

	// Fallback: return empty string if version file not found
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

