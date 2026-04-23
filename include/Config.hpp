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

    private:
        Config() {ring_buf_size_ = 1024; }
        
        /* Config values */
        std::size_t ring_buf_size_;
    };

    auto init_config() -> void;
}