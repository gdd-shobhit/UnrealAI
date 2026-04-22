# Perforce Integration Guide

This document explains how to use the Perforce integration features of the Blueprint Merge Tool.

## Overview

The Perforce integration allows you to:
- **Automatically detect** Blueprint conflicts from Perforce
- **Load BASE, LOCAL, and REMOTE versions** directly from Perforce
- **Resolve conflicts** using Perforce's conflict resolution system
- **Submit merged changes** back to Perforce

## Prerequisites

1. **Perforce must be configured** in Unreal Engine:
   - Go to **Edit → Editor Preferences → Source Control**
   - Select **Perforce** as the provider
   - Enter your Perforce server, username, and workspace

2. **Blueprint files must be managed** by Perforce (added to the depot)

3. **Conflicts must exist** for the merge tool to detect them

## Features

### Automatic Conflict Detection

The Perforce adapter can automatically detect Blueprints with conflicts:

```cpp
TArray<FString> ConflictedBlueprints;
int32 ConflictCount = FPerforceAdapter::DetectConflictedBlueprints(ConflictedBlueprints);

if (ConflictCount > 0)
{
    UE_LOG(LogTemp, Log, TEXT("Found %d conflicted Blueprints"), ConflictCount);
    for (const FString& BlueprintPath : ConflictedBlueprints)
    {
        UE_LOG(LogTemp, Log, TEXT("  - %s"), *BlueprintPath);
    }
}
```

### Loading Versions from Perforce

Load all three versions (BASE, LOCAL, REMOTE) for a conflicted Blueprint:

```cpp
UBlueprint* BaseBP = nullptr;
UBlueprint* LocalBP = nullptr;
UBlueprint* RemoteBP = nullptr;
TSharedPtr<FJsonObject> BaseSnapshot;
TSharedPtr<FJsonObject> LocalSnapshot;
TSharedPtr<FJsonObject> RemoteSnapshot;
FString Error;

if (FPerforceAdapter::LoadAllVersions(
    BlueprintPath,
    BaseBP, LocalBP, RemoteBP,
    BaseSnapshot, LocalSnapshot, RemoteSnapshot,
    Error))
{
    // All versions loaded successfully
    // Now you can perform a three-way diff
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Failed to load versions: %s"), *Error);
}
```

### Perforce Operations

#### Checkout Files

```cpp
FString Error;
if (FPerforceAdapter::CheckoutFile(FilePath, Error))
{
    UE_LOG(LogTemp, Log, TEXT("File checked out successfully"));
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Checkout failed: %s"), *Error);
}
```

#### Resolve Conflicts

```cpp
FString Error;
if (FPerforceAdapter::ResolveConflict(
    FilePath,
    EPerforceResolveMethod::AcceptYours,  // or AcceptTheirs, AcceptMerge, Ignore
    Error))
{
    UE_LOG(LogTemp, Log, TEXT("Conflict resolved successfully"));
}
```

#### Submit Changes

```cpp
TArray<FString> FilesToSubmit;
FilesToSubmit.Add(FilePath);

FString Error;
if (FPerforceAdapter::SubmitFiles(
    FilesToSubmit,
    TEXT("Merged Blueprint conflicts using Blueprint Merge Tool"),
    Error))
{
    UE_LOG(LogTemp, Log, TEXT("Files submitted successfully"));
}
```

## UI Integration

The merge UI can be extended to automatically load Perforce conflicts:

1. **Detect Conflicts Button**: Automatically finds all conflicted Blueprints
2. **Load from Perforce Button**: Loads BASE, LOCAL, REMOTE versions for a selected Blueprint
3. **Auto-Resolve Button**: Attempts automatic resolution using merge strategies
4. **Submit Button**: Submits resolved changes back to Perforce

## Workflow Example

### Manual Workflow

1. **Detect conflicts**:
   ```cpp
   TArray<FString> Conflicts;
   FPerforceAdapter::DetectConflictedBlueprints(Conflicts);
   ```

2. **Select a conflicted Blueprint** and load all versions:
   ```cpp
   FPerforceAdapter::LoadAllVersions(BlueprintPath, ...);
   ```

3. **Perform merge** using the existing merge tool workflow

4. **Resolve in Perforce**:
   ```cpp
   FPerforceAdapter::ResolveConflict(FilePath, EPerforceResolveMethod::AcceptMerge, Error);
   ```

5. **Submit changes**:
   ```cpp
   FPerforceAdapter::SubmitFiles({FilePath}, TEXT("Resolved merge conflicts"), Error);
   ```

### Automatic Workflow

1. Use the UI's "Detect Perforce Conflicts" button
2. Select a conflicted Blueprint from the list
3. Click "Load from Perforce" to automatically load all versions
4. Perform diff and create merge plan
5. Apply merge and resolve conflicts
6. Submit to Perforce

## File Version Handling

### BASE Version

The BASE version is the common ancestor version. In Perforce conflicts, this is typically stored in:
- `.BASE` file extension
- `.orig` file extension

The adapter automatically looks for these files.

### LOCAL Version

The LOCAL version is your current working copy. It's loaded directly from the file system.

### REMOTE Version

The REMOTE version is the latest version from the depot. In Perforce conflicts, this is typically:
- `.THEIRS` file extension (during conflict resolution)
- HEAD revision from the depot

## Limitations

1. **Version file detection** relies on standard Perforce conflict file naming (`.BASE`, `.THEIRS`, etc.)
2. **Custom Perforce setups** may require additional configuration
3. **Large Blueprints** may take time to load from Perforce
4. **Network access** is required to fetch REMOTE versions

## Troubleshooting

### "Perforce is not available"

- Check that Perforce is configured in Editor Preferences
- Verify the Perforce provider is selected
- Test connection in Source Control settings

### "Failed to load BASE/REMOTE version"

- Check that conflict files (`.BASE`, `.THEIRS`) exist
- Verify file permissions
- Check Perforce workspace mapping

### "File not managed by Perforce"

- Ensure the Blueprint file has been added to Perforce
- Check that the file is in the workspace
- Verify the file path is correct

## Future Enhancements

Potential improvements:
- Direct Perforce API integration (bypassing file system)
- Batch conflict resolution for multiple Blueprints
- Integration with Perforce streams
- Automatic conflict detection on sync
- Visual diff for Perforce conflicts
- Undo/redo support for Perforce operations

## API Reference

See `PerforceAdapter.h` for complete API documentation.

