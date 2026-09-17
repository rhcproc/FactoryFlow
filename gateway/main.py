"""HTTP/TCP translation only; C++ owns all machine behavior."""

import json
import os
import socket

from fastapi import FastAPI, HTTPException

app = FastAPI(title="FactoryFlow Gateway")
CONTROLLER_HOST = os.getenv("FACTORYFLOW_HOST", "127.0.0.1")
CONTROLLER_PORT = int(os.getenv("FACTORYFLOW_PORT", "9000"))


def forward(command: str) -> dict:
    try:
        with socket.create_connection((CONTROLLER_HOST, CONTROLLER_PORT), timeout=2) as connection:
            connection.sendall(json.dumps(command).encode("ascii") + b"\n")
            with connection.makefile("rb") as stream:
                frame = stream.readline(4097)
    except OSError as exc:
        raise HTTPException(503, "C++ controller unavailable or timed out") from exc

    try:
        if len(frame) > 4096 or not frame.endswith(b"\n"):
            raise ValueError("Invalid response framing")
        response = json.loads(frame)
        if not isinstance(response, dict):
            raise ValueError("Expected JSON object")
        if "error" in response:
            if not isinstance(response["error"], str):
                raise ValueError("Invalid error response")
        elif not (isinstance(response.get("state"), str)
                  and isinstance(response.get("inputs"), dict)
                  and isinstance(response.get("outputs"), dict)):
            raise ValueError("Missing status fields")
    except (ValueError, UnicodeError) as exc:
        raise HTTPException(502, "Invalid response from C++ controller") from exc

    if "error" in response:
        raise HTTPException(409, response["error"])
    return response


@app.get("/api/status")
def status():
    return forward("status")


@app.post("/api/start")
def start():
    return forward("start")


@app.post("/api/stop")
def stop():
    return forward("stop")


@app.post("/api/reset")
def reset():
    return forward("reset")
