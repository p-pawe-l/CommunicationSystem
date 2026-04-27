#pragma once

#include "Config.hpp"
#include "RingBuffer.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>


namespace droning::python {

    namespace py = pybind11;

    template <typename PacketType>
    class PySystem {
    private:
        std::unordered_map<std::string, std::shared_ptr<SafeRingBuffer<PacketType>>> clients_;
        mutable std::mutex runtime_mutex_;

        auto findClient(const std::string& client_id) -> std::shared_ptr<SafeRingBuffer<PacketType>> {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            const auto iter = clients_.find(client_id);
            if (iter == clients_.end()) {
                throw std::runtime_error("Client not found: " + client_id);
            }
            return iter->second;
        }

    public:
        PySystem() = default;
        ~PySystem() = default;

        auto start() -> void {}
        auto stop() -> void {}

        auto attachClient(const std::string& client_id) -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            const auto [_, inserted] = clients_.try_emplace(
                client_id,
                std::make_shared<SafeRingBuffer<PacketType>>(Config::getInstance()->getRingBufSize())
            );

            if (!inserted) {
                throw std::runtime_error("Client already exists: " + client_id);
            }
        }

        auto detachClient(const std::string& client_id) -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            if (clients_.erase(client_id) == 0) {
                throw std::runtime_error("Client not found: " + client_id);
            }
        }

        auto send(PacketType message) -> void {
            if (!message.contains("receiver")) {
                throw std::runtime_error("Message must contain a 'receiver' field");
            }

            const auto receiver = py::cast<std::string>(message["receiver"]);
            findClient(receiver)->safeWrite(std::move(message));
        }

        auto receive(const std::string& client_id) -> py::typing::Optional<PacketType> {
            auto data = findClient(client_id)->safeRead();
            if (!data.has_value()) {
                return py::none();
            }

            return std::move(data.value());
        }
    };

}
