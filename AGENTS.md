# FactoryFlow development

- Keep this milestone limited to the C++20 controller, plant simulator, console application, and tests.
- The controller reads Inputs and returns Outputs. It must not depend on or mutate the plant.
- Keep the core in src/main.cpp, src/Controller.hpp, src/Controller.cpp, and CMakeLists.txt.
- The inline plant simulation in main.cpp owns position, movement, routing, and sensors.
- Keep Inputs, Outputs, and State in Controller.hpp; favor a readable loop over abstractions.
- Safety inputs take priority on every scan. FAULT is latched and all outputs are off.
- Keep dependencies minimal and hardware I/O concepts explicit.
- Validate changes with `cmake -S . -B build`, `cmake --build build`,
  `ctest --test-dir build --output-on-failure`, and `./build/factoryflow`.
