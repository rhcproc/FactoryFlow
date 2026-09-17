"use client";

import { useCallback, useEffect, useRef, useState } from "react";

const states = ["IDLE", "TRANSPORTING", "SORTING", "COMPLETE", "FAULT"] as const;
type Telemetry = {
  state: typeof states[number];
  inputs: {
    entrySensor: boolean; sortingSensor: boolean; laneBSensor: boolean;
    emergencyStop: boolean; motorFault: boolean;
  };
  outputs: {
    conveyorMotor: boolean; diverterA: boolean; diverterB: boolean; diverterC: boolean;
  };
};
type Command = "start" | "stop" | "reset";

// Validate the wire shape, not machine behavior. Missing data is never treated as OFF.
function isTelemetry(value: unknown): value is Telemetry {
  if (!value || typeof value !== "object") return false;
  const data = value as Record<string, unknown>;
  const bools = (group: unknown, keys: string[]) => !!group && typeof group === "object"
    && keys.every((key) => typeof (group as Record<string, unknown>)[key] === "boolean");
  return states.some((state) => state === data.state)
    && bools(data.inputs, ["entrySensor", "sortingSensor", "laneBSensor", "emergencyStop", "motorFault"])
    && bools(data.outputs, ["conveyorMotor", "diverterA", "diverterB", "diverterC"]);
}

function Signal({ label, value, alarm = false }: { label: string; value: boolean | undefined; alarm?: boolean }) {
  return <div className={`signal ${value ? (alarm ? "alarm" : "active") : ""}`}>
    <span className="signal-name"><i className="lamp" aria-hidden="true" />{label}</span>
    <strong>{value === undefined ? "UNKNOWN" : value ? "ON" : "OFF"}</strong>
  </div>;
}

