#pragma once
#include <string_view>

namespace factoryflow {

// Input process image: sampled once at the beginning of each PLC scan.
struct Inputs {
    bool entrySensor = false;
    bool sortingSensor = false;
    bool emergencyStop = false;
    bool motorFault = false;
    bool laneBSensor = false;
};

struct Outputs {
    bool conveyorMotor = false;
    bool diverterA = false;
    bool diverterB = false;
    bool diverterC = false;
};

enum class State { IDLE, TRANSPORTING, SORTING, COMPLETE, FAULT };

constexpr std::string_view toString(State state) {
    switch (state) {
    case State::IDLE: return "IDLE";
    case State::TRANSPORTING: return "TRANSPORTING";
    case State::SORTING: return "SORTING";
    case State::COMPLETE: return "COMPLETE";
    case State::FAULT: return "FAULT";
    }
    return "UNKNOWN";
}

class Controller {
public:
    [[nodiscard]] Outputs scan(const Inputs& inputs);
    [[nodiscard]] State state() const { return state_; }

private:
    State state_ = State::IDLE;
};

}
