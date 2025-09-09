# Blueprint Merge Tool

Advanced Blueprint merging and diffing tool with three-way merge support for Unreal Engine.

## Overview

The Blueprint Merge Tool provides sophisticated merging capabilities for Unreal Engine Blueprints, supporting:

- **Three-way merging** (Base, Local, Remote)
- **Deterministic JSON snapshots** for reliable diffing
- **Automatic conflict resolution** with intelligent rules
- **Manual conflict resolution** with intuitive UI
- **AI-powered conflict resolution** (optional, integrates with UnrealAI)
- **Comprehensive validation** and integrity checks

## Architecture

### Core Components

1. **SnapshotManager** - Creates deterministic JSON snapshots of UBlueprint objects
2. **DiffEngine** - Computes three-way structural diffs and generates operation lists
3. **MergePlanner** - Applies automatic resolution rules with LLM fallback
4. **ApplyEngine** - Applies operations using Editor APIs with proper compilation/saving
5. **MergeUI** - Editor window for conflict visualization and manual resolution
6. **Validator** - Comprehensive testing and Blueprint integrity validation

### Identity & Canonicalization

The system uses a robust identity matching system:

1. **GUID** (NodeGuid, VarGuid, PinId) - Primary authoritative identity
2. **Object path + name + class** - Secondary identity for nodes/graphs
3. **Semantic matching** - Fallback based on class + function reference + pin names

Snapshots are normalized before diffing:
- Variables sorted by VarGuid (or VarName if GUID missing)
- Nodes sorted by NodeGuid (or computed stable key)
- Transient fields (timestamps, positions) removed for comparison

## Usage

### Basic Workflow

1. **Open the Tool**: Window → Blueprint Merge Tool
2. **Select Blueprints**: Choose Base, Local, and Remote Blueprint versions
3. **Perform Diff**: Click "Perform Diff" to analyze differences
4. **Create Merge Plan**: Click "Create Plan" to generate resolution strategy
5. **Resolve Conflicts**: Use auto-resolution buttons or manual review
6. **Apply Merge**: Click "Apply Merge" to execute the plan

### Conflict Resolution Strategies

- **Use Local**: Prefer all local changes over remote
- **Use Remote**: Prefer all remote changes over local  
- **Smart Merge**: Intelligently combine changes when possible
- **Non-Destructive**: Prefer additions, combine when safe

### Supported Operations

- **Variables**: Add, Remove, Update, Remap GUIDs
- **Nodes**: Add, Remove, Update properties, Move
- **Connections**: Link/Unlink pins
- **Components**: Add, Remove, Update properties
- **Graphs**: Add, Remove, Rename

## File Structure

```
Source/BlueprintMergeTool/
├── Public/
│   ├── BlueprintMergeToolModule.h     # Main module interface
│   ├── BlueprintMergeToolCommands.h   # UI commands
│   ├── BlueprintMergeToolStyle.h      # Slate styling
│   ├── SnapshotManager.h              # JSON snapshot creation
│   ├── DiffEngine.h                   # Three-way diff computation
│   ├── MergePlanner.h                 # Automatic resolution rules
│   ├── ApplyEngine.h                  # Operation application
│   ├── MergeUI.h                      # Main UI widget
│   ├── BlueprintMergeValidator.h      # Testing and validation
│   ├── UnrealAILLMAdapter.h          # AI integration
│   └── BlueprintMergeToolAPI.h       # API definitions
└── Private/
    ├── BlueprintMergeToolModule.cpp   # Module implementation
    ├── BlueprintMergeToolCommands.cpp # Command registration
    ├── BlueprintMergeToolStyle.cpp    # Style implementation
    ├── SnapshotManager.cpp            # Snapshot implementation
    ├── DiffEngine.cpp                 # Diff algorithms
    ├── MergePlanner.cpp               # Planning logic
    ├── ApplyEngine.cpp                # Operation execution
    ├── MergeUI.cpp                    # UI implementation
    ├── BlueprintMergeValidator.cpp    # Validation logic
    └── UnrealAILLMAdapter.cpp         # AI adapter
```

