cmake_minimum_required(VERSION 3.16)

project(drone_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PYBIND11_FINDPYTHON ON)

set(PROJECT_VENV_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python")
if(EXISTS "${PROJECT_VENV_PYTHON}")
    set(Python_EXECUTABLE "${PROJECT_VENV_PYTHON}" CACHE FILEPATH "Python executable" FORCE)
    execute_process(
        COMMAND "${PROJECT_VENV_PYTHON}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE PROJECT_PYBIND11_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(PROJECT_PYBIND11_DIR)
        set(pybind11_DIR "${PROJECT_PYBIND11_DIR}" CACHE PATH "pybind11 CMake package directory" FORCE)
    endif()
endif()

find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG QUIET)

file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS src/*.cpp)

set(PYTHON_BINDINGS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Python/PyClientImpl.cpp")
list(REMOVE_ITEM PROJECT_SOURCES "${PYTHON_BINDINGS_SOURCE}")

add_library(drone_system_core STATIC
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(drone_system_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(pybind11_FOUND)
    target_link_libraries(drone_system_core PUBLIC
        pybind11::headers
        Python::Python
    )
endif()

add_executable(app
    main.cpp
)

target_link_libraries(app PRIVATE
    drone_system_core
)

if(EXISTS "${PYTHON_BINDINGS_SOURCE}" AND pybind11_FOUND)
    pybind11_add_module(drone_system
        "${PYTHON_BINDINGS_SOURCE}"
    )

    target_link_libraries(drone_system PRIVATE
        drone_system_core
    )

    install(TARGETS drone_system
        LIBRARY DESTINATION .
    )
elseif(EXISTS "${PYTHON_BINDINGS_SOURCE}" AND NOT pybind11_FOUND)
    message(STATUS "pybind11 was not found, so the drone_system Python module will not be built.")
endif()
