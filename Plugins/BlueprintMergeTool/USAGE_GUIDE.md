# Blueprint Merge Tool - Usage Guide

## Quick Start

1. **Enable the Plugin**
   - Go to Edit → Plugins
   - Search for "Blueprint Merge Tool"
   - Enable the plugin and restart the editor

2. **Open the Tool**
   - Window → Blueprint Merge Tool
   - Or click the Blueprint Merge Tool icon in the toolbar

3. **Load Blueprints**
   - Select Base Blueprint (common ancestor)
   - Select Local Blueprint (your changes)
   - Select Remote Blueprint (incoming changes)

4. **Perform Merge**
   - Click "Perform Diff" to analyze differences
   - Click "Create Plan" to generate merge strategy
   - Review conflicts and choose resolution strategy
   - Click "Apply Merge" to execute

## Detailed Workflow

### 1. Blueprint Selection

The merge tool requires three Blueprint versions:

- **Base**: The common ancestor (usually from version control)
- **Local**: Your current working version
- **Remote**: The incoming changes (from another developer)

**Best Practices:**
- Ensure all Blueprints are saved before merging
- Use version control to get clean base versions
- Backup important Blueprints before merging

### 2. Diff Analysis

The diff process analyzes:
- **Variables**: Name, type, default value, flags, categories
- **Nodes**: Position, properties, connections, function references
- **Components**: Hierarchy, properties, attachments
- **Graphs**: Structure, execution flow, custom events

**Diff Output:**
- **Operations**: Non-conflicting changes that can be auto-applied
- **Conflicts**: Changes that require resolution decisions

### 3. Conflict Resolution

#### Automatic Strategies

- **Use Local**: Prefer all local changes
- **Use Remote**: Prefer all remote changes  
- **Smart Merge**: Intelligent combination of changes
- **Non-Destructive**: Prefer additions, avoid deletions

#### Manual Resolution

For complex conflicts, review each one individually:
1. Examine the conflicting values
2. Choose appropriate resolution
3. Consider impact on Blueprint functionality
4. Test the result

### 4. Merge Application

The apply process:
1. **Validates** the merge plan for consistency
2. **Creates** undo transaction for rollback
3. **Applies** operations in dependency order
4. **Compiles** the Blueprint to check for errors
5. **Saves** the modified Blueprint

## Conflict Types and Resolution

### Variable Conflicts

**Common Scenarios:**
- Different default values
- Type changes (dangerous)
- Category/tooltip changes (cosmetic)
- Flag changes (visibility, editability)

**Resolution Guidelines:**
- Type changes usually require manual review
- Default value conflicts can often be auto-resolved
- Cosmetic changes are low priority

### Node Conflicts

**Common Scenarios:**
- Position changes (low severity)
- Property modifications
- Function reference changes
- Pin connection differences

**Resolution Guidelines:**
- Position conflicts are usually auto-resolvable
- Function changes need careful review
- Connection changes may affect execution flow

### Component Conflicts

**Common Scenarios:**
- Transform changes
- Property modifications
- Hierarchy changes
- Attachment modifications

**Resolution Guidelines:**
- Transform changes are usually safe to merge
- Hierarchy changes need validation
- Property conflicts depend on the specific property

## Advanced Features

### Snapshot Export/Import

Export snapshots for:
- Version control integration
- External diff tools
- Backup and archival
- Automated testing

### AI-Powered Resolution

When integrated with UnrealAI:
1. Unresolved conflicts are analyzed by AI
2. AI provides resolution recommendations
3. Recommendations are validated before application
4. Complex conflicts get intelligent handling

### Custom Resolution Rules

Extend the system with custom rules:

```cpp
// Example: Custom resolution for PlayerController Blueprints
FMergePlannerConfig CustomConfig;
CustomConfig.PerTypeStrategies.Add(TEXT("InputVariable"), EResolutionStrategy::UseLocal);
CustomConfig.PerTypeStrategies.Add(TEXT("UIComponent"), EResolutionStrategy::UseRemote);
```

## Troubleshooting

### Common Issues

1. **"Blueprint is being edited"**
   - Close the Blueprint editor before merging
   - Save any pending changes

2. **"Compilation errors after merge"**
   - Check the Output Log for specific errors
   - Use the validation tools to identify issues
   - Consider rolling back and using different resolution

3. **"Snapshot creation failed"**
   - Ensure Blueprint is valid and saved
   - Check for corrupted Blueprint data
   - Try opening and resaving the Blueprint

4. **"Operations failed to apply"**
   - Check Blueprint is not read-only
   - Ensure sufficient permissions
   - Verify target Blueprint is valid

### Debugging Tools

**Console Commands:**
```
BlueprintMergeTool.RunSmokeTests          # Run basic functionality tests
BlueprintMergeTool.TestComponent All      # Test specific components
BlueprintMergeTool.RunExample            # Run example workflow
BlueprintMergeTool.RunComprehensiveTest  # Run full test suite
```

**Log Categories:**
- `LogTemp`: General merge tool operations
- `LogBlueprint`: Blueprint-specific operations
- `LogJson`: JSON parsing and serialization

### Performance Considerations

**Large Blueprints:**
- Snapshots may take longer to create
- Consider breaking large Blueprints into smaller ones
- Use batch operations for multiple merges

**Memory Usage:**
- Snapshots are kept in memory during merge process
- Clear tool state after completing merges
- Monitor memory usage with large Blueprints

## Integration Examples

### Version Control Integration

```cpp
// Example: Git integration workflow
void GitMergeWorkflow(const FString& BaseCommit, const FString& LocalCommit, const FString& RemoteCommit)
{
    // 1. Export Blueprints from each commit
    // 2. Create snapshots
    // 3. Perform merge
    // 4. Apply to working directory
    // 5. Commit merged result
}
```

### Automated Testing

```cpp
// Example: Automated merge testing
void AutomatedMergeTest()
{
    // 1. Load test case data
    // 2. Run merge workflow
    // 3. Validate results against expected output
    // 4. Report success/failure
}
```

### Batch Operations

```cpp
// Example: Merge multiple Blueprints
void BatchMergeWorkflow(const TArray<FString>& BlueprintPaths)
{
    for (const FString& Path : BlueprintPaths)
    {
        // 1. Load Blueprint versions
        // 2. Perform merge
        // 3. Validate result
        // 4. Continue to next Blueprint
    }
}
```

## Best Practices

### Before Merging
1. **Backup** important Blueprints
2. **Compile** all Blueprints to ensure they're valid
3. **Close** Blueprint editors to avoid conflicts
4. **Save** all pending changes

### During Merging
1. **Review** conflicts carefully before auto-resolution
2. **Test** critical functionality after merge
3. **Validate** Blueprint integrity
4. **Check** compilation status

### After Merging
1. **Test** the merged Blueprint thoroughly
2. **Commit** changes to version control
3. **Document** any manual resolution decisions
4. **Share** merge results with team

## Support

### Getting Help
- Check the README.md for technical details
- Review Examples/ folder for code samples
- Run smoke tests to verify installation
- Check Unreal Engine logs for detailed error messages

### Reporting Issues
When reporting issues, include:
- Unreal Engine version
- Plugin version
- Blueprint details (size, complexity)
- Error messages from logs
- Steps to reproduce

### Contributing
- Follow the modular architecture
- Add tests for new features
- Update documentation
- Ensure backward compatibility
