#pragma once

#include "SystemClient.hpp"
#include "Message.hpp"
#include <optional>
#include <iostream>

namespace droning {

    class MockController : public SystemClient<system_message<drone_data>> {
    private:
        std::size_t n_probes_;
        droning::drone_data drone_data_;

        auto getDroneData() -> droning::drone_data {
            // MOCKED - ASKS SYSTEM TO FETCH DRONE DATA

            return {
                    .dpos = {
                        .x_pos = 10 * n_probes_,
                        .y_pos = 20 * n_probes_,
                        .z_pos = 30 * n_probes_
                    },
                    .dvel = {
                        .x_vel = n_probes_,
                        .y_vel = 2 * n_probes_,
                        .z_vel = 3 * n_probes_
                    },
                    .dacc = {
                        .x_acc = 4 * n_probes_,
                        .y_acc = 5 * n_probes_,
                        .z_acc = 6 * n_probes_
                    },
                    .dengine = {
                        .thrust = 100 + n_probes_,
                        .roll = 10 + n_probes_,
                        .pitch = 20 + n_probes_,
                        .yaw = 30 + n_probes_
                    }
                };
        }

        auto generateProbe() -> system_message<drone_data> {
            return {
                .receiver = "AI_MODEL",
                .sender = getClientId(),
                .data = getDroneData(),
                .action = system_message_action::DATA_ACTION
            };
        }
    public:
        MockController(std::string controller_id):
        SystemClient(std::move(controller_id)),
        n_probes_(0)
        {}

        ~MockController() override { stop(); }

        auto generateData() -> void override {
            system_message<drone_data> msg = generateProbe();
            ++n_probes_;

            SystemClient::write_buffer_->safeWrite(msg);
        }

        auto processData() -> void override {
            std::optional<system_message<drone_data>> msg = read_buffer_->safeRead();
            if (msg == std::nullopt) return;
            drone_data_ = std::move(msg->data);

            std::cout << "-------- CONTROLLER --------" << std::endl;
            std::cout << drone_data_ << std::endl;
        }
    };


}
