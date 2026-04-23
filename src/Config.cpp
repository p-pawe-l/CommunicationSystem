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
