# Communication System

Communication System is a packet-based drone communication project with a C++ backend and a Python API layer. It is mainly being developed for controlling and operating real drones, especially Bitcraze Crazyflie devices.

The C++ layer provides the core ring-buffer based communication system and Python bindings through `pybind11`. The Python layer defines message helpers, client abstractions, controller abstractions, Crazyflie integration, and early GUI-related modules.

## Project Overview

The main runtime idea is simple:

1. Clients generate messages with `receivers`, `sender`, `type`, and `data` fields.
2. The shared `drone_system.System` routes messages between registered clients.
3. Python client classes provide generating and processing callbacks.
4. Drone-specific packages, such as `scripts/Crazyflie`, implement hardware behavior on top of the generic client layer.

## Current State

This project is under active development. The core C++/Python communication layer, Python client abstractions, and Crazyflie support are the main focus right now.

Current notes:

- `uv.lock` is committed so contributors can reproduce the Python environment with `uv sync`.
- Crazyflie telemetry callbacks and movement dispatch exist, but hardware workflows still need real-device validation.
- GUI modules are present in `scripts/GUI`, but there is no current runnable GUI demo entry point.
- YAML-based workflow declarations, a runner layer, simulated drones, and a CLI are planned but not implemented yet.

## Repository Structure

```text
.
├── .clang-format                  # C++ formatting rules
├── .clang-tidy                    # C++ linting rules
├── CMakeLists.txt                  # C++ build and pybind11 module setup
├── docker-compose.yml              # Development container composition
├── Makefile                        # Project make entry point
├── pyproject.toml                  # Python package/build metadata
├── uv.lock                         # Locked Python dependency resolution
├── main.cpp                        # C++ executable entry point
├── tests/
│   ├── tests_cpp/                  # C++ tests
│   └── tests_py/                   # Python tests
├── include/
│   ├── Config.hpp                  # Shared C++ configuration
│   ├── RingBuffer.hpp              # Thread-safe message buffer
│   └── Python/
│       ├── PyClient.hpp            # Python-facing client worker wrapper
│       ├── PyPacket.hpp            # Python packet conversion/binding model
│       └── PySystem.hpp            # Python-facing message routing system
├── src/
│   ├── Dockerfile.cpp              # C++ development container
│   ├── Config.cpp                  # Config implementation
│   └── Python/
│       └── PyClientImpl.cpp        # pybind11 module implementation
├── scripts/
│   ├── Dockerfile.python           # Python development container
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
- `scripts/system_message.py` defines the message container used by Python client code.
- `scripts/drone.py` defines the generic `DroneClient` base class.
- `scripts/Crazyflie/crazyflie_drone.py` connects the generic drone client to Crazyflie hardware through `cflib`.
- `scripts/Crazyflie/callback.py` defines shared cflib callback registration behavior.
- `scripts/GUI/map_client.py` contains GUI map support, but there is no active demo entry point at the moment.

## Setup With uv

The project targets Python 3.9+ and C++17. Python package metadata is defined in `pyproject.toml`, and dependency resolution is locked in `uv.lock`.

Install `uv` first if it is not already available on your machine, then sync from the lockfile:

```bash
uv sync
```

After changing dependencies in `pyproject.toml`, update the lockfile:

```bash
make lock
```

Commit `uv.lock` after dependency changes.

For Crazyflie hardware support, install the Bitcraze Crazyflie Python library as well:

```bash
uv sync --extra crazyflie
```

## Build

The C++ core and Python extension are configured with CMake and `scikit-build-core`. Build the Python package with:

```bash
uv build
```

For direct CMake builds, run CMake inside the uv-managed environment:

```bash
uv run cmake -S . -B build
uv run cmake --build build
```

The build can produce:

- `app`: the C++ executable from `main.cpp`
- `drone_system`: the Python extension module when `pybind11` is available

## Development Notes

- Generated files such as `__pycache__`, build outputs, logs, and local virtual environments are ignored through `.gitignore`.
- Python environments and dependency resolution are managed with `uv`; run `uv sync` after changing `pyproject.toml`.
- Python callbacks passed into `drone_system.Client` should be decorated with `@generating_func` or `@processing_func`.
- Hardware-specific drone implementations should live outside `scripts/drone.py` so the base client remains independent of device libraries.

## Formatting And Linting

Install development tools with:

```bash
make sync-dev
```

Format Python and C++ sources:

```bash
make format
```

Run Python and C++ lint checks:

```bash
make lint
```

Python formatting and linting use Ruff. C++ formatting uses `clang-format`, and C++ linting uses `clang-tidy` with the build directory as its compilation database source.

## Testing

Tests are split by language:

- C++ tests live in `tests/tests_cpp`.
- Python tests live in `tests/tests_py`.

Run only C++ tests:

```bash
make cpp-test
```

Run only Python tests:

```bash
make python-test
```

Run both test suites:

```bash
make test
```

The shared CI pipeline runs these same Makefile targets after the independent C++ and Python pipelines are green.

## Docker Development

The repository provides separate development containers for the C++ and Python toolchains. Their Dockerfiles live near the parts of the project they support:

- `src/Dockerfile.cpp` for the C++ backend toolchain.
- `scripts/Dockerfile.python` for the Python API/tooling environment.

Build both images:

```bash
docker compose build
```

Open a shell in the C++ environment:

```bash
docker compose run --rm cpp
```

Open a shell in the Python/uv environment:

```bash
docker compose run --rm python
```

Useful examples:

```bash
docker compose run --rm cpp make build
docker compose run --rm cpp make lint-cpp
docker compose run --rm python make sync-dev
docker compose run --rm python make lint-python
docker compose run --rm python make python-test
```

The Docker setup is intended for reproducible development and CI-like checks. Real Crazyflie hardware access may need additional USB/radio device configuration on the host.

## Roadmap
- Add YAML configuration support for declaring system workflows, clients, receivers, update rates, and startup behavior.
- Add a `SystemRunner` layer that can create the system, attach clients, start workers, and shut everything down cleanly from one entry point.
- Add a client registry so YAML files can instantiate clients by type, such as `crazyflie`, `simulated_drone`, `controller`, or `gui`.
- Add a simulated drone backend for testing controllers, routing, and GUI behavior without real Crazyflie hardware.
- Add message schema validation for required fields such as `sender`, `receivers`, `type`, and `data`, plus command and telemetry payload checks.
- Add record and replay support for routed messages so demos and bugs can be reproduced from saved message logs.
- Add a CLI for common workflows, such as running a YAML config, validating a config, replaying logs, and launching GUI workflows.
- Add focused tests for routing behavior, callback decorators, movement dispatch, message serialization, and future YAML parsing.