## Technical Details

### Snapshot Format

Snapshots are deterministic JSON representations containing:

```json
{
  "BlueprintName": "MyBlueprint",
  "BlueprintPath": "/Game/Blueprints/MyBlueprint",
  "ParentClass": "AActor",
  "BlueprintType": "Normal",
  "Variables": [
    {
      "VariableName": "Health",
      "VariableGuid": "12345678-1234-1234-1234-123456789012",
      "VarType": "Int",
      "DefaultValue": "100",
      "bExposeOnSpawn": false,
      "Category": "Stats"
    }
  ],
  "Graphs": [
    {
      "GraphName": "EventGraph",
      "GraphGuid": "87654321-4321-4321-4321-210987654321",
      "Nodes": [...],
      "Connections": [...]
    }
  ],
  "Components": [...],
  "SnapshotHash": "abc123def456..."
}
```

### Operation Types

The system supports these merge operations:

- `AddNode`, `RemoveNode`, `UpdateNodeProperty`, `MoveNode`
- `AddVariable`, `RemoveVariable`, `UpdateVariable`, `RemapVariableGuid`
- `LinkPins`, `UnlinkPins`, `UpdatePinProperty`
- `AddComponent`, `RemoveComponent`, `UpdateComponent`
- `AddGraph`, `RemoveGraph`, `RenameGraph`

### Conflict Resolution

Conflicts are automatically categorized by severity:

- **Low**: Position changes, cosmetic modifications
- **Medium**: Property changes, non-breaking modifications
- **High**: Structural changes, potential data loss
- **Critical**: Conflicting fundamental changes

## Integration with UnrealAI

If the UnrealAI plugin is available, the merge tool can use AI for conflict resolution:

1. Unresolved conflicts are packaged into structured prompts
2. AI analyzes conflicts and provides resolution strategies
3. Responses are parsed into concrete merge operations
4. Operations are validated before application

## Safety Features

- **Thread Safety**: All operations run on the Game Thread
- **Undo Support**: Operations wrapped in FScopedTransaction
- **Validation**: Comprehensive integrity checks before/after operations
- **Backup**: Original Blueprints remain unchanged until explicit save
- **Rollback**: Failed operations don't corrupt Blueprint state

## Testing

The plugin includes comprehensive testing:

- **Unit Tests**: Test individual components with synthetic data
- **Smoke Tests**: Validate core functionality end-to-end
- **Integration Tests**: Test with real Blueprint assets
- **Performance Tests**: Validate performance with large Blueprints

Run tests via: `FBlueprintMergeValidator::RunSmokeTests()`

## Configuration

Default merge configuration:
- Non-destructive strategy
- Prefer local for variables
- Auto-resolve position conflicts
- Manual review for high/critical conflicts
- AI resolution disabled by default

## Dependencies

- **Core Unreal Modules**: Core, CoreUObject, Engine, UnrealEd
- **Blueprint Modules**: BlueprintGraph, KismetCompiler, KismetDeveloper  
- **UI Modules**: Slate, SlateCore, EditorWidgets, ToolMenus
- **Utility Modules**: Json, JsonObjectConverter, HTTP

## Future Enhancements

- Visual diff viewer with side-by-side comparison
- Batch merge operations for multiple Blueprints
- Integration with source control systems
- Custom merge rule scripting
- Performance optimizations for large Blueprints
- Export/import of merge templates

## Known Limitations

- Currently supports Blueprint assets only (not other UAsset types)
- Limited to Editor-time operations (no runtime merging)
- AI integration requires separate UnrealAI plugin or Ollama setup
- Complex node types may require manual resolution

## Contributing

When extending the merge tool:

1. Follow the modular architecture pattern
2. Add comprehensive validation for new operation types
3. Include unit tests for new functionality
4. Update documentation for new features
5. Ensure thread safety for all operations

## License

This plugin is part of the UnrealAI project and follows the same licensing terms.
