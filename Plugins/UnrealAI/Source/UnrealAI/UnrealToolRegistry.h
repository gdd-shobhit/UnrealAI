#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/Array.h"

/** Delegate for executing a tool: args as JSON string, returns result string and optional error */
DECLARE_DELEGATE_TwoParams(FExecuteToolDelegate, const FString& /* ArgsJson */, FString& /* OutResult */);

/** Single tool definition for MCP */
struct FUnrealToolDef
{
	FString Name;
	FString Description;
	FString InputSchema; // JSON schema for arguments (MCP-compatible)
};

/**
 * Registry of MCP tools that Unreal can execute.
 * Tools: create_blueprint, create_node, link_nodes, create_component, etc.
 * Bridge (Python) lists tools and sends invoke requests; Unreal executes and returns result.
 */
class UNREALAI_API FUnrealToolRegistry
{
public:
	/** Register a tool. Schema is JSON object schema for args, e.g. {"type":"object","properties":{...}} */
	static void RegisterTool(FName Name, const FString& Description, const FString& InputSchema, FExecuteToolDelegate Delegate);

	/** Execute a tool by name. Returns true on success, OutResult holds result or error message */
	static bool ExecuteTool(FName Name, const FString& ArgsJson, FString& OutResult);

	/** Get all registered tools for MCP server (name, description, schema) */
	static TArray<FUnrealToolDef> GetRegisteredTools();

	/** Initialize built-in tools (create_blueprint, create_node, link_nodes, create_component) */
	static void RegisterBuiltInTools();

private:
	struct FToolEntry
	{
		FString Description;
		FString InputSchema;
		FExecuteToolDelegate Delegate;
	};
	static TMap<FName, FToolEntry>& GetMap();
};
