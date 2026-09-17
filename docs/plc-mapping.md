# From the C++ controller to a PLC

The existing [Controller.hpp](../src/Controller.hpp) and
[Controller.cpp](../src/Controller.cpp) already separate control decisions from
physical movement. [FactoryFlow.st](../plc/FactoryFlow.st) translates that same
controller into IEC 61131-3 Structured Text (ST). It adds no simulation or control
features. Read the ST file from top to bottom: data types, controller, cyclic caller.

## Concept mapping

| C++ concept | PLC equivalent | Meaning in FactoryFlow |
| --- | --- | --- |
| `Inputs` | A structure of `BOOL` values representing the input process image | A snapshot of sensor and fault signals used for this scan. |
| `Outputs` | A structure of `BOOL` commands representing the output process image | Motor and diverter commands sent to output channels after execution. |
| `Controller` object | An `FB_Controller` function block instance | Owns sequence state across calls. |
| `scan(inputs)` | One execution of the function block body | Checks safety, updates state, then calculates outputs. |
| `State` and `switch` | An enumerated type and `CASE` | Selects one sequence branch per call. |
| Sensor calculations in `main.cpp` | Physical sensors connected to digital input channels | The plant supplies feedback; the controller does not calculate box position. |
| `conveyorMotor` | A digital run command to a drive or motor starter | Requests motion; it is not motor power or a speed setpoint. |
| `diverterA/B/C` | Digital commands to diverter actuators, such as valve solenoids | Only B is selected in this milestone. Commands are not arrival feedback. |
| The approximately 10 ms C++ loop | A cyclic PLC task with a 10 ms interval | Calls the same controller instance once each cycle. |

