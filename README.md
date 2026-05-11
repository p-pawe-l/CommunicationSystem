# Communication System

Communication System is a mixed C++ and Python project for routing messages between drone clients, controllers, and visualization tools. The C++ layer provides the core ring-buffer based communication system and Python bindings through `pybind11`; the Python layer defines client abstractions, Crazyflie drone integration, controller helpers, and a Pygame-based swarm map demo.

## Project Overview

The main runtime idea is simple:

1. Clients generate messages with `receivers`, `sender`, `type`, and `data` fields.
2. The shared `drone_system.System` routes messages between registered clients.
3. Python client classes provide generating and processing callbacks.
4. Drone-specific packages, such as `scripts/Crazyflie`, implement hardware behavior on top of the generic client layer.

## Repository Structure

```text
.
├── CMakeLists.txt                  # C++ build and pybind11 module setup
├── Makefile                        # Project make entry point
├── pyproject.toml                  # Python package/build metadata
├── drone_gui_demo.py               # Demo with simulated drones, controllers, and GUI
├── main.cpp                        # C++ executable entry point
├── include/
│   ├── Config.hpp                  # Shared C++ configuration
│   ├── RingBuffer.hpp              # Thread-safe message buffer
│   └── Python/
│       ├── PyClient.hpp            # Python-facing client worker wrapper
│       ├── PyPacket.hpp            # Python packet conversion/binding model
│       └── PySystem.hpp            # Python-facing message routing system
├── src/
│   ├── Config.cpp                  # Config implementation
│   └── Python/
│       └── PyClientImpl.cpp        # pybind11 module implementation
├── scripts/
│   ├── controller.py               # Base controller abstractions
│   ├── drone.py                    # Hardware-independent drone client base
│   ├── func_decorators.py          # Callback metadata decorators
│   ├── system_message.py           # System message data container
│   ├── Crazyflie/
│   │   ├── callback.py             # Crazyflie callback base classes
│   │   ├── crazyflie_drone.py      # Crazyflie drone client implementation
│   │   ├── engine_cb.py            # Engine telemetry callback
│   │   ├── logconf.py              # Crazyflie logging configuration wrapper
│   │   ├── move_dispatch.py        # Movement command dispatch
│   │   ├── pos_cb.py               # Position telemetry callback
│   │   └── vel_cb.py               # Velocity telemetry callback
│   └── GUI/
│       ├── drawing.py              # GUI drawing helpers
│       ├── map_client.py           # Drone map visualization client
│       ├── models.py               # GUI state models
│       └── stats_window.py         # Statistics window
└── .github/workflows/
    └── ci.yml                      # CI workflow skeleton
```

## Key Modules

- `include/Python/PySystem.hpp` routes messages between registered clients.
- `include/Python/PyClient.hpp` runs Python generating and processing callbacks in worker threads.
- `scripts/func_decorators.py` marks Python callbacks as data generators or processors.
- `scripts/drone.py` defines the generic `DroneClient` base class.
- `scripts/Crazyflie/crazyflie_drone.py` connects the generic drone client to Crazyflie hardware through `cflib`.
- `scripts/GUI/map_client.py` visualizes drone telemetry and routed messages.

## Setup

The project targets Python 3.9+ and C++17. Python package metadata is defined in `pyproject.toml`.

```bash
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

For Crazyflie hardware support, install the Bitcraze Crazyflie Python library as well:

```bash
pip install cflib
```

## Build

The C++ core and Python extension are configured with CMake and `scikit-build-core`.

```bash
cmake -S . -B build
cmake --build build
```

The build can produce:

- `app`: the C++ executable from `main.cpp`
- `drone_system`: the Python extension module when `pybind11` is available

## Demo

After building the Python extension, run the GUI demo from the repository root:

```bash
python drone_gui_demo.py
```

Optional arguments:

```bash
python drone_gui_demo.py --duration 30
python drone_gui_demo.py --line-mode all
```

The demo creates simulated drones and controllers, starts the routing system, and displays live telemetry in the Pygame GUI.

## Development Notes

- Generated files such as `__pycache__`, build outputs, logs, and local virtual environments are ignored through `.gitignore`.
- Python callbacks passed into `drone_system.Client` should be decorated with `@generating_func` or `@processing_func`.
- Hardware-specific drone implementations should live outside `scripts/drone.py` so the base client remains independent of device libraries.

## Roadmap

- Add YAML configuration support for declaring system workflows, clients, receivers, update rates, and startup behavior.
- Add a `SystemRunner` layer that can create the system, attach clients, start workers, and shut everything down cleanly from one entry point.
- Add a client registry so YAML files can instantiate clients by type, such as `crazyflie`, `simulated_drone`, `controller`, or `gui`.
- Add a simulated drone backend for testing controllers, routing, and GUI behavior without real Crazyflie hardware.
- Add message schema validation for required fields such as `sender`, `receivers`, `type`, and `data`, plus command and telemetry payload checks.
- Add record and replay support for routed messages so demos and bugs can be reproduced from saved message logs.
- Add a CLI for common workflows, such as running a YAML config, validating a config, replaying logs, and launching the GUI demo.
- Add focused tests for routing behavior, callback decorators, movement dispatch, message serialization, and future YAML parsing.
