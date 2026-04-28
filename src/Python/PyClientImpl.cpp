#include "Python/PyClient.hpp"
#include "Python/PyPacket.hpp"
#include "Python/PySystem.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {
    using PyPacket = droning::python::PyPacket;
    using PyDroneSystem = droning::python::PySystem<PyPacket>;
    using PyDroneClient = droning::python::PyClient<PyPacket>;
}

PYBIND11_MODULE(drone_system, m) {
    m.doc() = "C++ routing system for drone clients";

    py::class_<PyDroneSystem>(m, "System")
        .def(
            py::init<>(),
            "Creates a new communication system"
        )
        .def(
            "start",
            &PyDroneSystem::start,
            py::call_guard<py::gil_scoped_release>(),
            "Starts the communication system"
        )
        .def(
            "stop",
            &PyDroneSystem::stop,
            py::call_guard<py::gil_scoped_release>(),
            "Stops the communication system"
        )
        .def(
            "attach_client",
            [](PyDroneSystem& system, const std::string& client_id) {
                system.attachClient(client_id);
            },
            py::arg("client_id"),
            "Registers a client inbox in the system"
        )
        .def(
            "attach_client",
            [](PyDroneSystem& system, PyDroneClient& client) {
                system.attachClient(
                    client.getClientId(),
                    client.getInboxBuffer(),
                    client.getOutboxBuffer()
                );
            },
            py::arg("client"),
            py::keep_alive<1, 2>(),
            "Registers a client in the system router"
        )
        .def(
            "detach_client",
            &PyDroneSystem::detachClient,
            py::arg("client_id"),
            "Removes client from the system"
        )
        .def(
            "send",
            [](PyDroneSystem& system, const py::object& message) {
                system.send(droning::python::pyPacketFromObject(message));
            },
            py::arg("message"),
            "Sends message to receivers selected by message['receivers']"
        )
        .def(
            "receive",
            [](PyDroneSystem& system, const std::string& client_id) -> py::object {
                auto message = system.receive(client_id);
                if (!message.has_value()) return py::none();
                return droning::python::pyPacketToDict(message.value());
            },
            py::arg("client_id"),
            "Returns message for client or None when no message is available"
        )
        .def(
            "routed_messages",
            &PyDroneSystem::routedMessages,
            "Returns number of messages routed by the system"
        );

    py::class_<PyDroneClient>(m, "Client")
        .def(
            py::init<std::string, PyDroneSystem*, bool>(),
            py::arg("client_id"),
            py::arg("system"),
            py::arg("auto_start") = false,
            py::keep_alive<1, 3>(),
            "Creates a client without assigned worker functions"
        )
        .def(
            py::init<std::string, PyDroneSystem*, py::object, py::object, bool>(),
            py::arg("client_id"),
            py::arg("system"),
            py::arg("gen_func"),
            py::arg("proc_func"),
            py::arg("auto_start") = false,
            py::keep_alive<1, 3>(),
            "Creates a client with assigned worker functions"
        )
        .def(
            "start",
            &PyDroneClient::start,
            py::call_guard<py::gil_scoped_release>(),
            "Starts client worker threads"
        )
        .def(
            "stop",
            &PyDroneClient::stop,
            py::call_guard<py::gil_scoped_release>(),
            "Stops client worker threads"
        )
        .def(
            "assign_generating_func",
            &PyDroneClient::assignGeneratingFunc,
            py::arg("func"),
            "Assigns callable marked with @generate_func"
        )
        .def(
            "assign_processing_func",
            &PyDroneClient::assignProcessingFunc,
            py::arg("func"),
            "Assigns callable marked with @process_func"
        )
        .def(
            "is_running",
            &PyDroneClient::isRunnning,
            "Returns whether client workers are running"
        )
        .def(
            "push_inbox",
            [](PyDroneClient& client, const py::object& message) {
                client.pushInbox(droning::python::pyPacketFromObject(message));
            },
            py::arg("message"),
            "Pushes message into client inbox"
        )
        .def(
            "pop_outbox",
            [](PyDroneClient& client) -> py::object {
                PyPacket message;
                const auto res = client.getOutboxBuffer().safeRead(&message);
                if (res != 0x01) return py::none();
                return droning::python::pyPacketToDict(message);
            },
            "Pops generated message from client outbox or returns None"
        );
}
