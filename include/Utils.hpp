#pragma once

#include <cstdint>


namespace droning  {

    enum class system_notification : uint8_t {
        TURN_ON = 0,
        TURN_OFF = 1
    };

    class SystemSubscriber {
    public:
        SystemSubscriber() = default;
        virtual ~SystemSubscriber() = default;

        virtual auto update(const system_notification notification) -> void = 0;
    };
}
