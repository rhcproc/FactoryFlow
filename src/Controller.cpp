#include "Controller.hpp"

namespace factoryflow {
Outputs Controller::scan(const Inputs& inputs) {
    // Safety has priority over every sequence state, including COMPLETE.
    // FAULT is latched: clearing an input must not restart machinery.
    if (inputs.emergencyStop || inputs.motorFault) {
        state_ = State::FAULT;
    }

    switch (state_) {
    case State::IDLE:
        if (inputs.entrySensor) state_ = State::TRANSPORTING;
        break;
    case State::TRANSPORTING:
        if (inputs.sortingSensor) state_ = State::SORTING;
        break;
    case State::SORTING:
        if (inputs.laneBSensor) state_ = State::COMPLETE;
        break;
    case State::COMPLETE:
    case State::FAULT:
        break;
    }

    // Rebuild the output process image each scan; inactive outputs stay safe.
    Outputs outputs;
    outputs.conveyorMotor = state_ == State::TRANSPORTING || state_ == State::SORTING;
    outputs.diverterB = state_ == State::SORTING;
    return outputs;
}
}
