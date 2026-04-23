#pragma once 

#include <cstddef>
#include <ostream>
#include <string>
#include <cstdint>

namespace droning {

    enum class system_message_action : uint8_t {
        DETACH = 0,
        DATA_ACTION = 1
    };

    template <typename PacketType>
    struct system_message {
        std::string receiver;
        std::string sender;

        PacketType data;
        system_message_action action;
    };

    namespace drone_data_structs {
        struct __drone_position { std::size_t x_pos; std::size_t y_pos; std::size_t z_pos; };
        struct __drone_velocity { std::size_t x_vel; std::size_t y_vel; std::size_t z_vel; };
        struct __drone_acceleration { std::size_t x_acc; std::size_t y_acc; std::size_t z_acc; };
        struct __drone_engine { std::size_t thrust; std::size_t roll; std::size_t pitch; std::size_t yaw; };
    }

    struct drone_data {
        drone_data_structs::__drone_position dpos;      /* Drone position info      */
        drone_data_structs::__drone_velocity dvel;      /* Drone velocity info      */
        drone_data_structs::__drone_acceleration dacc;  /* Drone acceleration info  */
        drone_data_structs::__drone_engine dengine;     /* Drone engine info        */
    };

    inline auto operator<<(std::ostream& os, const drone_data& data) -> std::ostream& {
        os << "drone_data {\n"
           << "  position: { x: " << data.dpos.x_pos
           << ", y: " << data.dpos.y_pos
           << ", z: " << data.dpos.z_pos << " }\n"
           << "  velocity: { x: " << data.dvel.x_vel
           << ", y: " << data.dvel.y_vel
           << ", z: " << data.dvel.z_vel << " }\n"
           << "  acceleration: { x: " << data.dacc.x_acc
           << ", y: " << data.dacc.y_acc
           << ", z: " << data.dacc.z_acc << " }\n"
           << "  engine: { thrust: " << data.dengine.thrust
           << ", roll: " << data.dengine.roll
           << ", pitch: " << data.dengine.pitch
           << ", yaw: " << data.dengine.yaw << " }\n"
           << "}";
        return os;
    }
}
