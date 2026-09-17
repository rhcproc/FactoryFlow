#include "Controller.hpp"
#include "TcpServer.hpp"
#include <csignal>
#include <memory>
#include <sstream>
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
std::string statusJson(State state, const Inputs& inputs, const Outputs& outputs) {
    std::ostringstream json;
    json << std::boolalpha
         << "{\"state\":\"" << toString(state) << "\",\"inputs\":{"
         << "\"entrySensor\":" << inputs.entrySensor
         << ",\"sortingSensor\":" << inputs.sortingSensor
         << ",\"laneBSensor\":" << inputs.laneBSensor
         << ",\"emergencyStop\":" << inputs.emergencyStop
         << ",\"motorFault\":" << inputs.motorFault
         << "},\"outputs\":{\"conveyorMotor\":" << outputs.conveyorMotor
         << ",\"diverterA\":" << outputs.diverterA
         << ",\"diverterB\":" << outputs.diverterB
         << ",\"diverterC\":" << outputs.diverterC << "}}";
    return json.str();
}
}

int main(int argc, char** argv) try {
    const bool serve = argc == 2 && std::string_view(argv[1]) == "--serve";
    if (argc != 1 && !serve) {
        std::cerr << "Usage: factoryflow [--serve]\n";
        return 1;
    }
    // A disconnected TCP client must not terminate the control process.
    std::signal(SIGPIPE, SIG_IGN);
    std::unique_ptr<TcpServer> server;
    if (serve) server = std::make_unique<TcpServer>(9000);
    bool running = !serve;

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

    if (serve) std::cout << "TCP READY 127.0.0.1:9000 (waiting for start)\n";
    for (int cycle = 0; serve || cycle < 1000; cycle = serve ? 0 : cycle + 1) {
        const auto command = server ? server->poll() : std::nullopt;
        std::string error;
        if (command) {
            // Protocol is a JSON string token; allow surrounding JSON whitespace.
            const auto first = command->find_first_not_of(" \t\r");
            const auto last = command->find_last_not_of(" \t\r");
            const auto token = first == std::string::npos ? "" : command->substr(first, last - first + 1);
            if (token == "\"start\"") {
                if (controller.state() == State::FAULT || controller.state() == State::COMPLETE) {
                    error = "reset_required";
                } else {
                    running = true;
                }
            } else if (token == "\"stop\"") {
                // Use the existing controller safety path, not a second state machine.
                inputs.emergencyStop = true;
                running = false;
            } else if (token == "\"reset\"") {
                // Reset the virtual experiment, equivalent to restarting the demo.
                controller = Controller{};
                boxPosition = 0.0;
                boxInLaneB = false;
                inputs = Inputs{};
                inputs.entrySensor = true;
                running = false;
            } else if (token != "\"status\"") {
                error = "invalid_request";
            }
        }
        logChange("EntrySensor", previousInputs.entrySensor, inputs.entrySensor);
        logChange("SortingSensor", previousInputs.sortingSensor, inputs.sortingSensor);
        logChange("LaneBSensor", previousInputs.laneBSensor, inputs.laneBSensor);
        // Scan the input image and calculate the output image.
        const auto previousState = controller.state();
        // While awaiting start, leave the fresh controller idle. Safety still scans.
        const auto outputs = (running || inputs.emergencyStop || inputs.motorFault)
            ? controller.scan(inputs) : Outputs{};
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
        if (command) {
            server->reply(error.empty() ? statusJson(controller.state(), inputs, outputs)
                                       : "{\"error\":\"" + error + "\"}");
        }
        if (!serve && controller.state() == State::COMPLETE) return 0;
        if (!serve && controller.state() == State::FAULT) return 1;

        // Fixed simulated time step; wall-clock pacing is not a real-time guarantee.
        nextCycle += std::chrono::milliseconds(10);
        std::this_thread::sleep_until(nextCycle);
    }
    std::cerr << "Simulation timed out\n";
    return 1;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
