#!/usr/bin/env python3
"""Small stdio MCP bridge for repeatable, project-scoped KiCad CLI checks."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


SERVER = {"name": "oscilla-kicad", "version": "0.1.0"}


TOOLS = [
    {
        "name": "kicad_status",
        "description": "Report whether kicad-cli is installed and its version.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "kicad_run_erc",
        "description": "Run KiCad ERC and write a JSON report inside the selected project root.",
        "inputSchema": {
            "type": "object",
            "required": ["project_root", "schematic"],
            "properties": {
                "project_root": {"type": "string"},
                "schematic": {"type": "string", "description": "Path relative to project_root."},
                "report": {"type": "string", "description": "Optional report path relative to project_root."},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "kicad_run_drc",
        "description": "Run KiCad PCB DRC with schematic parity and write a JSON report inside the project root.",
        "inputSchema": {
            "type": "object",
            "required": ["project_root", "board"],
            "properties": {
                "project_root": {"type": "string"},
                "board": {"type": "string", "description": "Path relative to project_root."},
                "report": {"type": "string", "description": "Optional report path relative to project_root."},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "kicad_render_board",
        "description": "Render a PCB to PNG for visual review. It does not modify the board.",
        "inputSchema": {
            "type": "object",
            "required": ["project_root", "board"],
            "properties": {
                "project_root": {"type": "string"},
                "board": {"type": "string"},
                "output": {"type": "string", "description": "Optional PNG path relative to project_root."},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "kicad_export_fabrication",
        "description": "Export Gerbers, Excellon drill data, a drill map, and drill report to an empty release directory. Use only after review approval.",
        "inputSchema": {
            "type": "object",
            "required": ["project_root", "board", "output_dir"],
            "properties": {
                "project_root": {"type": "string"},
                "board": {"type": "string"},
                "output_dir": {"type": "string", "description": "New, empty directory relative to project_root."},
            },
            "additionalProperties": False,
        },
    },
]


def text_result(value: Any, is_error: bool = False) -> dict[str, Any]:
    text = value if isinstance(value, str) else json.dumps(value, indent=2, sort_keys=True)
    result: dict[str, Any] = {"content": [{"type": "text", "text": text}]}
    if is_error:
        result["isError"] = True
    return result


def error(message: str) -> dict[str, Any]:
    return text_result(message, is_error=True)


def cli_path() -> str | None:
    return shutil.which("kicad-cli")


def run_cli(args: list[str]) -> subprocess.CompletedProcess[str]:
    executable = cli_path()
    if executable is None:
        raise RuntimeError("kicad-cli is not installed or not on PATH. Install KiCad, then restart the MCP host.")
    return subprocess.run([executable, *args], text=True, capture_output=True, check=False, timeout=120)


def project_root(args: dict[str, Any]) -> Path:
    raw = args.get("project_root")
    if not isinstance(raw, str) or not raw:
        raise ValueError("project_root must be a non-empty path.")
    root = Path(raw).expanduser().resolve()
    if not root.is_dir():
        raise ValueError(f"project_root is not a directory: {root}")
    return root


def scoped_path(root: Path, raw: Any, label: str) -> Path:
    if not isinstance(raw, str) or not raw:
        raise ValueError(f"{label} must be a non-empty path relative to project_root.")
    candidate = (root / raw).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"{label} must remain inside project_root.") from exc
    return candidate


def execute(args: list[str]) -> dict[str, Any]:
    completed = run_cli(args)
    return {
        "command": ["kicad-cli", *args],
        "exit_code": completed.returncode,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
    }


def require_file(path: Path, suffix: str) -> None:
    if path.suffix != suffix:
        raise ValueError(f"Expected a {suffix} file, got: {path.name}")
    if not path.is_file():
        raise ValueError(f"File does not exist: {path}")


def handle_tool(name: str, args: dict[str, Any]) -> dict[str, Any]:
    if name == "kicad_status":
        executable = cli_path()
        if executable is None:
            return text_result({"available": False, "message": "Install KiCad and restart the MCP host."})
        completed = run_cli(["version"])
        return text_result({"available": completed.returncode == 0, "path": executable, "version": completed.stdout.strip(), "stderr": completed.stderr.strip()})

    root = project_root(args)
    if name == "kicad_run_erc":
        schematic = scoped_path(root, args.get("schematic"), "schematic")
        require_file(schematic, ".kicad_sch")
        report = scoped_path(root, args.get("report", "build/kicad/erc.json"), "report")
        report.parent.mkdir(parents=True, exist_ok=True)
        result = execute(["sch", "erc", "--format", "json", "--exit-code-violations", "--output", str(report), str(schematic)])
        result["report"] = str(report)
        result["clean"] = result["exit_code"] == 0
        return text_result(result, is_error=result["exit_code"] not in (0, 5))

    if name == "kicad_run_drc":
        board = scoped_path(root, args.get("board"), "board")
        require_file(board, ".kicad_pcb")
        report = scoped_path(root, args.get("report", "build/kicad/drc.json"), "report")
        report.parent.mkdir(parents=True, exist_ok=True)
        result = execute(["pcb", "drc", "--format", "json", "--schematic-parity", "--exit-code-violations", "--output", str(report), str(board)])
        result["report"] = str(report)
        result["clean"] = result["exit_code"] == 0
        return text_result(result, is_error=result["exit_code"] not in (0, 5))

    if name == "kicad_render_board":
        board = scoped_path(root, args.get("board"), "board")
        require_file(board, ".kicad_pcb")
        output = scoped_path(root, args.get("output", "build/kicad/board.png"), "output")
        output.parent.mkdir(parents=True, exist_ok=True)
        result = execute(["pcb", "render", "--width", "1600", "--height", "1200", "--output", str(output), str(board)])
        result["output"] = str(output)
        return text_result(result, is_error=result["exit_code"] != 0)

    if name == "kicad_export_fabrication":
        board = scoped_path(root, args.get("board"), "board")
        require_file(board, ".kicad_pcb")
        output_dir = scoped_path(root, args.get("output_dir"), "output_dir")
        if output_dir.exists() and any(output_dir.iterdir()):
            raise ValueError(f"output_dir must be new or empty: {output_dir}")
        output_dir.mkdir(parents=True, exist_ok=True)
        gerbers = execute(["pcb", "export", "gerbers", "--output", str(output_dir), str(board)])
        drills = execute(["pcb", "export", "drill", "--output", str(output_dir), "--generate-map", "--generate-report", str(board)])
        result = {"output_dir": str(output_dir), "gerbers": gerbers, "drills": drills, "clean": gerbers["exit_code"] == 0 and drills["exit_code"] == 0}
        return text_result(result, is_error=not result["clean"])

    raise ValueError(f"Unknown tool: {name}")


def respond(request: dict[str, Any]) -> dict[str, Any] | None:
    method = request.get("method")
    request_id = request.get("id")
    if method == "notifications/initialized":
        return None
    if method == "initialize":
        return {"jsonrpc": "2.0", "id": request_id, "result": {"protocolVersion": "2024-11-05", "capabilities": {"tools": {}}, "serverInfo": SERVER}}
    if method == "ping":
        return {"jsonrpc": "2.0", "id": request_id, "result": {}}
    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": request_id, "result": {"tools": TOOLS}}
    if method == "tools/call":
        params = request.get("params", {})
        try:
            result = handle_tool(params.get("name", ""), params.get("arguments", {}))
        except (ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
            result = error(str(exc))
        return {"jsonrpc": "2.0", "id": request_id, "result": result}
    if request_id is None:
        return None
    return {"jsonrpc": "2.0", "id": request_id, "error": {"code": -32601, "message": f"Method not found: {method}"}}


def main() -> None:
    for line in sys.stdin:
        try:
            response = respond(json.loads(line))
        except json.JSONDecodeError as exc:
            response = {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": str(exc)}}
        if response is not None:
            print(json.dumps(response), flush=True)


if __name__ == "__main__":
    main()
