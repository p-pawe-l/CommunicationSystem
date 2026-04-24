#pragma once

#include "SystemClient.hpp"
#include "RingBuffer.hpp"
#include "Message.hpp"
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <string>
#include <iostream>
#include <atomic>
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
            std::shared_ptr<RingBuffer<system_message<PacketType>>> read_buf_;   /**< Buffer to read data from client   */
            std::shared_ptr<RingBuffer<system_message<PacketType>>> write_buf_;  /**< Buffer to send data to client     */

            std::shared_ptr<std::mutex> read_mutex_;                             /**< Reading data from client - mutex  */
            std::shared_ptr<std::mutex> write_mutex_;                            /**< Sending data from client - mutex  */
        };

    private:
        std::unordered_map<std::string, struct __sys_client_channel> clients_;
        std::unique_ptr<RingBuffer<system_message<PacketType>>> routing_buf_;   

        std::atomic<bool> is_running_;
    
        std::thread system_worker_;
        std::thread routing_worker_;
        
        std::mutex runtime_mutex_;
        std::mutex routing_mutex_;

        inline auto getChannel(const std::string& client_id) -> __sys_client_channel& {
            return clients_.at(client_id);
        }

        /**
        * @brief Takes snapshot of collection of all clients in the system
        */
        inline auto getSnapClient() -> std::unordered_map<std::string, struct __sys_client_channel> {
            std::lock_guard<std::mutex> client_collection_guard(runtime_mutex_);
            return clients_;
        }
        
        auto routeToSharedBuf() -> void {
            while (is_running_) {
                for (auto& [_, channel] : getSnapClient()) {
                    
                    std::optional<system_message<PacketType>> msg;
                    {
                        std::lock_guard<std::mutex> read_guard(*(channel.read_mutex_));
                        msg = channel.read_buf_->read();
                    }

                    if (msg == std::nullopt) continue;
                    if (msg->action == system_message_action::DETACH) {
                        try {
                            detach_client(msg->sender);
                            std::cout << "Client: " << msg->sender << " detach\n";
                        } catch (std::out_of_range& _) {
                            std::cout << "Client: " << msg->sender << " detachment failed!\n";
                        }
                        continue;
                    }

                    {
                        std::lock_guard<std::mutex> routing_guard(routing_mutex_);
                        routing_buf_->write(std::move(msg.value()));
                    }
                }
            }
        }

        auto routeToReceivers() -> void {
            while (is_running_) {
                std::optional<system_message<PacketType>> msg = routing_buf_->read();
                if (msg == std::nullopt) continue;
                
                try {
                    __sys_client_channel& channel = getChannel(msg->receiver);
                    channel.write_buf_->write(*msg);
                } catch (std::out_of_range& _) {
                    std::cout << "Client: " << msg->receiver << " not found!\n"; 
                    continue;
                }
            }
        };

    public:
        System(): is_running_(false) {
            routing_buf_ = std::make_unique<RingBuffer<system_message<PacketType>>>(Config::getInstance()->getRingBufSize());
        }
        ~System() {
            is_running_ = false;
            if (system_worker_.joinable()) system_worker_.join();
            if (routing_worker_.joinable()) routing_worker_.join();
        }

        auto start() -> void {
            if (!is_running_) {
                is_running_ = true;
                system_worker_ = std::thread(&droning::System<PacketType>::routeToReceivers, this);
                routing_worker_ = std::thread(&droning::System<PacketType>::routeToSharedBuf, this);
            }
        } 

        auto stop() -> void {
            if (is_running_) {
                is_running_ = false;
                if (system_worker_.joinable()) system_worker_.join();
                if (routing_worker_.joinable()) routing_worker_.join();
            }
        }

        auto attach_client(SystemClient<system_message<PacketType>>* client) -> void {
            struct __sys_client_channel cl_des {
                .read_buf_ = client->getWriteBuf(),
                .write_buf_ = client->getReadBuf(),
                .read_mutex_ = client->getWriteMutex(),
                .write_mutex_ = client->getReadMutex()
            };
            {
                std::lock_guard<std::mutex> guard(runtime_mutex_);
                clients_.insert({client->getClientId(), std::move(cl_des)});
            }
        }

        auto detach_client(std::string client_id) -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            auto iter = clients_.find(std::move(client_id));
            if (iter != clients_.end()) {
                clients_.erase(iter);
                return;
            }
            throw std::out_of_range("Invalid client_id provided!");
        }

    };

}
