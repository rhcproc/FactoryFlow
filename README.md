# FactoryFlow

FactoryFlow v0.1 is a C++20 industrial conveyor control simulator demonstrating
PLC-style cyclic control. This milestone contains only a controller, a virtual
plant, a console application, and automated tests. No external libraries are required.

For the equivalent PLC implementation, read [the PLC mapping guide](docs/plc-mapping.md)
and [the commented Structured Text source](plc/FactoryFlow.st).

## Build and run

Requires a C++20 compiler and CMake 3.20 or newer.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/factoryflow
```

The application takes approximately 2.5 seconds and logs sensor, output, state,
and routing changes, rather than every scan. Controller tests run without sleeping;
the integration test runs the actual simulator.

## Architecture

The core implementation has just four files, read in this order:

- `src/Controller.hpp`: Inputs, Outputs, State, and the Controller declaration.
- `src/Controller.cpp`: the unchanged PLC state machine and safety logic.
- `src/main.cpp`: physical plant variables, sensor generation, and the cyclic loop.
- `CMakeLists.txt`: builds the application and tests directly, without library layers.

In `main.cpp`, the initial plant position generates the first input image. Each
cycle calls `controller.scan(inputs)`, applies the returned outputs to the plant,
advances position, and updates sensors for the next cycle. Only the plant code in
`main.cpp` changes position; the controller receives no access to it.

`tests/tests.cpp` checks controller sequencing, emergency stop, motor fault, and
input separation. CTest also runs the simulator and checks Lane B arrival,
completion, and outputs switching off. The former standalone plant API tests are
removed because that API is now inline simulation code with a fixed time step.

## Sequence and physical model

BOX-001 begins at 0 m. The entry sensor covers 0–0.10 m. Detection starts
`IDLE -> TRANSPORTING` and enables the conveyor at 1 m/s. The sorting sensor covers
1.95–2.05 m around a station at 2 m. Detection starts `TRANSPORTING -> SORTING`;
the motor stays on and diverter B activates. A simplified gate holds an unselected
box at the station; exclusive selection of B allows travel to 2.5 m, where the
plant records arrival in Lane B.

An additional `laneBSensor` input provides physical arrival confirmation. On the
next scan, the controller transitions `SORTING -> COMPLETE` and disables all
outputs. COMPLETE is terminal for this single-box milestone. Lanes A and C are
reserved output signals and are never activated by this controller.

Emergency stop or motor fault takes priority in every state: the detecting scan
enters FAULT with the motor and all diverters off. FAULT stays latched even if the
input clears; restart the application to start a new run. Tests inject these
conditions through input snapshots. This models scan-level safety logic,
not a certified safety circuit or real drive dynamics. There is no inertia.

The loop uses a fixed 0.01-second simulation step and a steady clock for approximate
wall-clock pacing; it makes no hard real-time guarantee. A 1,000-cycle guard exits
with failure if the sequence does not finish. Sensor widths and gate geometry are
deliberately simple; acceleration, multiple boxes, and additional routes are out of scope.