A function block preserves instance state between executions, which is why it
matches the C++ object. Here `state` is exposed as an output for observation,
like `Controller::state()`; callers should not write it. This scan-to-scan memory
does not imply power-loss retention. See the vendor documentation for
[function blocks](https://www.fernhillsoftware.com/help/iec-61131/common-elements/program-unit/function-block.html).

## Signals and physical I/O

| Input | Current simulator | Physical interpretation |
| --- | --- | --- |
| `entrySensor` | Box is within 0–0.10 m and has not entered Lane B | Box detected at conveyor entry. |
| `sortingSensor` | Box is within 1.95–2.05 m and has not entered Lane B | Box detected near the sorting station. |
| `laneBSensor` | Box has entered Lane B at 2.5 m | Arrival confirmation at Lane B. |
| `emergencyStop` | Boolean input, normally false | Logical stop-active status: TRUE requests FAULT. |
| `motorFault` | Boolean input, normally false | Logical drive/starter fault status: TRUE requests FAULT. |

These are logical signal meanings, not electrical wiring polarities. Hardware I/O
mapping must provide those meanings to the controller. The C++ code treats safety
signals as scan-level inputs; this ST translation has the same scope and is not
a safety-rated emergency-stop implementation.

The ST controller does not know the box ID, conveyor speed, meters, or travel time.
Those belong to the existing C++ plant or to the physical machine. Lane B selection
is fixed; no route lookup is needed. All signals are level-sensitive: there are no
edge detectors, timers, or debounce logic.

## One cyclic execution

The C++ loop initializes sensors from the starting position, then repeats:

1. Pass the current `Inputs` snapshot to `controller.scan(inputs)`.
2. Check the two safety inputs and update the sequence state.
3. Calculate `Outputs` from the resulting state.
4. Apply the outputs to the simulated plant and advance position by 10 ms.
5. Recalculate sensors from position for the next scan.

On a PLC, the physical machine moves independently. The cyclic program reads
mapped inputs, calls `controller(inputs := inputs)`, and copies
`controller.outputs` to mapped outputs. A common runtime model samples inputs at
task start and transfers outputs to the I/O driver at task end; physical bus timing
depends on the target configuration. See
[I/O task timing](https://help.plc.abb.com/buscycle_task_general.html) and
[cyclic task configuration](https://help.plc.abb.com/_cds_obj_task_tab_config_type.html).
There is no sleep or infinite loop inside the ST program: the task schedules it.

## Exact sequence semantics

Before `CASE`, either safety input forces FAULT, including when the previous state
was COMPLETE. Otherwise the following sequence applies:

| State before sequence | Condition | State after sequence |
| --- | --- | --- |
| IDLE | `entrySensor` | TRANSPORTING |
| TRANSPORTING | `sortingSensor` | SORTING |
| SORTING | `laneBSensor` | COMPLETE |
| Any state | No applicable condition | Unchanged |

There is no fall-through or repeated state evaluation. For example, if all three
sensors are already TRUE at startup, three calls are required to reach COMPLETE.
ST uses a [CASE statement](https://www.fernhillsoftware.com/help/iec-61131/structured-text/st-case.html)
with the same branch ordering as the C++ `switch`.

Outputs are calculated **after** that transition, within the same invocation:

| Resulting state | Motor | Diverter A | Diverter B | Diverter C |
| --- | --- | --- | --- | --- |
| IDLE | OFF | OFF | OFF | OFF |
| TRANSPORTING | ON | OFF | OFF | OFF |
| SORTING | ON | OFF | ON | OFF |
| COMPLETE | OFF | OFF | OFF | OFF |
| FAULT | OFF | OFF | OFF | OFF |

The motor remains on during sorting. Arrival feedback, not diverter activation,
causes completion. COMPLETE has no normal exit; a safety fault can still move it
to FAULT. FAULT stays latched when inputs clear. Neither version has a reset input.
A newly initialized controller begins at IDLE; actual PLC restart/initialization
behavior depends on the runtime and its retention settings.

## Using the ST example

`plc/FactoryFlow.st` is readable source, not a vendor-specific project or a
configured hardware deployment. In a PLC editor:

1. Create the three data types, the `FB_Controller` function block, and the
   `PLC_PRG` program from the corresponding sections. Some editors require
   separate objects and separate declaration/body panes rather than a file import.
2. Assign `PLC_PRG` to one cyclic task with a 10 ms interval.
3. Bind `PLC_PRG.inputs` members to logical input channels and
   `PLC_PRG.outputs` members to output channels using the target's I/O mapping.
   No device addresses are prescribed here. Without mapping or manually supplied
   inputs, the default inputs remain FALSE and the controller stays IDLE.
4. Keep the same `controller` instance between scans and call it once per cycle.

The source uses ordinary structs, an enum, `IF`, and `CASE`; it needs no libraries.
The task and I/O setup belong to the chosen PLC tool, so they are described here
rather than adding a vendor project to this milestone.

## Checking equivalence

Both implementations use the same initialized state, safety priority, three
conditional transitions, and four output assignments. To compare them in a PLC
watch window, initialize a fresh instance for each scenario:

| Scenario | Inputs supplied on successive calls | Expected result |
| --- | --- | --- |
| Waiting | All FALSE | IDLE; all outputs OFF. |
| Normal flow | Entry TRUE; sorting TRUE; Lane B TRUE | TRANSPORTING, SORTING, COMPLETE; outputs as above. |
| No arrival yet | Reach SORTING, then all sensors FALSE | Remain SORTING with motor and B ON. |
| Simultaneous sensors | All three sensors TRUE for three calls | TRANSPORTING, then SORTING, then COMPLETE. |
| Emergency stop | Set `emergencyStop` TRUE in any state | FAULT; all outputs OFF in that call. |
| Motor fault | Set `motorFault` TRUE in any state | FAULT; all outputs OFF in that call. |
| Fault clears | Enter FAULT, then clear both fault inputs | Remain FAULT; all outputs OFF. |

This translation has been reviewed against the C++ source. No ST compiler or PLC
runtime is configured in this repository, so CMake/CTest validate only the C++
implementation and simulator, not ST compilation or execution.
