"""
MCP tool registry for UnrealAI bridge.
Register tools here; the bridge uses them for LLM tool-calling.
Tool execution happens in Unreal (C++); this module only defines name, description, and parameter schema.

Mapping to C++:
  - Tool "name" must match FUnrealToolRegistry::RegisterTool(FName(...)) in UnrealToolRegistry.cpp.
  - Parameter keys must match the JSON keys read in each Tool_*(ArgsJson, OutResult) in UnrealToolRegistry.cpp.
  See TOOLS_MAPPING.md for the full flow.

To add or edit tools:
  - Edit _register_builtins() below (keep names/params in sync with UnrealToolRegistry.cpp), or
  - Call register_tool(name, description, parameters) before the bridge runs.
    Parameters can be a dict of param_name -> type string, e.g. {"x": "number", "label": "string"}.
"""

from typing import Any

# ---------------------------------------------------------------------------
# Registry: list of OpenAI-format tool definitions
# ---------------------------------------------------------------------------

_registry: list[dict[str, Any]] = []

# Short type names -> JSON schema type
_TYPE_MAP = {
    "string": {"type": "string"},
    "number": {"type": "number"},
    "integer": {"type": "integer"},
    "boolean": {"type": "boolean"},
    "array": {"type": "array"},
    "object": {"type": "object"},
}


def register_tool(
    name: str,
    description: str,
    parameters: dict[str, Any] | None = None,
    *,
    required: list[str] | None = None,
) -> None:
    """
    Register a tool for the LLM. Parameters can be:
    - A dict of param_name -> type string, e.g. {"name": "string", "count": "number"}
    - A full JSON schema dict for "properties" (and optionally "required")
    """
    if parameters is None:
        parameters = {}
    props: dict[str, dict[str, str]] = {}
    for key, value in parameters.items():
        if isinstance(value, str) and value in _TYPE_MAP:
            props[key] = _TYPE_MAP[value].copy()
        elif isinstance(value, dict):
            props[key] = value
        else:
            props[key] = {"type": "string"}
    schema: dict[str, Any] = {"type": "object", "properties": props}
    if required:
        schema["required"] = required
    _registry.append({
        "type": "function",
        "function": {
            "name": name,
            "description": description,
            "parameters": schema,
        },
    })


def get_tools() -> list[dict[str, Any]]:
    """Return the list of tools in OpenAI format (for the bridge)."""
    return list(_registry)


def clear_tools() -> None:
    """Clear all registered tools (mainly for tests)."""
    _registry.clear()


# ---------------------------------------------------------------------------
# Built-in Unreal tools (must match Unreal FUnrealToolRegistry)
# ---------------------------------------------------------------------------

def _register_builtins() -> None:
    register_tool(
        "create_blueprint",
        "Create a new Blueprint asset. Args: name (string), parent_class_path (optional string, e.g. /Script/Engine.Actor)",
        {"name": "string", "parent_class_path": "string"},
    )
    register_tool(
        "create_node",
        "Add a node to a Blueprint graph. Args: blueprint_path, graph_name, node_type, position_x, position_y",
        {
            "blueprint_path": "string",
            "graph_name": "string",
            "node_type": "string",
            "position_x": "number",
            "position_y": "number",
        },
    )
    register_tool(
        "link_nodes",
        "Connect two nodes in a Blueprint graph. Args: blueprint_path, graph_name, from_node_id, from_pin, to_node_id, to_pin",
        {
            "blueprint_path": "string",
            "graph_name": "string",
            "from_node_id": "string",
            "from_pin": "string",
            "to_node_id": "string",
            "to_pin": "string",
        },
    )
    register_tool(
        "create_component",
        "Add a component to a Blueprint. Args: blueprint_path, component_class (optional, e.g. /Script/Engine.SceneComponent)",
        {"blueprint_path": "string", "component_class": "string"},
    )


_register_builtins()
