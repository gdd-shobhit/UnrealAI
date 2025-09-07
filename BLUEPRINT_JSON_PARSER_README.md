# Blueprint JSON Parser System

This system allows you to read Unreal Engine blueprints, convert them to JSON format for editing, and then recreate or modify the blueprints with the updated JSON data.

## Overview

**Yes, you can read blueprints and write changes to them!** This system provides:

1. **Blueprint to JSON**: Extract blueprint structure (variables, functions, nodes, connections) into editable JSON
2. **JSON to Blueprint**: Create new blueprints from JSON data
3. **Blueprint Updates**: Modify existing blueprints using JSON data
4. **Blueprint Loading/Saving**: Load and save blueprint assets programmatically

## Features

### Core Functionality
- **BlueprintToJson()**: Converts a blueprint asset to JSON string
- **JsonToBlueprint()**: Creates a new blueprint from JSON data
- **UpdateBlueprintFromJson()**: Updates an existing blueprint with JSON data
- **LoadBlueprint()**: Loads a blueprint asset by name/path
- **SaveBlueprint()**: Saves and compiles a blueprint

### What Gets Extracted
- **Variables**: Name, type, default value, category, edit flags
- **Functions**: Function graphs with nodes and connections
- **Event Graph**: Main execution flow with nodes and connections
- **Components**: Attached components
- **Properties**: Custom properties and metadata

### JSON Structure
The JSON format matches your existing `TestJsonParser2.json` structure:
```json
{
  "blueprint_name": "BP_Example",
  "description": "Description",
  "parent_class": "Character",
  "variables": [...],
  "functions": [...],
  "event_graph": [...],
  "event_connections": [...],
  "components": [...],
  "properties": {...}
}
```

## Usage

### 1. Basic Blueprint to JSON Conversion
```cpp
// Get the parser
UBlueprintJsonParser* Parser = NewObject<UBlueprintJsonParser>();

// Load an existing blueprint
UBlueprint* Blueprint = Parser->LoadBlueprint("/Game/Path/To/Blueprint");

// Convert to JSON
FString JsonString = Parser->BlueprintToJson(Blueprint);

// Save JSON to file or use as needed
// You can now edit this JSON manually or programmatically
```

### 2. Create Blueprint from JSON
```cpp
// Your modified JSON data
FString ModifiedJson = "...";

// Create new blueprint
UBlueprint* NewBlueprint = Parser->JsonToBlueprint(ModifiedJson, "NewBlueprintName");

// Save it
Parser->SaveBlueprint(NewBlueprint);
```

### 3. Update Existing Blueprint
```cpp
// Load existing blueprint
UBlueprint* Blueprint = Parser->LoadBlueprint("/Game/Path/To/Blueprint");

// Your modified JSON
FString UpdatedJson = "...";

// Update the blueprint
bool Success = Parser->UpdateBlueprintFromJson(Blueprint, UpdatedJson);

// Save changes
Parser->SaveBlueprint(Blueprint);
```

## Testing

The system includes a test actor (`ABlueprintJsonParserTest`) with three test functions:

1. **TestBlueprintToJson()**: Tests converting an existing blueprint to JSON
2. **TestJsonToBlueprint()**: Tests creating a new blueprint from JSON
3. **TestUpdateBlueprintFromJson()**: Tests updating an existing blueprint with JSON

### How to Test
1. Place the `ABlueprintJsonParserTest` actor in your level
2. Call the test functions from Blueprint or C++
3. Check the Output Log for results

## File Structure

```
Source/Testing/
├── BlueprintJsonParser.h          # Main parser class header
├── BlueprintJsonParser.cpp        # Main parser implementation
├── BlueprintJsonParserTest.h      # Test class header
└── BlueprintJsonParserTest.cpp    # Test class implementation
```

## Dependencies

The system requires these Unreal Engine modules:
- `BlueprintGraph`: For blueprint graph manipulation
- `UnrealEd`: For editor utilities
- `Json` & `JsonUtilities`: For JSON parsing
- `Kismet2`: For blueprint creation and compilation

## Limitations & Notes

### Current Implementation
- **Node Creation**: The `CreateNodesInGraph()` function is simplified and logs node creation rather than creating actual nodes
- **Advanced Features**: Complex node types and advanced blueprint features may require additional implementation
- **Editor Only**: Some functions require the editor to be running

### For Production Use
To make this production-ready, you would need to:
1. Implement full node creation in `CreateNodesInGraph()`
2. Add support for more complex blueprint features
3. Add error handling and validation
4. Implement proper pin connection logic
5. Add support for different node types (K2 nodes, custom nodes, etc.)

## Example Workflow

1. **Extract**: Use `BlueprintToJson()` to get JSON from an existing blueprint
2. **Edit**: Modify the JSON file manually or programmatically
3. **Apply**: Use `UpdateBlueprintFromJson()` to apply changes back to the blueprint
4. **Save**: Use `SaveBlueprint()` to save and compile the changes

## Use Cases

- **Blueprint Templates**: Create blueprint templates from JSON
- **Version Control**: Track blueprint changes in JSON format
- **Automation**: Programmatically modify blueprints
- **Migration**: Convert blueprints between projects
- **Backup**: Store blueprint structure in human-readable format

## Troubleshooting

### Common Issues
1. **Module Dependencies**: Ensure all required modules are included in `Testing.Build.cs`
2. **Editor Context**: Some functions require the Unreal Editor to be running
3. **Blueprint Paths**: Use correct asset paths (e.g., `/Game/Path/To/Blueprint`)
4. **JSON Format**: Ensure JSON matches the expected structure

### Debug Output
The system provides extensive logging. Check the Output Log for:
- Success/failure messages
- JSON conversion results
- Blueprint loading status
- Error details

## Future Enhancements

Potential improvements:
- Full node creation and connection support
- Support for more blueprint features (interfaces, macros, etc.)
- Batch operations for multiple blueprints
- JSON schema validation
- Blueprint diff/merge functionality
- Integration with version control systems
