"""Run against the actual built C++ process, through FastAPI's HTTP test client."""
import json
from pathlib import Path
import socket
import subprocess
import time

import pytest
from fastapi.testclient import TestClient

from gateway import main


@pytest.fixture
def live_client(monkeypatch):
    root = Path(__file__).resolve().parents[2]
    executable = root / "build" / "factoryflow"
    if not executable.exists():
        pytest.skip("Build the C++ application first")
    monkeypatch.setattr(main, "CONTROLLER_HOST", "127.0.0.1")
    monkeypatch.setattr(main, "CONTROLLER_PORT", 9000)
    process = subprocess.Popen([str(executable), "--serve"], stdout=subprocess.DEVNULL)
    try:
        with TestClient(main.app) as client:
            for _ in range(100):
                assert process.poll() is None, "C++ server exited (is port 9000 already in use?)"
                if client.get("/api/status").status_code == 200:
                    break
                time.sleep(0.02)
            else:
                pytest.fail("C++ server did not become ready")
            yield client
    finally:
        process.terminate()
        process.wait(timeout=5)


def test_commands_and_lane_b(live_client):
    client = live_client
    initial = client.get("/api/status").json()
    assert initial["state"] == "IDLE"
    assert initial["inputs"]["entrySensor"] is True
    assert not any(initial["outputs"].values())
    started = client.post("/api/start").json()
    assert started["state"] == "TRANSPORTING"
    assert started["outputs"]["conveyorMotor"] is True
    stopped = client.post("/api/stop").json()
    assert stopped["state"] == "FAULT"
    assert stopped["inputs"]["emergencyStop"] is True
    assert not any(stopped["outputs"].values())
    assert client.post("/api/start").status_code == 409
    assert client.get("/api/status").json()["state"] == "FAULT"
    assert client.post("/api/reset").json() == initial
    assert client.post("/api/start").status_code == 200
    # A client that never completes its request must not block plant motion.
    with socket.create_connection(("127.0.0.1", 9000), timeout=2) as slow:
        slow.sendall(b'"sta')
        time.sleep(3)
    complete = client.get("/api/status").json()
    assert complete["state"] == "COMPLETE"
    assert complete["inputs"]["laneBSensor"] is True
    assert not any(complete["outputs"].values())
    assert client.post("/api/start").status_code == 409
    assert client.post("/api/reset").json() == initial


def test_tcp_framing(live_client):
    with socket.create_connection(("127.0.0.1", 9000), timeout=2) as connection:
        connection.sendall(b'"sta')
        time.sleep(0.03)
        connection.sendall(b'tus"\n')
        with connection.makefile("rb") as stream:
            assert json.loads(stream.readline())["state"] == "IDLE"
    for frame in (b'{"command":"start"}\n', b'"unknown"\n', b'x' * 129):
        with socket.create_connection(("127.0.0.1", 9000), timeout=2) as connection:
            connection.sendall(frame)
            with connection.makefile("rb") as stream:
                assert json.loads(stream.readline()) == {"error": "invalid_request"}
    assert live_client.get("/api/status").json()["state"] == "IDLE"
