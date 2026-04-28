#pragma once 

#include "../RingBuffer.hpp"
#include "../Config.hpp"
#include "PySystem.hpp"
#include "PyPacket.hpp"

#include <memory>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>

#include <iostream>
#include <exception>
#include <pybind11/pytypes.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <optional>
#include <chrono>
#include <type_traits>
#include <vector>


namespace droning::python {
   
    inline constexpr const char* PYTHON_FUNC_ATTR = "_client_func_type";
    inline constexpr const char* PYTHON_GEN_FUNC_MARK = "generate";
    inline constexpr const char* PYTHON_PROC_FUNC_MARK = "process";
    inline constexpr std::size_t PY_CLIENT_PROCESS_BATCH_LIMIT = 64;

    namespace py = pybind11;

    template <typename PythonPacketType>
    class PyClient {
        using string_t = std::string;
        
        template <typename _T>
        using opt_t = std::optional<_T>;

        template <typename _T>
        using u_ptr = std::unique_ptr<_T>;

        using thread_t = std::thread;
        using atomic_bool_t = std::atomic<bool>;
        using mutex_t = std::mutex;

    private:
        string_t client_id_;                                    /** < Identificator in the system */
        
        opt_t<py::object> python_generating_func_;              /** < Python generating func */
        opt_t<py::object> python_processing_func_;              /** < Python processing func */

        u_ptr<SafeRingBuffer<PythonPacketType>> inbox_buf_;     /** < Inbox buffer for incoming messages */
        u_ptr<SafeRingBuffer<PythonPacketType>> outbox_buf_;    /** < Outbox buffer for sending message to system */
    
        PySystem<PythonPacketType>* ptr_system_;                /** < Pointer to connected system */

        thread_t generate_worker_;                              /** < Thread responsible for generating data */
        thread_t process_worker_;                               /** < Thread responsible for processing data */
        mutex_t runtime_mutex_;                                 /** < Runtime mutex for thread-safe operations on PyClient */

        atomic_bool_t is_running_;                              /** < ATOMIC flag for checking is client is running */
        atomic_bool_t bufs_initialized_;                        /** < ATOMIC flag for checking if buffors were initialized */
        atomic_bool_t auto_start_;                              /** < ATOMIC Flag for automatic starting and stoping client */ 
    
    public:
        /**
         * @brief Creates Python client without assigned worker functions.
         *
         * Worker functions must be assigned before start() is called.
         *
         * @param client_id: Identifier of the client in the system.
         * @param ptr_system: Pointer to connected Python system.
         * @param start_stop: Flag for automatic start and stop in constructor/destructor.
         */
        PyClient(string_t client_id, PySystem<PythonPacketType>* ptr_system, bool auto_start = true): 
        client_id_(std::move(client_id)),
        ptr_system_(std::move(ptr_system)),
        bufs_initialized_(false),
        is_running_(false),
        auto_start_(auto_start),
        python_generating_func_(std::nullopt),
        python_processing_func_(std::nullopt)
        {   
            if (!bufs_initialized_) initBuffers();
            if (auto_start_) start();
        }

        /**
         * @brief Creates Python client with assigned worker functions.
         *
         * Provided functions are validated against their decorator metadata
         * before they are stored by the client.
         *
         * @param client_id: Identifier of the client in the system.
         * @param ptr_system: Pointer to connected Python system.
         * @param gen_func: Python function used by generating worker.
         * @param proc_func: Python function used by processing worker.
         * @param start_stop: Flag for automatic start and stop in constructor/destructor.
         */
        PyClient(string_t client_id, PySystem<PythonPacketType>* ptr_system, py::object gen_func, py::object proc_func, 
                 bool auto_start = false):
        client_id_(std::move(client_id)),
        ptr_system_(ptr_system),
        bufs_initialized_(false),
        is_running_(false),
        auto_start_(auto_start)
        {
            assignGeneratingFunc(std::move(gen_func));
            assignProcessingFunc(std::move(proc_func));
            if (!bufs_initialized_) initBuffers();
            if (auto_start_) start();
        }

        /**
         * @brief Destroys Python client.
         *
         * If automatic lifecycle management is enabled, worker threads are
         * stopped before destruction finishes.
         */
        ~PyClient() { stop(); }

        /**
         * @brief Starts generating and processing workers.
         *
         * Checks that both Python worker functions are assigned before
         * launching threads.
         */
        auto start() -> void {
            checkForWorkersFunctions();

            if (!is_running_) {
                is_running_ = true;
                setCloseFlagForBuffers(false);
                
                // True there means that workers are starting to work
                startStopWorkers(true);
            }
        }

