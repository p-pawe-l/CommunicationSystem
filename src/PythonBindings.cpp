#include "PySystem.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(drone_system, m) {
    m.doc() = "C++ routing system for drone clients";

    py::class_<droning::PySystem<py::dict>>(m, "System")
        .def(
            py::init<>(), 
            "Creates a new communication system"
        )
        .def(
            "start",
            &droning::PySystem<py::dict>::start,
            "Starts the communication system"
        )
        .def(
            "stop",
            &droning::PySystem<py::dict>::stop,
            "Stops the communication system"
        )
        .def(
            "attachClient", 
            &droning::PySystem<py::dict>::attachClient,
            py::arg("client_id"), 
            "Register a client inbox in the system"
        )
        .def(
            "detachClient", 
            &droning::PySystem<py::dict>::detachClient,
            py::arg("client_id"), 
            "Removes client from the inbox in the system"
        )
        .def(
            "send", 
            &droning::PySystem<py::dict>::send,
            py::arg("message"),
            "Sends message to the system")
        .def(
            "receive", 
            &droning::PySystem<py::dict>::receive,
            py::arg("client_id"),
            "Returns message from client with provided Id. If no message is available returns None"
        );
}
