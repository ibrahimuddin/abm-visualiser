# ABM Visualiser: Cancer Cell Simulation via WebGPU

A high-performance Agent-Based Model (ABM) designed to simulate and visualise cancer cells and "greed" dynamics. This project leverages **WebGPU** for modern, cross-platform graphics and **Dear ImGui** for real-time parameter tuning.

## Features
* **Scalable Simulation:** Supports up to 100,000 agents using a batch-rendering strategy.
* **Real-time UI:** Adjust zoom, rotation, speed, and biological parameters (Protein synthesis) live.
* **Heat Map Mode:** Toggleable visual modes to identify high-velocity cell clusters.
* **Cross-Platform:** Built using C++17 and CMake, compatible with Windows, macOS, and Linux.

## Prerequisites
Before building, ensure you have the following installed:
* **CMake** (v3.15 or higher)
* **C++17 Compiler** (GCC, Clang, or MSVC)
* **Graphics Drivers:** Latest drivers with support for Vulkan, Metal, or DX12.

## Build Instructions

Follow these steps to clone and build the project from source:

```bash
# 1. Clone the repository
git clone [https://github.com/ibrahimuddin/abm-visualiser](https://github.com/ibrahimuddin/abm-visualiser)
cd abm-visualiser

# 2. Create and enter the build directory
mkdir build
cd build

# 3. Configure the project
cmake ..

# 4. Build the executable
cmake --build .

# 5. Compile (Linux/macOS)
make

# 6. Run the simulation
./ABM_Visualizer