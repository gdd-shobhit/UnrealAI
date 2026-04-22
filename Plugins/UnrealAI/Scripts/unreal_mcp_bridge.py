"""
UnrealAI MCP Bridge: HTTP server that runs an LLM with tool-calling.
Unreal connects via HTTP: POST /run (start), GET /poll (log + tool_call), POST /tool_result (result).
Run: python unreal_mcp_bridge.py
Requires: pip install -r requirements.txt
"""

import json
import threading
import uuid
from queue import Queue, Empty
from typing import Any, Optional
from flask import Flask, request, jsonify

from mcp_tools import get_tools

# ---------------------------------------------------------------------------
# Session state
# ---------------------------------------------------------------------------

class SessionState:
    def __init__(self):
        self.log_lines: list[str] = []
        self.last_sent_index: int = 0
        self.pending_tool_calls: list[dict] = []  # [{ "id", "name", "args" }, ...]
        self.result_queue: Queue = Queue()  # agent thread blocks on get()
        self.done: bool = False
        self.error: Optional[str] = None
        self.lock = threading.Lock()

sessions: dict[str, SessionState] = {}
sessions_lock = threading.Lock()

def _log(session_id: str, msg: str) -> None:
    with sessions_lock:
        s = sessions.get(session_id)
    if s:
        with s.lock:
            s.log_lines.append(msg)

def _get_openai_tools_format():
    return get_tools()

# ---------------------------------------------------------------------------
# LLM client: OpenAI-compatible (Ollama, OpenAI) + Claude
# ---------------------------------------------------------------------------

def _provider_base(provider: str) -> tuple[str, str]:
    """Returns (api_base, api_key_env). api_base empty => use default."""
    p = (provider or "").strip().lower()
    if p in ("openai api", "openai"):
        return ("", "OPENAI_API_KEY")
    if p in ("claude api", "claude"):
        return ("", "ANTHROPIC_API_KEY")
    return ("http://localhost:11434/v1", "")  # Ollama

def _run_llm_chat(session_id: str, provider: str, messages: list[dict], stream: bool = True):
    """Yields (content_delta, tool_calls). Uses OpenAI client for OpenAI/Ollama; Anthropic for Claude."""
    import os
    base, key_env = _provider_base(provider)
    api_key = os.environ.get(key_env, "")
    p = (provider or "").strip().lower()

    if p in ("claude api", "claude"):
        return _run_claude(session_id, messages, api_key, stream)
    return _run_openai_compatible(session_id, base, api_key, messages, stream)

def _run_openai_compatible(session_id: str, base: str, api_key: str, messages: list[dict], stream: bool):
    """OpenAI and Ollama (OpenAI-compatible)."""
    try:
        from openai import OpenAI
        client = OpenAI(base_url=base if base else None, api_key=api_key or "dummy")
        kwargs = {
            "model": "llama3.2" if "localhost" in (base or "") else "gpt-4o-mini",
            "messages": messages,
            "stream": stream,
            "tools": _get_openai_tools_format(),
            "tool_choice": "auto",
        }
        if stream:
            stream_obj = client.chat.completions.create(**kwargs)
            for chunk in stream_obj:
                if not chunk.choices:
                    continue
                delta = chunk.choices[0].delta
                if getattr(delta, "content", None):
                    yield ("content", delta.content)
                if getattr(delta, "tool_calls", None):
                    for tc in delta.tool_calls or []:
                        tid = getattr(tc, "id", None) or ""
                        fn = getattr(tc, "function", None)
                        name = getattr(fn, "name", None) or ""
                        args = getattr(fn, "arguments", None) or "{}"
                        yield ("tool_call", {"id": tid, "name": name, "args": args})
        else:
            r = client.chat.completions.create(**{**kwargs, "stream": False})
            if r.choices:
                msg = r.choices[0].message
                if getattr(msg, "content", None):
                    yield ("content", msg.content)
                for tc in getattr(msg, "tool_calls", None) or []:
                    yield ("tool_call", {
                        "id": getattr(tc, "id", ""),
                        "name": getattr(tc.function, "name", ""),
                        "args": getattr(tc.function, "arguments", "{}"),
                    })
    except Exception as e:
        _log(session_id, f"[Bridge Error] OpenAI/Ollama: {e}")
        yield ("error", str(e))

