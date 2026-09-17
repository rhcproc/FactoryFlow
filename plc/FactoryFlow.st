(*
    FactoryFlow - PLC Controller
    IEC 61131-3 Structured Text

    C++ FactoryFlow controller를 PLC 방식으로 구현한 예제.
*)

(************************************************************
 * 1. DATA TYPES
 ************************************************************)

TYPE
    FF_Inputs : STRUCT
        entrySensor    : BOOL := FALSE;
        sortingSensor  : BOOL := FALSE;
        laneBSensor    : BOOL := FALSE;

        emergencyStop  : BOOL := FALSE;
        motorFault     : BOOL := FALSE;

        resetButton    : BOOL := FALSE;
    END_STRUCT;


    FF_Outputs : STRUCT
        conveyorMotor  : BOOL := FALSE;

        diverterA      : BOOL := FALSE;
        diverterB      : BOOL := FALSE;
        diverterC      : BOOL := FALSE;
    END_STRUCT;


    FF_State :
    (
        IDLE,
        TRANSPORTING,
        SORTING,
        COMPLETE,
        FAULT
    );
END_TYPE



(************************************************************
 * 2. CONTROLLER
 *
 * C++:
 *
 *     Controller controller;
 *
 * 와 비슷한 역할
 ************************************************************)

FUNCTION_BLOCK FB_Controller

VAR_INPUT
    inputs : FF_Inputs;
END_VAR


VAR_OUTPUT
    outputs : FF_Outputs;

    state : FF_State := IDLE;

    jamDetected : BOOL := FALSE;
END_VAR


VAR
    (* PLC On-delay Timer *)
    jamTimer : TON;
END_VAR



(************************************************************
 * 3. JAM TIMER
 *
 * TRANSPORTING 상태가 5초 이상 지속되면
 * conveyor jam으로 간주한다.
 ************************************************************)

jamTimer(
    IN := (state = TRANSPORTING),
    PT := T#5s
);


IF jamTimer.Q THEN

    (* Fault latch *)
    jamDetected := TRUE;

END_IF;



(************************************************************
 * 4. SAFETY / FAULT CHECK
 *
 * C++:
 *
 * if (inputs.emergencyStop ||
 *     inputs.motorFault ||
 *     jamDetected)
 * {
 *     state = State::FAULT;
 * }
 ************************************************************)

IF inputs.emergencyStop
   OR inputs.motorFault
   OR jamDetected THEN

    state := FAULT;

END_IF;



(************************************************************
 * 5. STATE MACHINE
 ************************************************************)

CASE state OF


    (************************************************
     * IDLE
     ************************************************)

    IDLE:

        IF inputs.entrySensor THEN

            state := TRANSPORTING;

        END_IF;



    (************************************************
     * TRANSPORTING
     ************************************************)

    TRANSPORTING:

        IF inputs.sortingSensor THEN

            state := SORTING;

        END_IF;



    (************************************************
     * SORTING
     ************************************************)

    SORTING:

        IF inputs.laneBSensor THEN

            state := COMPLETE;

        END_IF;



    (************************************************
     * COMPLETE
     ************************************************)

    COMPLETE:

        (*
            현재 demo에서는 작업 완료 후
            아무 동작도 하지 않는다.
        *)



    (************************************************
     * FAULT
     ************************************************)

    FAULT:

        (*
            FAULT는 자동으로 해제되지 않는다.

            Operator가 Reset을 누르고
            실제 fault condition이 제거된 경우에만
            IDLE로 돌아간다.
        *)

        IF inputs.resetButton
           AND NOT inputs.emergencyStop
           AND NOT inputs.motorFault THEN

            jamDetected := FALSE;

            state := IDLE;

        END_IF;


END_CASE;



(************************************************************
 * 6. OUTPUT LOGIC
 *
 * 모든 scan에서 output 값을 다시 계산한다.
 ************************************************************)

outputs.conveyorMotor :=
    (state = TRANSPORTING)
    OR
    (state = SORTING);


outputs.diverterA := FALSE;


outputs.diverterB :=
    (state = SORTING);


outputs.diverterC := FALSE;



(************************************************************
 * 7. SAFETY OUTPUT OVERRIDE
 *
 * FAULT 상태에서는 physical output을 모두 OFF.
 ************************************************************)

IF state = FAULT THEN

    outputs.conveyorMotor := FALSE;

    outputs.diverterA := FALSE;
    outputs.diverterB := FALSE;
    outputs.diverterC := FALSE;

END_IF;


END_FUNCTION_BLOCK



(************************************************************
 * 8. PLC MAIN PROGRAM
 ************************************************************)

PROGRAM PLC_PRG

VAR

    (*
        Controller instance

        C++:
            Controller controller;
    *)

    controller : FB_Controller;


    (*
        실제 시스템에서는 EtherCAT input에 mapping
    *)

    inputs : FF_Inputs;


    (*
        실제 시스템에서는 EtherCAT output에 mapping
    *)

    outputs : FF_Outputs;

END_VAR



(************************************************************
 * PLC CYCLIC SCAN
 *
 * PLC task에서 예를 들어 10 ms마다 PLC_PRG가 실행된다.
 ************************************************************)

controller(
    inputs := inputs
);


outputs := controller.outputs;


END_PROGRAM