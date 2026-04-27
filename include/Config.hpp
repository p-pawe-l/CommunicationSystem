#pragma once

#include <cstddef>

namespace droning {

    class Config {
    public:
        Config(const Config& obj) = delete;
        Config(const Config&& obj) = delete;
        void operator=(const Config&) = delete;
        void operator=(Config&&) = delete;

        static auto getInstance() -> Config*;

        auto setRingBufSize(const std::size_t new_size) -> void;
        [[nodiscard]] auto getRingBufSize() const -> std::size_t;

        auto setRBufMultiplier(const std::size_t new_multipler) -> void;
        [[nodiscard]] auto getRBufMultiplier() const -> std::size_t;

    private:
        Config(): ring_buf_size_(1024), routing_buf_size_multiplier_(5) {}

        /* Config values */
        std::size_t ring_buf_size_;
        std::size_t routing_buf_size_multiplier_;
    };
}
