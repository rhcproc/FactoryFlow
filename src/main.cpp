#include "Controller.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

using namespace factoryflow;

namespace {
void logChange(const char* name, bool previous, bool current) {
    if (previous != current) {
        std::cout << name << " = " << (current ? "ON" : "OFF") << '\n';
    }
}
}

int main() {
    constexpr double dt = 0.01;
    Controller controller;
    // Simple virtual plant. Only this loop owns and changes physical position.
    constexpr double conveyorSpeed = 1.0; // meters per second
    constexpr double sortingPosition = 2.0; // meters
    constexpr double laneBPosition = 2.5; // meters
    double boxPosition = 0.0;
    bool boxInLaneB = false;
    Inputs previousInputs;
    Outputs previousOutputs;
    Inputs inputs;
    inputs.entrySensor = boxPosition <= 0.10;
    auto nextCycle = std::chrono::steady_clock::now();
    std::cout << "FactoryFlow v0.1\nSYSTEM READY\n" << "BOX-001 created\n";

    for (int cycle = 0; cycle < 1000; ++cycle) {
        logChange("EntrySensor", previousInputs.entrySensor, inputs.entrySensor);
        logChange("SortingSensor", previousInputs.sortingSensor, inputs.sortingSensor);
        logChange("LaneBSensor", previousInputs.laneBSensor, inputs.laneBSensor);
        // Scan the input image and calculate the output image.
        const auto previousState = controller.state();
        const auto outputs = controller.scan(inputs);
        if (previousState != controller.state()) {
            std::cout << "STATE " << toString(previousState) << " -> "
                      << toString(controller.state()) << '\n';
        }
        logChange("ConveyorMotor", previousOutputs.conveyorMotor, outputs.conveyorMotor);
        logChange("DiverterA", previousOutputs.diverterA, outputs.diverterA);
        logChange("DiverterB", previousOutputs.diverterB, outputs.diverterB);
        logChange("DiverterC", previousOutputs.diverterC, outputs.diverterC);

        // Apply outputs to the plant and advance physical time by one cycle.
        const bool previouslyInLaneB = boxInLaneB;
        if (outputs.conveyorMotor && !boxInLaneB) {
            // The transfer gate holds the box until B is selected exclusively.
            const bool routeB = outputs.diverterB && !outputs.diverterA && !outputs.diverterC;
            const double limit = routeB ? laneBPosition : std::max(boxPosition, sortingPosition);
            boxPosition = std::min(boxPosition + conveyorSpeed * dt, limit);
            if (routeB && boxPosition >= laneBPosition) boxInLaneB = true;
        }

        // Generate the next input image from the updated physical position.
        previousInputs = inputs;
        inputs.entrySensor = !boxInLaneB && boxPosition <= 0.10;
        inputs.sortingSensor = !boxInLaneB &&
            boxPosition >= sortingPosition - 0.05 && boxPosition <= sortingPosition + 0.05;
        inputs.laneBSensor = boxInLaneB && boxPosition >= laneBPosition;
        previousOutputs = outputs;
        if (!previouslyInLaneB && boxInLaneB) std::cout << "BOX-001 -> LANE_B\n";
        if (controller.state() == State::COMPLETE) return 0;
        if (controller.state() == State::FAULT) return 1;

        // Fixed simulated time step; wall-clock pacing is not a real-time guarantee.
        nextCycle += std::chrono::milliseconds(10);
        std::this_thread::sleep_until(nextCycle);
    }
    std::cerr << "Simulation timed out\n";
    return 1;
}