        /**
         * @brief Stops generating and processing workers.
         *
         * Closes buffers to wake waiting worker threads, then joins them.
         */
        auto stop() -> void {
            if (is_running_) {
                is_running_ = false;
                setCloseFlagForBuffers(true);

                // False there means to join workers and after 
                // joining stops them from working 
                startStopWorkers(false);
            }
        }

        /**
         * @brief Assigns Python function used by generating worker.
         *
         * Function must be marked with generate metadata.
         *
         * @param func: Python callable used for generating data.
         */
        auto assignGeneratingFunc(py::object func) -> void {
            auto func_type = getFuncMetaType(func, PYTHON_FUNC_ATTR);
            if (func_type != PYTHON_GEN_FUNC_MARK) {
                throw std::runtime_error("Generating func must be marked with @genereate_func");
            }
            python_generating_func_ = std::move(func);
        }

        /**
         * @brief Assigns Python function used by processing worker.
         *
         * Function must be marked with process metadata.
         *
         * @param func: Python callable used for processing received data.
         */
        auto assignProcessingFunc(py::object func) -> void {
            auto func_type = getFuncMetaType(func, PYTHON_FUNC_ATTR);
            if (func_type != PYTHON_PROC_FUNC_MARK) {
                throw std::runtime_error("Processing func must be marked with @process_func");
            }
            python_processing_func_ = std::move(func);
        }

        /**
         * @brief Checks whether client workers are running.
         *
         * @return bool: True if client is running, otherwise false.
         */
        [[nodiscard]] auto isRunnning() -> bool { return is_running_; }

        [[nodiscard]] auto getClientId() const -> std::string { return client_id_; }

        /**
         * @brief Returns reference to client inbox buffer.
         *
         * The buffer is owned by PyClient. Callers receive a borrowed
         * reference that can be used for reading or writing messages, but the
         * reference must not outlive the client.
         *
         * @return SafeRingBuffer<PythonPacketType>&: Borrowed inbox buffer reference.
         */
        [[nodiscard]] auto getInboxBuffer() -> SafeRingBuffer<PythonPacketType>& { return *inbox_buf_; }

        /**
         * @brief Returns reference to client outbox buffer.
         *
         * The buffer is owned by PyClient. Callers receive a borrowed
         * reference that can be used for reading or writing messages, but the
         * reference must not outlive the client.
         *
         * @return SafeRingBuffer<PythonPacketType>&: Borrowed outbox buffer reference.
         */
        [[nodiscard]] auto getOutboxBuffer() -> SafeRingBuffer<PythonPacketType>& { return *outbox_buf_; }

        /**
         * @brief Writes message into client inbox buffer.
         *
         * This helper is intended for Python-facing integration tests and
         * system delivery code.
         *
         * @param message: Message delivered to client inbox.
         */
        auto pushInbox(PythonPacketType message) -> void {
            inbox_buf_->safeWrite(std::move(message));
        }

        /**
         * @brief Reads generated message from client outbox buffer.
         *
         * @return py::typing::Optional<PythonPacketType>: Message generated by
         * client or None when outbox is empty.
         */
        auto popOutbox() -> py::typing::Optional<PythonPacketType> {
            PythonPacketType message;
            const auto res = outbox_buf_->safeRead(&message);
            if (res != 0x01) return py::none();
            return std::move(message);
        }

    private:
        /**
         * @brief Initializes inbox and outbox buffers using configured size.
         */
        auto initBuffers() noexcept -> void {
            Config* conf = Config::getInstance();
            inbox_buf_ = std::make_unique<SafeRingBuffer<PythonPacketType>>(conf->getRingBufSize());
            outbox_buf_ = std::make_unique<SafeRingBuffer<PythonPacketType>>(conf->getRingBufSize());
            bufs_initialized_ = true;
        }

        /**
         * @brief Sets close flag for both client buffers.
         *
         * @param flag: New close state for inbox and outbox buffers.
         */
        auto setCloseFlagForBuffers(bool flag) noexcept -> void {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            inbox_buf_->setAccessFlag(flag);
            outbox_buf_->setAccessFlag(flag);
        }

        /**
         * @brief Starts or joins worker threads.
         *
         * @param start_flag: True starts workers, false joins existing workers.
         */
        auto startStopWorkers(bool start_flag) noexcept -> void {
            if (start_flag) {
                generate_worker_ = std::thread(&PyClient::generateDataLoop, this);
                process_worker_ = std::thread(&PyClient::processDataLoop, this);
            } else {
                if (generate_worker_.joinable()) generate_worker_.join();
                if (process_worker_.joinable()) process_worker_.join();
            }
        }

