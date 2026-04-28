#pragma once

#include "Config.hpp"
#include "RingBuffer.hpp"

#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <memory>
#include <mutex>
#include <optional>
#include <atomic>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>


namespace droning::python {

    namespace py = pybind11;
    inline constexpr std::size_t PY_SYSTEM_ROUTE_BATCH_LIMIT = 256;

    template <typename PacketType>
    class PySystem {
    private:
        struct __py_client_channel {
            SafeRingBuffer<PacketType>* inbox_buf_;
            SafeRingBuffer<PacketType>* outbox_buf_;
        };

        std::unordered_map<std::string, __py_client_channel> clients_;
        std::unordered_map<std::string, std::shared_ptr<SafeRingBuffer<PacketType>>> owned_inboxes_;
        mutable std::mutex runtime_mutex_;
        std::atomic<bool> is_running_;
        std::atomic<std::size_t> routed_messages_;
        std::thread routing_worker_;

        auto findClient(const std::string& client_id) -> __py_client_channel {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            const auto iter = clients_.find(client_id);
            if (iter == clients_.end()) {
                throw std::runtime_error("Client not found: " + client_id);
            }
            return iter->second;
        }

        auto getClientsSnapshot() -> std::unordered_map<std::string, __py_client_channel> {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            return clients_;
        }

        auto routeToReceiver(
            const std::unordered_map<std::string, __py_client_channel>& clients_snapshot,
            PacketType message,
            const std::string& receiver_id
        ) -> bool {
            const auto receiver = clients_snapshot.find(receiver_id);
            if (receiver == clients_snapshot.end() || receiver->second.inbox_buf_ == nullptr) {
                return false;
            }

            receiver->second.inbox_buf_->safeWrite(std::move(message));
            ++routed_messages_;
            return true;
        }

        auto routePacket(
            const std::unordered_map<std::string, __py_client_channel>& clients_snapshot,
            const PacketType& message
        ) -> bool {
            bool routed_any = false;

            for (const auto& receiver_id : message.receivers_) {
                routed_any = routeToReceiver(clients_snapshot, message, receiver_id) || routed_any;
            }

            return routed_any;
        }

        auto routeMessages() -> void {
            while (is_running_) {
                bool routed_any = false;
                auto clients_snapshot = getClientsSnapshot();

                for (auto& [_, channel] : clients_snapshot) {
                    if (channel.outbox_buf_ == nullptr) continue;

                    for (std::size_t routed_from_client = 0;
                         is_running_ && routed_from_client < PY_SYSTEM_ROUTE_BATCH_LIMIT;
                         ++routed_from_client) {
                        PacketType message;
                        auto res = channel.outbox_buf_->safeRead(&message);
                        if (res != 0x01) break;

                        routed_any = routePacket(clients_snapshot, message) || routed_any;
                    }
                }

                if (!routed_any) {
                    std::this_thread::yield();
                }
            }
        }

    public:
        PySystem(): is_running_(false), routed_messages_(0) {}
        ~PySystem() { stop(); }

        auto start() -> void {
            if (!is_running_) {
                is_running_ = true;
                routing_worker_ = std::thread(&PySystem::routeMessages, this);
            }
        }

        auto stop() -> void {
            if (is_running_) {
                is_running_ = false;
                if (routing_worker_.joinable()) routing_worker_.join();
            }
        }

        auto attachClient(const std::string& client_id) -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            auto inbox = std::make_shared<SafeRingBuffer<PacketType>>(Config::getInstance()->getRingBufSize());
            const auto [_, inserted] = clients_.try_emplace(
                client_id,
                __py_client_channel{
                    .inbox_buf_ = inbox.get(),
                    .outbox_buf_ = nullptr,
                }
            );

            if (!inserted) {
                throw std::runtime_error("Client already exists: " + client_id);
            }
            owned_inboxes_.insert({client_id, std::move(inbox)});
        }

        auto attachClient(
            const std::string& client_id,
            SafeRingBuffer<PacketType>& inbox_buf,
            SafeRingBuffer<PacketType>& outbox_buf
        ) -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            const auto [_, inserted] = clients_.try_emplace(
                client_id,
                __py_client_channel{
                    .inbox_buf_ = &inbox_buf,
                    .outbox_buf_ = &outbox_buf,
                }
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
            owned_inboxes_.erase(client_id);
        }

        auto send(PacketType message) -> void {
            if (message.receivers_.empty()) {
                throw std::runtime_error("Message must contain a non-empty 'receivers' field");
            }

            auto clients_snapshot = getClientsSnapshot();
            if (!routePacket(clients_snapshot, message)) {
                throw std::runtime_error("No receivers found for message");
            }
        }

        auto receive(const std::string& client_id) -> std::optional<PacketType> {
            PacketType data;
            auto res = findClient(client_id).inbox_buf_->safeRead(&data);
            if (res != 0x01) {
                return std::nullopt;
            }

            return std::move(data);
        }

        [[nodiscard]] auto routedMessages() const noexcept -> std::size_t {
            return routed_messages_;
        }
    };

}