def _run_claude(session_id: str, messages: list[dict], api_key: str, stream: bool):
    """Anthropic Claude. Maps OpenAI-style messages and tools to Claude format."""
    try:
        from anthropic import Anthropic
        client = Anthropic(api_key=api_key)
        # Convert to Claude message format
        system = ""
        claude_messages = []
        for m in messages:
            role = m.get("role", "")
            content = m.get("content") or ""
            if role == "system":
                system = content
                continue
            if role == "assistant" and m.get("tool_calls"):
                # Claude doesn't have assistant tool_calls in same form; we send tool_use blocks
                parts = [{"type": "text", "text": content}] if content else []
                for tc in m["tool_calls"]:
                    parts.append({
                        "type": "tool_use",
                        "id": tc.get("id", ""),
                        "name": tc.get("function", {}).get("name", ""),
                        "input": _parse_json_args(tc.get("function", {}).get("arguments", "{}")),
                    })
                claude_messages.append({"role": "assistant", "content": parts})
                continue
            if role == "user" and isinstance(content, list) and content and content[0].get("type") == "tool_result":
                # Convert tool result to user message for Claude
                parts = [{"type": "text", "text": content[0].get("content", "")}]
                claude_messages.append({"role": "user", "content": parts})
                continue
            if isinstance(content, str):
                claude_messages.append({"role": role, "content": content})
            else:
                claude_messages.append({"role": role, "content": content})

        # Build Claude tools from registry
        claude_tools = []
        for t in get_tools():
            f = t.get("function", {})
            claude_tools.append({
                "name": f.get("name", ""),
                "description": f.get("description", ""),
                "input_schema": f.get("parameters", {"type": "object", "properties": {}}),
            })

        kwargs = {
            "model": "claude-3-5-sonnet-20241022",
            "max_tokens": 4096,
            "system": system or "You are a helpful assistant that can create and modify Unreal Engine Blueprints via tools.",
            "messages": claude_messages,
            "tools": claude_tools,
        }
        if stream:
            with client.messages.stream(**kwargs) as stream_obj:
                for event in stream_obj:
                    if hasattr(event, "type"):
                        if event.type == "content_block_delta":
                            if hasattr(event.delta, "text") and event.delta.text:
                                yield ("content", event.delta.text)
                        elif event.type == "content_block_stop" and hasattr(event, "index"):
                            pass
            # Claude streaming tool_use may come in content_block_start; for simplicity we do one non-stream call when tools needed
            # So we don't stream tool calls for Claude here; fallback to one shot below
        else:
            r = client.messages.create(**kwargs)
            for block in r.content:
                if block.type == "text":
                    yield ("content", block.text)
                if block.type == "tool_use":
                    yield ("tool_call", {"id": block.id, "name": block.name, "args": json.dumps(block.input) if isinstance(block.input, dict) else str(block.input)})
    except Exception as e:
        _log(session_id, f"[Bridge Error] Claude: {e}")
        yield ("error", str(e))

def _parse_json_args(s: str) -> dict:
    try:
        return json.loads(s) if isinstance(s, str) else (s or {})
    except json.JSONDecodeError:
        return {}

# ---------------------------------------------------------------------------
# Orchestrator agent loop (runs in background thread per session)
# ---------------------------------------------------------------------------

