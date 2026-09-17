# FactoryFlow gateway

```text
HMI -> REST -> FastAPI -> TCP -> C++ Controller + Plant Simulator
```

The HMI is an external caller, not a UI included in this milestone. FastAPI only
translates HTTP requests to TCP messages and returns the C++ response. It contains
no sensor simulation, conveyor rules, state transitions, or fault decisions.
C++ remains the owner of the machine state and runs its approximately 10 ms cycle
independently of HTTP requests. A gateway failure does not stop the C++ loop.

## Run locally

From the repository root (Python 3.10+, C++20, CMake 3.20+):

```sh
cmake -S . -B build
cmake --build build
python3 -m venv gateway/.venv
gateway/.venv/bin/python -m pip install -r gateway/requirements.txt
./build/factoryflow --serve
```

In another terminal:

```sh
gateway/.venv/bin/python -m uvicorn gateway.main:app --host 127.0.0.1 --port 8000
```

The C++ TCP server listens on `127.0.0.1:9000`. Service mode starts IDLE and waits
for start; it remains available in COMPLETE and FAULT. Stop the processes with
Ctrl-C. Running `./build/factoryflow` without arguments retains the standalone
automatic demo. The TCP implementation uses POSIX sockets (macOS/Linux).

```sh
curl http://127.0.0.1:8000/api/status
curl -X POST http://127.0.0.1:8000/api/start
curl -X POST http://127.0.0.1:8000/api/stop
curl -X POST http://127.0.0.1:8000/api/reset
```

No request bodies are required. All successful endpoints return the C++ status
snapshot: `state`, `inputs`, and `outputs`. Status reflects the completed cycle,
including sensors updated after plant movement. There is no gateway-side cache.
`FACTORYFLOW_HOST` and `FACTORYFLOW_PORT` configure the gateway's TCP destination;
the bundled C++ server uses the fixed loopback address above.

## Command semantics belong to C++

| Command | C++ application behavior |
| --- | --- |
| `status` | Read the current snapshot without changing the run. |
| `start` | Enable cyclic scans; repeated start during a run is harmless. COMPLETE or FAULT returns `reset_required`. |
| `stop` | Set the existing simulated emergency-stop input. The unchanged controller scans it, latches FAULT, and disables all outputs before acknowledgement. |
| `reset` | Reinitialize the virtual experiment: new Controller, box at entry, simulated faults cleared, outputs off, waiting for start. Can also abort an active virtual run. |

The existing controller has no normal-stop or reset inputs. These commands are
therefore simulator lifecycle operations in `main.cpp`, not additions to
`Controller::scan()` or the ST controller. Stop is deliberately not pause/resume.
Reset here resets a virtual plant; it is not a physical machine reset procedure.

## TCP protocol

One connection carries one request and one response, then the server closes it.
Each frame is UTF-8 JSON followed by LF (`\n`). Requests are deliberately restricted
to these four literal JSON strings, with optional surrounding spaces/tabs/CR:

```text
"status"\n
"start"\n
"stop"\n
"reset"\n
```

Here `\n` denotes an actual newline byte. Requests are not JSON objects. Escaped
spellings, extra fields, multiple requests per connection, and other tokens are
not supported. The C++ implementation matches these fixed tokens without a JSON
library. A request may be split across TCP packets; the newline ends the frame.
The request limit is 128 bytes including LF. Invalid requests return
`{"error":"invalid_request"}`; rejected starts return
`{"error":"reset_required"}`. Neither executes the requested operation.

Example successful response (one line on the wire):

```json
{"state":"TRANSPORTING","inputs":{"entrySensor":false,"sortingSensor":false,"laneBSensor":false,"emergencyStop":false,"motorFault":false},"outputs":{"conveyorMotor":true,"diverterA":false,"diverterB":false,"diverterC":false}}
```

The server handles one active connection at a time with nonblocking reads/writes
and a one-second connection deadline, continuing scans while waiting for bytes.
The gateway uses a two-second socket timeout and a 4 KiB response limit. TCP
disconnects do not reset or stop the simulation. There are no automatic retries:
after a lost reply, a command may already have executed; query status before retrying.

| HTTP result | Meaning |
| --- | --- |
| 200 | C++ accepted the request and returned a snapshot. |
| 409 | C++ rejected the command; `detail` contains its error. |
| 502 | C++ returned an invalid or incomplete protocol response. |
| 503 | C++ connection failed or timed out. |

## Tests

Stop manually launched services before integration tests (they use port 9000).

```sh
ctest --test-dir build --output-on-failure
gateway/.venv/bin/python -m pytest gateway/tests -q
```

Gateway unit tests verify forwarding and error translation, using FastAPI's
[TestClient](https://fastapi.tiangolo.com/tutorial/testing/). Integration tests
launch the built C++ process, exercise all endpoints, check Lane B completion,
and test fragmented, invalid, and stalled TCP requests. They skip if the C++
executable has not been built. No control algorithm is reproduced in Python.
