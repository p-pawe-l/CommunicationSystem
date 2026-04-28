#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace droning::python {

    namespace py = pybind11;

    struct PyPacket {
        std::string receiver_;
        std::string sender_;
        std::string type_;
        std::unordered_map<std::string, double> numbers_;
        std::unordered_map<std::string, std::string> strings_;
        std::unordered_map<std::string, bool> bools_;
    };

    inline auto pyPacketFromObject(const py::object& raw_message) -> PyPacket {
        py::dict message = py::cast<py::dict>(raw_message);
        if (!message.contains("receiver")) {
            throw std::runtime_error("Message must contain a 'receiver' field");
        }

        PyPacket packet;
        packet.receiver_ = py::cast<std::string>(message["receiver"]);

        if (message.contains("sender") && !message["sender"].is_none()) {
            packet.sender_ = py::cast<std::string>(message["sender"]);
        }

        if (message.contains("type") && !message["type"].is_none()) {
            packet.type_ = py::cast<std::string>(message["type"]);
        }

        if (!message.contains("data") || message["data"].is_none()) {
            return packet;
        }

        py::dict data = py::cast<py::dict>(message["data"]);
        const auto data_size = static_cast<std::size_t>(py::len(data));
        packet.numbers_.reserve(data_size);
        packet.strings_.reserve(data_size);
        packet.bools_.reserve(data_size);

        for (auto item : data) {
            const auto key = py::cast<std::string>(item.first);
            py::handle value = item.second;

            if (py::isinstance<py::bool_>(value)) {
                packet.bools_[key] = py::cast<bool>(value);
            } else if (py::isinstance<py::int_>(value) || py::isinstance<py::float_>(value)) {
                packet.numbers_[key] = py::cast<double>(value);
            } else if (py::isinstance<py::str>(value)) {
                packet.strings_[key] = py::cast<std::string>(value);
            }
        }

        return packet;
    }

    inline auto pyPacketToDict(const PyPacket& packet) -> py::dict {
        py::dict message;
        message["receiver"] = packet.receiver_;
        message["sender"] = packet.sender_;
        message["type"] = packet.type_;

        py::dict data;
        for (const auto& [key, value] : packet.numbers_) data[py::str(key)] = value;
        for (const auto& [key, value] : packet.strings_) data[py::str(key)] = value;
        for (const auto& [key, value] : packet.bools_) data[py::str(key)] = value;
        message["data"] = std::move(data);

        return message;
    }

}