def _agent_loop(session_id: str, prompt: str, provider: str) -> None:
    with sessions_lock:
        state = sessions.get(session_id)
    if not state:
        return
    _log(session_id, f"[Bridge] Starting agent (provider: {provider})")
    messages = [
        {"role": "system", "content": "You are a helpful assistant that creates and modifies Unreal Engine Blueprints. Use the provided tools (create_blueprint, create_node, link_nodes, create_component) to fulfill user requests. Always use tools when the user asks for Blueprint creation or modification."},
        {"role": "user", "content": prompt},
    ]
    max_steps = 20
    step = 0
    while step < max_steps:
        step += 1
        content_acc = []
        tool_calls_acc: list[dict] = []
        try:
            # Use non-stream for simplicity so we get a single response with optional tool_calls
            gen = _run_llm_chat(session_id, provider, messages, stream=False)
            for kind, value in gen:
                if kind == "content":
                    content_acc.append(value)
                elif kind == "tool_call":
                    tool_calls_acc.append(value)
                elif kind == "error":
                    state.error = value
                    with state.lock:
                        state.done = True
                    return
        except Exception as e:
            _log(session_id, f"[Bridge Error] {e}")
            with state.lock:
                state.done = True
            return

        if content_acc:
            text = "".join(content_acc)
            _log(session_id, f"Streaming: {text[:200]}{'...' if len(text) > 200 else ''}")
            messages.append({"role": "assistant", "content": text})

        if not tool_calls_acc:
            with state.lock:
                state.done = True
            _log(session_id, "[Bridge] Done.")
            return

        # Hand out one tool call at a time to Unreal; collect results
        assistant_tool_calls = []
        tool_results = []
        for tc in tool_calls_acc:
            tid = tc.get("id") or str(uuid.uuid4())
            name = tc.get("name", "")
            args_str = tc.get("args", "{}")
            if isinstance(args_str, dict):
                args_str = json.dumps(args_str)
            _log(session_id, f"Calling tool: {name} {args_str[:80]}...")
            with state.lock:
                state.pending_tool_calls.append({"id": tid, "name": name, "args": args_str})
            result = state.result_queue.get()
            if result is None:
                break
            assistant_tool_calls.append({"id": tid, "function": {"name": name, "arguments": args_str}})
            tool_results.append({"type": "tool_result", "tool_call_id": tid, "content": result})
            _log(session_id, f"Tool {name} returned: {result[:100]}...")
        messages.append({"role": "assistant", "content": "", "tool_calls": assistant_tool_calls})
        messages.append({"role": "user", "content": tool_results})

    with state.lock:
        state.done = True
    _log(session_id, "[Bridge] Done (max steps reached).")

# ---------------------------------------------------------------------------
# HTTP API
# ---------------------------------------------------------------------------

app = Flask(__name__)

@app.route("/run", methods=["POST"])
def run():
    data = request.get_json() or {}
    prompt = data.get("prompt", "").strip()
    provider = data.get("provider", "Local LLM")
    if not prompt:
        return jsonify({"error": "prompt required"}), 400
    session_id = str(uuid.uuid4())
    with sessions_lock:
        sessions[session_id] = SessionState()
    t = threading.Thread(target=_agent_loop, args=(session_id, prompt, provider))
    t.daemon = True
    t.start()
    return jsonify({"session_id": session_id})

@app.route("/poll", methods=["GET"])
def poll():
    session_id = request.args.get("session", "").strip()
    if not session_id:
        return jsonify({"error": "session required"}), 400
    with sessions_lock:
        state = sessions.get(session_id)
    if not state:
        return jsonify({"error": "session not found", "done": True}), 200
    with state.lock:
        new_lines = state.log_lines[state.last_sent_index:]
        state.last_sent_index = len(state.log_lines)
        tool_call = state.pending_tool_calls[0] if state.pending_tool_calls else None
        done = state.done
        err = state.error
    payload = {"log_lines": new_lines, "done": done}
    if err:
        payload["error"] = err
    if tool_call:
        payload["tool_call"] = {
            "id": tool_call["id"],
            "name": tool_call["name"],
            "args": _parse_json_args(tool_call["args"]),
        }
    return jsonify(payload)

@app.route("/tool_result", methods=["POST"])
def tool_result():
    data = request.get_json() or {}
    session_id = (data.get("session_id") or "").strip()
    tool_call_id = (data.get("tool_call_id") or "").strip()
    result = data.get("result", "")
    if not session_id or not tool_call_id:
        return jsonify({"error": "session_id and tool_call_id required"}), 400
    with sessions_lock:
        state = sessions.get(session_id)
    if not state:
        return jsonify({"error": "session not found"}), 404
    with state.lock:
        for i, tc in enumerate(state.pending_tool_calls):
            if tc["id"] == tool_call_id:
                state.pending_tool_calls.pop(i)
                state.result_queue.put(result)
                return jsonify({"ok": True})
        state.result_queue.put(result)
    return jsonify({"ok": True})

@app.route("/tools", methods=["GET"])
def list_tools():
    """Return tool definitions for MCP/clients (optional)."""
    return jsonify({"tools": get_tools()})

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import os
    port = int(os.environ.get("UNREAL_AI_BRIDGE_PORT", "8765"))
    host = os.environ.get("UNREAL_AI_BRIDGE_HOST", "127.0.0.1")
    print(f"UnrealAI MCP Bridge listening on http://{host}:{port}")
    print("Endpoints: POST /run, GET /poll?session=..., POST /tool_result, GET /tools")
    app.run(host=host, port=port, threaded=True)
