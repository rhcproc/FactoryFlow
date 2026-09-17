import io
from unittest.mock import MagicMock

import pytest
from fastapi.testclient import TestClient

from gateway import main


@pytest.fixture
def client():
    return TestClient(main.app)


@pytest.mark.parametrize("method,command", [
    ("get", "status"), ("post", "start"), ("post", "stop"), ("post", "reset")
])
def test_forwards_exact_command_and_response(monkeypatch, client, method, command):
    connection = MagicMock()
    connection.__enter__.return_value = connection
    connection.makefile.return_value = io.BytesIO(
        b'{"state":"IDLE","inputs":{"entrySensor":true},"outputs":{"conveyorMotor":false}}\n'
    )
    monkeypatch.setattr(main.socket, "create_connection", lambda *a, **k: connection)
    response = getattr(client, method)(f"/api/{command}")
    assert response.status_code == 200
    assert response.json() == {
        "state": "IDLE", "inputs": {"entrySensor": True},
        "outputs": {"conveyorMotor": False}
    }
    connection.sendall.assert_called_once_with(f'"{command}"\n'.encode())


@pytest.mark.parametrize("error", [ConnectionRefusedError(), TimeoutError()])
@pytest.mark.parametrize("command", ["status", "start", "stop", "reset"])
def test_unavailable(monkeypatch, client, error, command):
    def unavailable(*args, **kwargs):
        raise error
    monkeypatch.setattr(main.socket, "create_connection", unavailable)
    response = client.get("/api/status") if command == "status" else client.post(f"/api/{command}")
    assert response.status_code == 503


@pytest.mark.parametrize("frame,code", [
    (b'not json\n', 502), (b'{}\n', 502), (b'[]\n', 502),
    (b'', 502), (b'{}', 502), (b'x' * 4097, 502),
    (b'{"error":"reset_required"}\n', 409),
])
def test_protocol_errors(monkeypatch, client, frame, code):
    connection = MagicMock()
    connection.__enter__.return_value = connection
    connection.makefile.return_value = io.BytesIO(frame)
    monkeypatch.setattr(main.socket, "create_connection", lambda *a, **k: connection)
    assert client.post("/api/start").status_code == code
