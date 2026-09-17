cmake_minimum_required(VERSION 3.20)
project(FactoryFlow VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
add_executable(factoryflow src/main.cpp src/Controller.cpp)
target_link_libraries(factoryflow PRIVATE Threads::Threads)

include(CTest)
if(BUILD_TESTING)
    add_executable(factoryflow_tests tests/tests.cpp src/Controller.cpp)
    target_include_directories(factoryflow_tests PRIVATE src)
    foreach(scenario normal emergency_stop motor_fault separation)
        add_test(NAME ${scenario} COMMAND factoryflow_tests ${scenario})
    endforeach()
    add_test(NAME simulator_flow COMMAND factoryflow)
    set_tests_properties(simulator_flow PROPERTIES
        PASS_REGULAR_EXPRESSION "BOX-001 -> LANE_B[\r\n]+LaneBSensor = ON[\r\n]+STATE SORTING -> COMPLETE[\r\n]+ConveyorMotor = OFF[\r\n]+DiverterB = OFF"
        TIMEOUT 15)
endif()
