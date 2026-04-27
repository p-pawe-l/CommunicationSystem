#pragma once

#include "Message.hpp"
#include "SystemClient.hpp"
#include <optional>

namespace droning {

    class MockAIModel : public SystemClient<system_message<drone_data>> {
    private:
        std::optional<system_message<drone_data>> pending_response_;

        auto train_model(drone_data* data) -> void {}

        auto predict(drone_data* data) -> void {
            data->dpos.x_pos += 1;
            data->dpos.y_pos += 1;
            data->dpos.z_pos += 1;
        }

    public:
        MockAIModel(std::string model_id): SystemClient(std::move(model_id)) {}
        ~MockAIModel() override { stop(); }

        auto processData() -> void override {
            std::optional<system_message<drone_data>> msg = read_buffer_->safeRead();
            if (msg == std::nullopt) return;

            train_model(&msg->data);
            predict(&msg->data);

            pending_response_ = system_message<drone_data>{
                .receiver = "MOCK_CONTROL",
                .sender = getClientId(),
                .data = std::move(msg->data),
                .action = system_message_action::DATA_ACTION
            };
        }

        auto generateData() -> void override {
            if (!pending_response_.has_value()) return;

            write_buffer_->safeWrite(std::move(*pending_response_));
            pending_response_.reset();
        }
    };
}
