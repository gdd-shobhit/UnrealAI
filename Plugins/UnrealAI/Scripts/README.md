# UnrealAI MCP Bridge

HTTP server that runs an LLM with tool-calling. Unreal connects via HTTP: **POST /run**, **GET /poll**, **POST /tool_result**.

## Setup

```bash
cd Plugins/UnrealAI/Scripts
pip install -r requirements.txt
```

## Run

```bash
python unreal_mcp_bridge.py
```

Listens on `http://127.0.0.1:8765` by default. Set `UNREAL_AI_BRIDGE_PORT` and `UNREAL_AI_BRIDGE_HOST` to override.

## Endpoints

- **POST /run** – Body: `{ "prompt": "...", "provider": "Local LLM" | "Claude API" | "OpenAI API" }`. Returns `{ "session_id": "..." }`.
- **GET /poll?session=&lt;id&gt;** – Returns `{ "log_lines": [...], "done": bool, "tool_call": { "id", "name", "args" }? }`. Unreal polls until `done`; when `tool_call` is present, Unreal executes the tool and POSTs the result to `/tool_result`.
- **POST /tool_result** – Body: `{ "session_id", "tool_call_id", "result" }`.
- **GET /tools** – Returns tool definitions (optional).

## Providers

- **Local LLM**: Ollama at `http://localhost:11434/v1` (no API key).
- **OpenAI API**: Set `OPENAI_API_KEY`.
- **Claude API**: Set `ANTHROPIC_API_KEY`.

## Protocol

1. Unreal sends prompt via **POST /run** and gets a session id.
2. Unreal repeatedly **GET /poll**; bridge returns new log lines and optionally one pending tool call.
3. When a tool call is returned, Unreal executes it (via `FUnrealToolRegistry::ExecuteTool`) and **POST /tool_result** with the result.
4. Bridge feeds the result to the LLM and continues; next poll may return more log lines or the next tool call.
5. When the agent is finished, poll returns `done: true`.
