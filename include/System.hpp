#pragma once

#include "SystemClient.hpp"
#include "Utils.hpp"
#include "RingBuffer.hpp"
#include "Message.hpp"
#include <exception>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <string>
#include <iostream>
#include <atomic>
#include <algorithm>
#include <memory>

#include "Config.hpp"

namespace droning {

    template <typename PacketType>
    class System  {
    private:
        /**
        * @brief Client descriptor in the system
        */
        struct __sys_client_channel {
            std::shared_ptr<SystemClient<system_message<PacketType>>> client_;
            std::shared_ptr<SafeRingBuffer<system_message<PacketType>>> read_buf_;   /**< Buffer to read data from client   */
            std::shared_ptr<SafeRingBuffer<system_message<PacketType>>> write_buf_;  /**< Buffer to send data to client     */
        };

    private:
        std::unordered_map<std::string, struct __sys_client_channel> clients_;
        std::unique_ptr<SafeRingBuffer<system_message<PacketType>>> routing_buf_;

        std::atomic<bool> is_running_;
        std::thread system_worker_;
        std::thread routing_worker_;

        std::mutex runtime_mutex_;
        std::mutex routing_mutex_;

        std::size_t num_clients_;

        /**
         * @brief Returns client`s communication channel based on client_id
         */
        inline auto getChannel(const std::string& client_id) -> __sys_client_channel& {
            return clients_.at(client_id);
        }

        /**
        * @brief Takes snapshot of collection of all clients in the system
        */
        inline auto getSnapClients() -> std::unordered_map<std::string, struct __sys_client_channel> {
            std::lock_guard<std::mutex> client_collection_guard(runtime_mutex_);
            return clients_;
        }

        /**
        * @brief Routes packages from clients->system buffer to shared system buffer
        */
        auto routeToSharedBuf() -> void {
            while (is_running_) {
                for (auto& [_, channel] : getSnapClients()) {
                    std::optional<system_message<PacketType>> msg = channel.read_buf_->safeRead();

                    if (msg == std::nullopt) continue;
                    switch (msg->action) {
                        case system_message_action::DETACH:
                            try {
                                detach_client(msg->sender);
                                std::cout << "Client: " << msg->sender << "detached.\n";
                            }
                            catch (std::out_of_range& _) {
                                std::cout << "Client: " << msg->sender << " detachment failed!\n";
                            }
                            break;
                        case system_message_action::DATA_ACTION:
                            routing_buf_->safeWrite(std::move(msg.value()));
                            break;
                    }
                }
            }
        }

        auto routeToReceivers() -> void {
            while (is_running_) {
                std::optional<system_message<PacketType>> msg = routing_buf_->safeRead();
                if (msg == std::nullopt) continue;

                try {
                    __sys_client_channel& channel = getChannel(msg->receiver);
                    channel.write_buf_->safeWrite(std::move(msg.value()));
                } catch (std::out_of_range& _) {
                    std::cout << "Client: " << msg->receiver << " not found!\n";
                    continue;
                }
            }
        };

        auto notifyClients(const system_notification notification) -> void {
            for (auto& cl : clients_) {
                cl.second.client_->update(notification);
            }
        }


    public:
        System(): is_running_(false), num_clients_(0) {
            routing_buf_ = std::make_unique<SafeRingBuffer<system_message<PacketType>>>(
                Config::getInstance()->getRingBufSize() * Config::getInstance()->getRBufMultiplier()
            );
        }
        ~System() {}

        /**
         * @brief Starts system
         */
        auto start() -> void {
            if (!is_running_) {
                is_running_ = true;
                system_worker_ = std::thread(&droning::System<PacketType>::routeToReceivers, this);
                routing_worker_ = std::thread(&droning::System<PacketType>::routeToSharedBuf, this);
                notifyClients(system_notification::TURN_ON);
            }
        }

        /**
         * @brief Stops system from working
         */
        auto stop() -> void {
            if (is_running_) {
                is_running_ = false;
                notifyClients(system_notification::TURN_OFF);
                if (system_worker_.joinable()) system_worker_.join();
                if (routing_worker_.joinable()) routing_worker_.join();

            }
        }

    	/**
    	* @brief Attaches new client to the system
    	* @param client - new client to be registered in the system
    	*/
        auto attachClient(std::shared_ptr<SystemClient<system_message<PacketType>>> client) -> void {
            struct __sys_client_channel cl_des {
                .client_ = client,
                .read_buf_ = client->getWriteBuf(),
                .write_buf_ = client->getReadBuf(),
            };
            {
                std::lock_guard<std::mutex> guard(runtime_mutex_);
                try {
                    clients_.insert({client->getClientId(), std::move(cl_des)});
                    num_clients_++;
                } catch (std::exception& e) {
                    std::cout << "Client: " << client->getClientId() << " failed to be inserted!\n";
                    std::cout << e.what() << std::endl;
                    return;
                }
            }
        }

        /*
         * @brief Attaches new client to the system and immediately starts it
         * @param client - new client to be registered in the system
         */
        auto attachStartClient(std::shared_ptr<SystemClient<system_message<PacketType>>> client) -> void {
            try {
                attachClient(client);
                client->start();
            } catch (std::exception& e) {
                std::cout << e.what() << std::endl;
                return;
            }
        }

        auto detachClient(std::string client_id) -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            auto iter = clients_.find(std::move(client_id));
            if (iter != clients_.end()) {
                clients_.erase(iter);
                return;
            }
            throw std::out_of_range("Invalid client_id provided!");
        }

        auto detachStopClient(std::string client_id) -> void {
            try {
                getSnapClients().at(client_id).second.client_->stop();
                detachClient(std::move(client_id));
            } catch (std::exception& e) {
                std::cout << e.what() << std::endl;
                return;
            }
        }

        auto attach_client(std::shared_ptr<SystemClient<system_message<PacketType>>> client) -> void {
            attachClient(std::move(client));
        }

        auto attach_start_client(std::shared_ptr<SystemClient<system_message<PacketType>>> client) -> void {
            attachStartClient(std::move(client));
        }

        auto detach_client(std::string client_id) -> void {
            detachClient(std::move(client_id));
        }

        auto detach_stop_client(std::string client_id) -> void {
            detachStopClient(std::move(client_id));
        }

    };

}
