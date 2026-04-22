# How Python Tools Map to C++

The Python side (mcp_tools.py + unreal_mcp_bridge.py) only **defines** tools for the LLM. **Execution** happens in Unreal. The link is the **tool name** and the **JSON arguments**.

## Flow

```
Python (mcp_tools.py)          Python (bridge)              Unreal (C++)
─────────────────────         ─────────────────            ─────────────────
register_tool("create_        LLM returns tool_call:         Poll returns:
  blueprint", ...)       →       name: "create_blueprint"  →   ToolName, ToolArgsJson
                                     args: {...}                   ↓
                                                          FUnrealToolRegistry::ExecuteTool(
                                                            FName("create_blueprint"),
                                                            ArgsJson,
                                                            OutResult
                                                          )
                                                                    ↓
                                                          TMap<FName, FToolEntry> lookup
                                                                    ↓
                                                          Tool_CreateBlueprint(ArgsJson, OutResult)
```

## What Must Match

| Python (mcp_tools.py) | C++ (UnrealToolRegistry.cpp) |
|----------------------|------------------------------|
| **Tool name** (string in `register_tool(name, ...)`) | **FName** in `RegisterTool(FName(TEXT("...")), ...)` |
| **Parameter names** in `parameters` dict | **JSON keys** read in each `Tool_*(ArgsJson, OutResult)` (e.g. `Obj->GetStringField(TEXT("name"))`) |
| **Parameter types** (for LLM only) | C++ parses JSON; types are not enforced by the registry |

So:

1. **Name**: The `name` you use in Python must be **exactly** the same as the `FName` used in C++ when calling `RegisterTool(...)`.
2. **Arguments**: The parameter names in Python are the keys the LLM (and bridge) send. The C++ implementation must read those same keys from `ArgsJson` (e.g. `"name"`, `"blueprint_path"`, `"graph_name"`).

## Where Things Live in C++

- **Registration**: `FUnrealToolRegistry::RegisterBuiltInTools()` in **UnrealToolRegistry.cpp** (called from `FUnrealAIModule::StartupModule()` in UnrealAI.cpp).
- **Execution**: When the UI receives a tool call from the bridge (poll result), it calls  
  `FUnrealToolRegistry::ExecuteTool(FName(*R.ToolName), R.ToolArgsJson, ToolResult)` in **UnrealAI.cpp**.
- **Implementation**: Each tool is a static function, e.g. `Tool_CreateBlueprint`, `Tool_CreateNode`, etc., in **UnrealToolRegistry.cpp**. They parse `ArgsJson` with `FJsonSerializer::Deserialize` and read fields with `Obj->GetStringField(TEXT("name"))`, `Obj->GetNumberField(TEXT("position_x"))`, etc.

## Adding a New Tool (both sides)

1. **C++** (UnrealToolRegistry.cpp):
   - Implement a static function `static void Tool_MyTool(const FString& ArgsJson, FString& OutResult)` that parses JSON and does the work.
   - In `RegisterBuiltInTools()`, add:
     - `RegisterTool(FName(TEXT("my_tool")), Description, InputSchemaJson, FExecuteToolDelegate::CreateStatic(Tool_MyTool));`
2. **Python** (mcp_tools.py):
   - In `_register_builtins()` add:
     - `register_tool("my_tool", "Description for the LLM", {"arg1": "string", "arg2": "number"});`
   - Use the **same name** and the **same argument names** the C++ code expects in the JSON.

If the Python tool name or argument names don’t match the C++ side, the LLM may call a tool that Unreal doesn’t have, or Unreal will receive JSON with different keys and the handler will fail or use wrong values.
