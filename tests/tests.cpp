#include "Controller.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace factoryflow;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool allOff(const Outputs& outputs) {
    return !outputs.conveyorMotor && !outputs.diverterA &&
           !outputs.diverterB && !outputs.diverterC;
}

void normal() {
    Controller controller;
    Inputs inputs;
    require(allOff(controller.scan(inputs)), "idle outputs must be off");
    require(controller.state() == State::IDLE, "must wait for entry");
    inputs.entrySensor = true;
    auto outputs = controller.scan(inputs);
    require(controller.state() == State::TRANSPORTING && outputs.conveyorMotor,
            "entry must start transport");
    require(!outputs.diverterA && !outputs.diverterB && !outputs.diverterC,
            "transport must not activate diverters");
    inputs.entrySensor = false;
    (void)controller.scan(inputs);
    require(controller.state() == State::TRANSPORTING, "must wait for sorting sensor");
    inputs.sortingSensor = true;
    outputs = controller.scan(inputs);
    require(controller.state() == State::SORTING && outputs.conveyorMotor &&
            outputs.diverterB && !outputs.diverterA && !outputs.diverterC,
            "sorting must enable motor and only B");
    inputs.sortingSensor = false;
    (void)controller.scan(inputs);
    require(controller.state() == State::SORTING, "must wait for lane arrival");
    inputs.laneBSensor = true;
    require(allOff(controller.scan(inputs)), "completion must disable outputs");
    require(controller.state() == State::COMPLETE, "arrival must complete sequence");
    require(allOff(controller.scan({})) && controller.state() == State::COMPLETE,
            "completion must persist");
}

void safety(bool emergencyStop) {
    // Exercise safety priority in IDLE, TRANSPORTING, SORTING, and COMPLETE.
    for (int stage = 0; stage < 4; ++stage) {
        Controller controller;
        Inputs inputs;
        if (stage >= 1) { inputs.entrySensor = true; (void)controller.scan(inputs); }
        if (stage >= 2) { inputs.sortingSensor = true; (void)controller.scan(inputs); }
        if (stage >= 3) { inputs.laneBSensor = true; (void)controller.scan(inputs); }
        inputs.emergencyStop = emergencyStop;
        inputs.motorFault = !emergencyStop;
        require(allOff(controller.scan(inputs)), "fault outputs must turn off in same scan");
        require(controller.state() == State::FAULT, "safety must enter FAULT");
        require(allOff(controller.scan({})), "clearing safety input must not restart");
        require(controller.state() == State::FAULT, "FAULT must latch");
    }

}

void separation() {
    Controller controller;
    const Inputs snapshot{.entrySensor = true};
    Outputs outputs;
    for (int cycle = 0; cycle < 100; ++cycle) outputs = controller.scan(snapshot);
    require(outputs.conveyorMotor, "controller must request motor");
    require(controller.state() == State::TRANSPORTING,
            "scans alone must not advance physical sensors or finish transport");
    require(snapshot.entrySensor && !snapshot.sortingSensor && !snapshot.laneBSensor,
            "controller must not mutate input snapshot");
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected test scenario");
        const std::string_view scenario = argv[1];
        if (scenario == "normal") normal();
        else if (scenario == "emergency_stop") safety(true);
        else if (scenario == "motor_fault") safety(false);
        else if (scenario == "separation") separation();
        else throw std::runtime_error("unknown scenario");
        std::cout << scenario << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
