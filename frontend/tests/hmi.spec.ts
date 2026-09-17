import { expect, test } from "@playwright/test";

const snapshot = {
  state: "IDLE",
  inputs: { entrySensor: true, sortingSensor: false, laneBSensor: false, emergencyStop: false, motorFault: false },
  outputs: { conveyorMotor: false, diverterA: false, diverterB: false, diverterC: false },
};

test("only GET telemetry changes the display; commands are forwarded", async ({ page }) => {
  let reads = 0;
  const commands: string[] = [];
  await page.route("**/api/status", async (route) => {
    reads++;
    await route.fulfill({ json: snapshot });
  });
  await page.route(/\/api\/(start|stop|reset)$/, async (route) => {
    expect(route.request().method()).toBe("POST");
    commands.push(route.request().url().split("/").pop()!);
    // Deliberately different: command responses must not drive the display.
    await route.fulfill({ json: { ...snapshot, state: "FAULT" } });
  });
  await page.goto("/");
  await expect(page.getByTestId("connection")).toHaveText("CONTROLLER ONLINE");
  for (const command of ["START", "STOP", "RESET"]) {
    await page.getByRole("button", { name: command, exact: true }).click();
    await expect(page.getByText(`${command} acknowledged by controller.`)).toBeVisible();
    await expect(page.getByTestId("controller-state")).toHaveText("IDLE");
  }
  expect(commands).toEqual(["start", "stop", "reset"]);
  const before = reads;
  await expect.poll(() => reads).toBeGreaterThan(before + 1);
});

test("offline clears stale signals, disables commands, and recovers", async ({ page }) => {
  let online = true;
  await page.route("**/api/status", async (route) => {
    await route.fulfill(online ? { json: snapshot } : { status: 503, json: { detail: "unavailable" } });
  });
  await page.goto("/");
  await expect(page.getByTestId("controller-state")).toHaveText("IDLE");
  online = false;
  await expect(page.getByTestId("connection")).toHaveText("CONTROLLER OFFLINE");
  await expect(page.getByTestId("controller-state")).toHaveText("—");
  await expect(page.locator(".signal strong").first()).toHaveText("UNKNOWN");
  for (const name of ["START", "STOP", "RESET"]) await expect(page.getByRole("button", { name, exact: true })).toBeDisabled();
  online = true;
  await expect(page.getByTestId("connection")).toHaveText("CONTROLLER ONLINE");
  await expect(page.getByTestId("controller-state")).toHaveText("IDLE");
});

test("malformed telemetry is unknown, never a false OFF signal", async ({ page }) => {
  await page.route("**/api/status", (route) => route.fulfill({ json: { state: "IDLE", inputs: {}, outputs: {} } }));
  await page.goto("/");
  await expect(page.getByTestId("connection")).toHaveText("CONTROLLER OFFLINE");
  await expect(page.locator(".signal strong").first()).toHaveText("UNKNOWN");
});

test("all controller states are rendered directly and faults are prominent", async ({ page }) => {
  let state = "IDLE";
  await page.route("**/api/status", (route) => route.fulfill({ json: { ...snapshot, state } }));
  await page.goto("/");
  for (const next of ["IDLE", "TRANSPORTING", "SORTING", "COMPLETE", "FAULT"]) {
    state = next;
    await expect(page.getByTestId("controller-state")).toHaveText(next);
  }
  await expect(page.locator(".fault-panel")).toBeVisible();
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true);
  await expect(page.getByRole("button", { name: "STOP", exact: true })).toBeVisible();
});

test("controller rejection is shown without inventing a state transition", async ({ page }) => {
  await page.route("**/api/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/api/start", (route) => route.fulfill({ status: 409, json: { detail: "reset_required" } }));
  await page.goto("/");
  await page.getByRole("button", { name: "START", exact: true }).click();
  await expect(page.getByText("Command rejected: reset_required")).toBeVisible();
  await expect(page.getByTestId("controller-state")).toHaveText("IDLE");
});
