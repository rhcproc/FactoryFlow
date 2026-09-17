import { expect, test } from "@playwright/test";

test("live HMI → FastAPI → TCP → C++ commands and completion", async ({ page }) => {
  test.skip(process.env.LIVE_HMI !== "1", "Requires the running C++ and FastAPI processes");
  await page.goto("/");
  await expect(page.getByTestId("connection")).toHaveText("CONTROLLER ONLINE");
  await page.getByRole("button", { name: "RESET", exact: true }).click();
  await expect(page.getByTestId("controller-state")).toHaveText("IDLE");
  await page.getByRole("button", { name: "START", exact: true }).click();
  await expect(page.getByTestId("controller-state")).toHaveText("TRANSPORTING");
  await page.getByRole("button", { name: "STOP", exact: true }).click();
  await expect(page.getByTestId("controller-state")).toHaveText("FAULT");
  await expect(page.locator(".fault-panel")).toBeVisible();
  await expect(page.locator(".signal").filter({ hasText: "Emergency stop" })).toContainText("ON");
  await page.getByRole("button", { name: "RESET", exact: true }).click();
  await expect(page.getByTestId("controller-state")).toHaveText("IDLE");
  await page.getByRole("button", { name: "START", exact: true }).click();
  await expect(page.getByTestId("controller-state")).toHaveText("COMPLETE", { timeout: 8000 });
  await expect(page.locator(".arrival")).toContainText("ON");
  await expect(page.locator(".signal").filter({ hasText: "Conveyor motor" })).toContainText("OFF");
  await page.screenshot({ path: "test-results/hmi-desktop.png", fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({ path: "test-results/hmi-mobile.png", fullPage: true });
});

test("real gateway outage displays offline without stale telemetry", async ({ page }) => {
  test.skip(process.env.OFFLINE_HMI !== "1", "Run with FastAPI stopped and Next.js running");
  await page.goto("/");
  await expect(page.getByTestId("connection")).toHaveText("CONTROLLER OFFLINE", { timeout: 8000 });
  await expect(page.getByTestId("controller-state")).toHaveText("—");
  await expect(page.getByRole("button", { name: "START", exact: true })).toBeDisabled();
  await expect(page.locator(".signal strong").first()).toHaveText("UNKNOWN");
});
