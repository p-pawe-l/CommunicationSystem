#pragma once 

#include "../RingBuffer.hpp"
#include "../Config.hpp"
#include "PySystem.hpp"

#include <memory>
#include <pybind11/pybind11.h>

#include <iostream>
#include <exception>
#include <pybind11/pytypes.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <optional>


namespace droning::python {
   
    inline constexpr const char* PYTHON_FUNC_ATTR = "__client_func_role__";
    inline constexpr const char* PYTHON_GEN_FUNC_MARK = "generate";
    inline constexpr const char* PYTHON_PROC_FUNC_MARK = "process";

    namespace py = pybind11;

    template <typename PythonPacketType>
    class PyClient {
        using string_t = std::string;
        
        template <typename _T>
        using opt_t = std::optional<_T>;

        using thread_t = std::thread;
        using atomic_bool_t = std::atomic<bool>;
        using mutex_t = std::mutex;

    private:
        string_t client_id_;                            /** < Identificator in the system */
        
        opt_t<py::object> python_generating_func_;      /** < Python generating func */
        opt_t<py::object> python_processing_func_;      /** < Python processing func */

        SafeRingBuffer<PythonPacketType> inbox_buf_;    /** < Inbox buffer for incoming messages */
        SafeRingBuffer<PythonPacketType> outbox_buf_;   /** < Outbox buffer for sending message to system */
    
        PySystem<PythonPacketType>* ptr_system_;        /** < Pointer to connected system */

        thread_t generate_worker_;                      /** < Thread responsible for generating data */
        thread_t process_worker_;                       /** < Thread responsible for processing data */

        atomic_bool_t is_running_;                      /** < ATOMIC flag for checking is client is running */
        bool start_stop_;                               /** < Flag for automatic starting and stoping client */ 
    
    public:
        PyClient(string_t& client_id, PySystem<PythonPacketType>* ptr_system, bool start_stop = false): 
        client_id_(std::move(client_id)),
        ptr_system_(std::move(ptr_system)),
        start_stop_(start_stop),
        python_generating_func_(std::nullopt),
        python_processing_func_(std::nullopt)
        {
            initBuffers();
            if (start_stop_) start();
        }
        PyClient(string_t& client_id, PySystem<PythonPacketType>* ptr_system, py::object gen_func, py::object proc_func, bool start_stop = false):
        client_id_(std::move(client_id)),
        ptr_system_(std::move(ptr_system)),
        start_stop_(start_stop)
        {
            assignGeneratingFunc(std::move(gen_func));
            assignProcessingFunc(std::move(proc_func));
            initBuffers();
            if (start_stop_) start();
        }
        ~PyClient() {
            if (start_stop_) stop();
        }

        auto start() noexcept -> void {
            if (!is_running_) {
                is_running_ = true;

                checkForWorkersFunctions();
                generate_worker_ = std::thread(&PyClient::generateDataLoop, this);
                process_worker_ = std::thread(&PyClient::processDataLoop, this);
            }
        }

        auto stop() noexcept -> void {
            if (is_running_) {
                is_running_ = false;
                if (generate_worker_.joinable()) generate_worker_.join();
                if (process_worker_.joinable()) process_worker_.join();
                inbox_buf_.setNotAccessFlag(true);
                outbox_buf_.setNotAccessFlag(true);
            }
        }

        auto assignGeneratingFunc(py::object func) -> void {
            auto func_type = getFuncMetaType(func, PYTHON_FUNC_ATTR);
            if (func_type != PYTHON_GEN_FUNC_MARK) {
                throw std::runtime_error("Generating func must be marked with @genereate_func");
            }
            python_generating_func_ = std::move(func);
        }

        auto assignProcessingFunc(py::object func) -> void {
            auto func_type = getFuncMetaType(func, PYTHON_FUNC_ATTR);
            if (func_type != PYTHON_PROC_FUNC_MARK) {
                throw std::runtime_error("Processing func must be marked with @process_func");
            }
            python_processing_func_ = std::move(func);
        }

        [[nodiscard]] auto isRunnnig() -> bool { return is_running_; }

    private:
        auto initBuffers() noexcept -> void {
            Config* conf = Config::getInstance();
            inbox_buf_ = std::make_unique<SafeRingBuffer<PythonPacketType>>(conf->getRingBufSize());
            outbox_buf_ = std::make_unique<SafeRingBuffer<PythonPacketType>>(conf->getRingBufSize());
        }

        [[nodiscard]] auto getFuncMetaType(const py::object& func, std::string&& meta_descriptor) -> std::string {
            auto func_type = py::getattr(func, std::move(meta_descriptor.c_str()), py::none());
            if (func_type.is_none()) return "";
            return func_type.cast<std::string>();
        }

        auto checkForWorkersFunctions() -> void {
            std::string missing_funcs = "";
            if (python_generating_func_ == std::nullopt) {
                missing_funcs += "Generating Function,";
            }
            if (python_processing_func_ == std::nullopt) {
                missing_funcs += "Processing Function,";
            }
            if (missing_funcs != "") {
                throw std::runtime_error(
                    std::string("Missing functions for PyClient workers!") +
                    std::string("Update this functions: ") + std::move(missing_funcs)
                );
            }
        }

        auto generateDataLoop() -> void {
            while (is_running_) {
                // Python function for gen data loop 
            }
        }

        auto processDataLoop() -> void {
            while (is_running_) {
                // Python function for proc data loop
            }
        }

        enum class __client_system_notification : uint8_t {
            CLIENT_STARTED_WORKING =    0x00,
            CLIENT_STOPPED_WORKING =    0x01, 
            CLIENT_STARTED_SLEEPING =   0x02,
            CLIENT_STOPPED_SLEEPING =   0x03,
        };

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
