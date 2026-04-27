#pragma once 

#include "../RingBuffer.hpp"
#include "../Config.hpp"
#include "PySystem.hpp"

#include <pybind11/pybind11.h>

#include <iostream>
#include <exception>
#include <string>
#include <thread>
#include <atomic>

namespace droning::python {

    template <typename PythonPacketType>
    class PyClient {
        using string_t = std::string;
        using thread_t = std::thread;
        using atomic_bool_t = std::atomic<bool>;
        using py_t = pybind11;

    private:
        string_t client_id_;                            /** < Identificator in the system */
        py_t::object python_object_;                      /** < Object from python code */

        SafeRingBuffer<PythonPacketType> inbox_buf_;    /** < Inbox buffer for incoming messages */
        SafeRingBuffer<PythonPacketType> outbox_buf_;   /** < Outbox buffer for sending message to system */
    
        PySystem<PythonPacketType>* ptr_system_;        /** < Pointer to connected system */

        thread_t generate_worker_;                      /** < Thread responsible for generating data */
        thread_t process_worker_;                       /** < Thread responsible for processing data */

        atomic_bool_t is_running_;                      /** < ATOMIC flag for checking is client is running */

    public:
        PyClient(string_t& client_id, PySystem<PythonPacketType>* ptr_system): 
        client_id_(std::move(client_id)),
        ptr_system_(std::move(ptr_system))
        {
            inbox_buf_ = SafeRingBuffer<PythonPacketType>(Config::getInstance()->getRingBufSize());
            outbox_buf_ = SafeRingBuffer<PythonPacketType>(Config::getInstance()->getRingBufSize());
            notifySystem(std::move(__client_system_notification::CLIENT_STARTED_WORKING));
        }
        virtual ~PyClient() {
            notifySystem(std::move(__client_system_notification::CLIENT_STOPED_WORKING));
        }

        // Manual operations on Pythonic-Client
        auto start() -> void;
        auto stop() -> void; 
        [[nodiscard]] auto isRunnnig() -> bool { return is_running_; }

    private:     
        enum class __client_system_notification : uint8_t {
            CLIENT_STARTED_WORKING = 0x00,
            CLIENT_STOPED_WORKING = 0x01, 
            CLIENT_STARTED_SLEEPING = 0x02,
            CLIENT_STOPED_SLEEPING = 0x03
        };
        
        auto generateDataLoop() -> void;
        auto processDataLoop() -> void;

        /**
        * @brief Notify systems about changes in client
        */
        auto notifySystem(__client_system_notification&& n) -> void {
            if (!isRunnnig()) { return; }
            try { ptr_system_->updateClientDescriptor(client_id_, n); }
            catch (std::exception& e) {
                std::cerr << "System notifying failure!\n";
                std::cerr << e.what() << "\n";
            }
        }

    };

}