export default function Hmi() {
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null);
  const [connection, setConnection] = useState<"connecting" | "online" | "offline">("connecting");
  const [updated, setUpdated] = useState<string | null>(null);
  const [pending, setPending] = useState<Command | null>(null);
  const [message, setMessage] = useState("");
  const active = useRef(false);
  const statusRequest = useRef<AbortController | null>(null);
  const commandRequest = useRef<AbortController | null>(null);

  const offline = useCallback(() => {
    setTelemetry(null);
    setConnection("offline");
  }, []);

  const refresh = useCallback(async () => {
    if (statusRequest.current || commandRequest.current) return;
    const request = new AbortController();
    statusRequest.current = request;
    const timeout = setTimeout(() => request.abort(), 2500);
    try {
      const response = await fetch("/api/status", { cache: "no-store", signal: request.signal });
      if (!response.ok) throw new Error("Status unavailable");
      const data: unknown = await response.json();
      if (!isTelemetry(data)) throw new Error("Invalid telemetry");
      if (active.current && statusRequest.current === request) {
        setTelemetry(data);
        setConnection("online");
        setUpdated(new Date().toLocaleTimeString());
      }
    } catch {
      if (active.current && statusRequest.current === request) offline();
    } finally {
      clearTimeout(timeout);
      if (statusRequest.current === request) statusRequest.current = null;
    }
  }, [offline]);

  useEffect(() => {
    active.current = true;
    void refresh();
    const interval = setInterval(() => void refresh(), 500);
    return () => {
      active.current = false;
      clearInterval(interval);
      statusRequest.current?.abort();
      statusRequest.current = null;
      commandRequest.current?.abort();
      commandRequest.current = null;
    };
  }, [refresh]);

  async function send(command: Command) {
    if (commandRequest.current) return;
    // Invalidate an older poll; only a new GET may update displayed telemetry.
    statusRequest.current?.abort();
    statusRequest.current = null;
    const request = new AbortController();
    commandRequest.current = request;
    setPending(command);
    setMessage("");
    const timeout = setTimeout(() => request.abort(), 2500);
    try {
      const response = await fetch(`/api/${command}`, { method: "POST", signal: request.signal });
      if (response.ok) {
        setMessage(`${command.toUpperCase()} acknowledged by controller.`);
      } else if (response.status === 409) {
        const error = await response.json();
        setMessage(`Command rejected: ${typeof error.detail === "string" ? error.detail : "controller rejected request"}`);
      } else {
        throw new Error("Command unavailable");
      }
    } catch {
      if (active.current) {
        offline();
        setMessage("Command not confirmed. Check live status before retrying.");
      }
    } finally {
      clearTimeout(timeout);
      commandRequest.current = null;
      if (active.current) {
        setPending(null);
        void refresh();
      }
    }
  }

  const inputs = telemetry?.inputs;
  const outputs = telemetry?.outputs;
  const fault = telemetry?.state === "FAULT";

  return <main>
    <header className="topbar">
      <div className="brand"><span className="brand-mark" aria-hidden="true">F<span>F</span></span>
        <div><h1>FactoryFlow</h1><p>CONVEYOR CONTROL / OPERATOR HMI</p></div>
      </div>
      <div className={`connection ${connection}`} role="status" data-testid="connection">
        <i className="lamp" aria-hidden="true" />
        {connection === "online" ? "CONTROLLER ONLINE" : connection === "offline" ? "CONTROLLER OFFLINE" : "CONNECTING TO CONTROLLER"}
      </div>
    </header>

    <section className={`state-panel ${fault ? "fault-panel" : ""}`} aria-label="Controller status">
      <div><p className="eyebrow">01 / CONTROLLER STATE</p>
        <h2 data-testid="controller-state" aria-live="polite">{telemetry?.state ?? "—"}</h2>
        <p>{fault ? "Controller reports FAULT. Review the safety inputs below." : connection === "online"
          ? "Live state reported by the C++ controller."
          : "Waiting for valid telemetry. Machine state is unknown."}</p>
      </div>
      <div className="state-list" aria-label="Controller state indicators">
        {states.map((state) => <span key={state} className={telemetry?.state === state ? "selected" : ""}>
          <i aria-hidden="true" />{state}</span>)}
      </div>
    </section>

    <div className="workspace">
      <section className="panel process-panel" aria-labelledby="process-title">
        <div className="panel-heading"><div><p className="eyebrow">02 / PROCESS OVERVIEW</p><h2 id="process-title">Conveyor & routing</h2></div>
          <span className="tag">LIVE I/O</span></div>
        <div className="process" aria-label="Conveyor schematic">
          <div className={`station ${inputs?.entrySensor ? "lit" : ""}`}><span className="station-symbol">E</span><strong>Entry Sensor</strong><small>{inputs === undefined ? "UNKNOWN" : inputs.entrySensor ? "DETECTED" : "CLEAR"}</small></div>
          <span className="arrow" aria-hidden="true">→</span>
          <div className={`conveyor ${outputs?.conveyorMotor ? "lit" : ""}`}><div className="rollers" aria-hidden="true">○ ○ ○ ○</div><strong>Conveyor</strong><small>{outputs === undefined ? "UNKNOWN" : outputs.conveyorMotor ? "MOTOR ON" : "MOTOR OFF"}</small></div>
          <span className="arrow" aria-hidden="true">→</span>
          <div className={`station ${inputs?.sortingSensor ? "lit" : ""}`}><span className="station-symbol">S</span><strong>Sorting Sensor</strong><small>{inputs === undefined ? "UNKNOWN" : inputs.sortingSensor ? "DETECTED" : "CLEAR"}</small></div>
          <span className="arrow" aria-hidden="true">→</span>
          <div className="lanes">
            {(["A", "B", "C"] as const).map((lane) => {
              const value = outputs?.[`diverter${lane}`];
              return <div key={lane} className={`lane ${value ? "lit" : ""}`}><strong>Lane {lane}</strong><span>Diverter {value === undefined ? "UNKNOWN" : value ? "ON" : "OFF"}</span></div>;
            })}
          </div>
        </div>
        <div className="arrival"><Signal label="Lane B sensor" value={inputs?.laneBSensor} /></div>
        <p className="caption">Static process schematic · Indicators reflect received I/O only.</p>
      </section>

      <section className="panel controls" aria-labelledby="controls-title">
        <p className="eyebrow">03 / OPERATOR COMMANDS</p><h2 id="controls-title">Run controls</h2>
        <p>Commands are sent to the controller through the gateway.</p>
        <div className="buttons">{(["start", "stop", "reset"] as const).map((command) =>
          <button key={command} className={`command ${command}`} disabled={connection !== "online" || pending !== null}
            onClick={() => void send(command)}>{pending === command ? "SENDING…" : command.toUpperCase()}<span aria-hidden="true">{command === "start" ? "▶" : command === "stop" ? "■" : "↺"}</span></button>
        )}</div>
        <p className="control-note">STOP requests the controller’s emergency-stop input. RESET restarts the virtual experiment.</p>
        <p className="command-message" role="status">{message}</p>
      </section>
    </div>

    <section className="io-grid" aria-label="Live input and output signals">
      <div className="panel"><p className="eyebrow">DIGITAL INPUTS</p><h2>Position sensors</h2>
        <Signal label="Entry sensor" value={inputs?.entrySensor} /><Signal label="Sorting sensor" value={inputs?.sortingSensor} /><Signal label="Lane B sensor" value={inputs?.laneBSensor} /></div>
      <div className="panel"><p className="eyebrow">DIGITAL OUTPUTS</p><h2>Actuators</h2>
        <Signal label="Conveyor motor" value={outputs?.conveyorMotor} /><Signal label="Diverter A" value={outputs?.diverterA} /><Signal label="Diverter B" value={outputs?.diverterB} /><Signal label="Diverter C" value={outputs?.diverterC} /></div>
      <div className="panel"><p className="eyebrow">SAFETY INPUTS</p><h2>Stop & fault signals</h2>
        <Signal label="Emergency stop" value={inputs?.emergencyStop} alarm /><Signal label="Motor fault" value={inputs?.motorFault} alarm />
        <p className="caption">Reported directly by the controller process.</p></div>
    </section>
    <footer><span>FACTORYFLOW / LOCAL CONTROL STATION</span><span>Polling 500 ms · Last valid response: {updated ?? "none"}{connection === "offline" && updated ? " (stale)" : ""}</span></footer>
  </main>;
}
