# FactoryFlow development

- Keep this milestone limited to the controller, simulator, PLC mapping, TCP/FastAPI gateway, Next.js HMI, and tests.
- The frontend displays GET /api/status telemetry only and forwards commands; it must not simulate or control plant behavior locally.
- The controller reads Inputs and returns Outputs. It must not depend on or mutate the plant.
- Keep control logic in src/Controller.hpp and src/Controller.cpp; transport belongs in src/TcpServer.*.
- FastAPI only forwards requests. C++ owns commands, plant state, and control decisions.
- The inline plant simulation in main.cpp owns position, movement, routing, and sensors.
- Keep Inputs, Outputs, and State in Controller.hpp; favor a readable loop over abstractions.
- Safety inputs take priority on every scan. FAULT is latched and all outputs are off.
- Keep dependencies minimal and hardware I/O concepts explicit.
- Validate changes with `cmake -S . -B build`, `cmake --build build`,
  `ctest --test-dir build --output-on-failure`, and `./build/factoryflow`.
- Run gateway tests with `gateway/.venv/bin/python -m pytest gateway/tests -q` after building C++.
