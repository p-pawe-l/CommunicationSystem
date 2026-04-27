#include "Config.hpp"


auto droning::Config::getInstance() -> droning::Config* {
    static Config instance_ptr_;
    return &instance_ptr_;
}

auto droning::Config::getRingBufSize() const -> std::size_t {
    return ring_buf_size_;
}

auto droning::Config::setRingBufSize(const std::size_t new_size) -> void {
    ring_buf_size_ = new_size;
}

auto droning::Config::getRBufMultiplier() const -> std::size_t {
    return routing_buf_size_multiplier_;
}

auto droning::Config::setRBufMultiplier(const std::size_t new_multiplier) -> void {
    routing_buf_size_multiplier_ = new_multiplier;
}