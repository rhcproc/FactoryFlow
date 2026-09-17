# FactoryFlow operator HMI

```text
Next.js HMI → HTTP REST → FastAPI gateway → TCP → C++ controller + plant
```

This frontend is an HMI only. Control authority remains in C++. It does not
simulate movement, estimate box position, choose routes, determine faults, or
advance controller states. The state names in the UI are labels and validation
of the received JSON, not a state machine.

All machine indicators come exclusively from `GET /api/status`. Polling runs
approximately every 500 ms, with no overlapping requests and a 2.5-second timeout.
The static schematic highlights sensors and actuator commands. It contains no
moving box or animation. Short states can occur between polls; the HMI does not
invent intermediate states that it did not observe.

START, STOP, and RESET send POST requests to the existing FastAPI endpoints.
Command responses supply acknowledgement/error text only; a fresh status GET
updates machine indicators. The HMI does not optimistically change the state.
Controls are unavailable while disconnected or while a command is pending.
The existing C++ application interprets STOP as its emergency-stop input and
RESET as reinitializing the virtual experiment.

Failed, timed-out, or malformed status responses show **CONTROLLER OFFLINE** and
clear all indicators to UNKNOWN. The last-response timestamp is marked stale.
The display cannot distinguish gateway failure from controller failure; it keeps
polling and recovers automatically when valid telemetry returns. An unconfirmed
command is not retried automatically.

## Run the stack

Use Node.js 20.9+ and npm. This project uses Next.js 15, compatible with the
repository workstation's Node 20 runtime. From the repository root, build and
start C++:

```sh
cmake -S . -B build
cmake --build build
./build/factoryflow --serve
```

In a second terminal, start the gateway (install its requirements as described
in [gateway/README.md](../gateway/README.md) if needed):

```sh
gateway/.venv/bin/python -m uvicorn gateway.main:app --host 127.0.0.1 --port 8000
```

In a third terminal:

```sh
cd frontend
npm ci
npm run dev
```

Open `http://127.0.0.1:3000`. For a production build, use `npm run build` followed
by `npm start` instead of the development server.

Next.js forwards the four `/api/*` paths to `http://127.0.0.1:8000` with
[external rewrites](https://nextjs.org/docs/app/api-reference/config/next-config-js/rewrites).
This keeps browser requests on the same origin without changing FastAPI CORS or
adding a second gateway implementation. There is no Next.js API control logic.
Set `GATEWAY_URL` before starting development or building production to change
the destination. Restart/rebuild after changing it.

## Verification

With Next.js running, from `frontend/`:

```sh
npm run build
npm run typecheck
npx playwright install chromium
npm test
LIVE_HMI=1 npm test -- tests/live.spec.ts
```

The standard browser tests intercept HTTP with explicit test fixtures only; no
fake telemetry is included in the application. They cover forwarding, polling,
GET-only state display, malformed responses, offline recovery, state presentation,
and mobile layout. The optional live test uses the running gateway and C++ process
and operates START/STOP/RESET before confirming Lane B completion.

To check a real outage, stop FastAPI while viewing the HMI. Within the next failed
poll (or timeout), it must show CONTROLLER OFFLINE and UNKNOWN signals. Restart
FastAPI and confirm that live telemetry returns.
With FastAPI stopped, `OFFLINE_HMI=1 npm test -- tests/live.spec.ts` automates
the real-outage check without intercepted responses.

The package override selects a patched PostCSS 8 dependency while retaining
Next.js 15 compatibility with Node 20. The lockfile records the tested versions.

The main screen and polling are in `app/page.tsx`, styling in `app/globals.css`,
and HTTP forwarding in `next.config.ts`. No WebSocket or backend logic is added.