        /**
         * @brief Reads metadata marker from Python function.
         *
         * @param func: Python function object to inspect.
         * @param meta_descriptor: Name of Python attribute storing function role.
         *
         * @return std::string: Metadata value or empty string when attribute is missing.
         */
        [[nodiscard]] auto getFuncMetaType(const py::object& func, std::string&& meta_descriptor) -> std::string {
            auto func_type = py::getattr(func, std::move(meta_descriptor.c_str()), py::none());
            if (func_type.is_none()) return "";
            return func_type.cast<std::string>();
        }

        /**
         * @brief Checks that both worker functions are assigned.
         *
         * Throws when generating or processing function is missing.
         */
        auto checkForWorkersFunctions() -> void {
            std::string missing_funcs = "";
            if (python_generating_func_ == std::nullopt) missing_funcs += "Generating Function,";
            if (python_processing_func_ == std::nullopt) missing_funcs += "Processing Function,";
            if (missing_funcs != "") {
                throw std::runtime_error
                (
                    std::string("Missing functions for PyClient workers!") +
                    std::string("Update this functions: ") + std::move(missing_funcs)
                );
            }
        }

        /**
         * @brief Handles status returned by buffer operations.
         *
         * @param res: Buffer operation status code.
         * @param buf_name: Name of buffer used in warning messages.
         *
         * @return bool: True if operation succeeded, otherwise false.
         */
        [[nodiscard]] auto handleBufferResult(const uint8_t res, std::string&& buf_name) -> bool {
            if (res == 0x01) return true;

            std::cerr << "WARNING FOR CLIENT: " << client_id_ << "!\n";
            if (res == 0x00) std::cerr << "Closed " << buf_name << " buffer!\n";
            else std::cerr << "Buffer " << buf_name << " operation failed with code: " << static_cast<int>(res) << "\n";

            return false;
        }

        /**
         * @brief Worker loop that calls Python generating function.
         *
         * Generated data is written to outbox buffer for delivery to system.
         */
        auto generateDataLoop() -> void {
            while (is_running_) {
                PythonPacketType data;
                {
                    py::gil_scoped_acquire gil;
                    try {
                        if constexpr (std::is_same_v<PythonPacketType, PyPacket>) {
                            py::object raw_message = (python_generating_func_.value())();
                            data = pyPacketFromObject(raw_message);
                        } else {
                            data = (python_generating_func_.value())();
                        }
                    }
                    catch (std::exception& e) {
                        std::cerr << "Error during execution python generating function" << std::endl;
                        std::cerr << e.what() << std::endl;
                        continue;
                    }
                }

                outbox_buf_->safeWrite(std::move(data));
            }
        }

        /**
         * @brief Worker loop that calls Python processing function.
         *
         * Incoming data is read from inbox buffer and passed to Python code.
         */
        auto processDataLoop() -> void {
            while (is_running_) {
                uint8_t res;
                PythonPacketType incoming_data;
                res = inbox_buf_->safeWaitRead(&incoming_data);

                if (res == 0x01) {
                    std::vector<PythonPacketType> incoming_batch;
                    incoming_batch.reserve(PY_CLIENT_PROCESS_BATCH_LIMIT);
                    incoming_batch.push_back(std::move(incoming_data));

                    while (incoming_batch.size() < PY_CLIENT_PROCESS_BATCH_LIMIT) {
                        PythonPacketType next_data;
                        const auto next_res = inbox_buf_->safeRead(&next_data);
                        if (next_res != 0x01) break;
                        incoming_batch.push_back(std::move(next_data));
                    }

                    py::gil_scoped_acquire gil;
                    for (auto& packet : incoming_batch) {
                        try {
                            if constexpr (std::is_same_v<PythonPacketType, PyPacket>) {
                                (python_processing_func_.value())(pyPacketToDict(packet));
                            } else {
                                (python_processing_func_.value())(std::move(packet));
                            }
                        }
                        catch (std::exception& e) {
                            std::cerr << "Error during execution python processing function" << std::endl;
                            std::cerr << e.what() << std::endl;
                        }
                    }
                }

                if (res == 0x00 && !is_running_) break;
                if (!handleBufferResult(res, "inbox")) continue;
            }
        }

        enum class __client_system_notification : uint8_t {
            CLIENT_STARTED_WORKING =    0x00,
            CLIENT_STOPPED_WORKING =    0x01,
            CLIENT_STARTED_SLEEPING =   0x02,
            CLIENT_STOPPED_SLEEPING =   0x03,
        };

        /**
         * @brief Notifies system about changes in client state.
         * // TODO - implement real usage of this function 
         * @param n: Client state notification.
         */
        auto notifySystem(__client_system_notification&& n) -> void {
            if (!is_running_) { return; }
            try { ptr_system_->updateClientDescriptor(client_id_, n); }
            catch (std::exception& e) {
                std::cerr << "System notifying failure!\n";
                std::cerr << e.what() << "\n";
            }
        }

    };

}
