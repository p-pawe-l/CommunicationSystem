#pragma once 

#include "RingBuffer.hpp"
#include <atomic>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <memory>

#include "Config.hpp"

namespace droning {


    template <typename T>
    class Callback {
    private:
        std::string cb_name_;

    public:
        Callback(std::string cb_name): cb_name_{std::move(cb_name)} {}

        [[nodiscard]] auto getName() const -> std::string { return cb_name_; }

        virtual auto executeOnSend(const T& data) -> void = 0;
        virtual auto executeOnReceive(const T& data) -> void = 0;


    };

    template <typename T>
    class SystemClient  {
    private:
        /**
        * @brief Wrapper for callback`s function execution  
        */
        struct __callback_struct_data {
            T data_;            /** < Data to be processed by callback */
            bool is_on_send_;   /** < Flag signaling is data was on send or on receive */
        };

        auto notify_system() -> void { 
            // TODO
        }

    protected:
        std::shared_ptr<RingBuffer<T>> read_buffer_;
        std::shared_ptr<RingBuffer<T>> write_buffer_;
        std::unique_ptr<RingBuffer<__callback_struct_data>> callback_buffer_;

        std::vector<std::shared_ptr<Callback<T>>> callbacks_;

        std::shared_ptr<std::mutex> read_mutex_;
        std::shared_ptr<std::mutex> writer_mutex_;
        std::mutex callback_mutex_;

        std::thread gen_worker_;
        std::thread proc_worker_;
        std::thread callback_worker_;

        std::string client_id_;
        
        std::atomic<bool> is_running_;

        auto threadEntryGenerating() -> void { while (is_running_) generateData(); }
        auto threadEntryProcessing() -> void { while (is_running_) processData(); }
        auto threadEntryRunningCallbacks() -> void { while (is_running_) processCallbacks(); }

    public:
        SystemClient(std::string client_id): client_id_{std::move(client_id)}, is_running_(false) {
            read_buffer_ = std::make_shared<RingBuffer<T>>(Config::getInstance()->getRingBufSize());
            write_buffer_ = std::make_shared<RingBuffer<T>>(Config::getInstance()->getRingBufSize());
            callback_buffer_ = std::make_unique<RingBuffer<__callback_struct_data>>(Config::getInstance()->getRingBufSize() * 2);
            read_mutex_ = std::make_shared<std::mutex>();
            writer_mutex_ = std::make_shared<std::mutex>();
        }

        virtual ~SystemClient() {
            notify_system();
            stop();
        }

        /**
        * @brief Function for starting the client. Sets running flag to true and
        * starts all workers (threads).
        */
        auto start() -> void {
            if (!is_running_) {
                is_running_ = true;
                gen_worker_ = std::thread(&SystemClient<T>::threadEntryGenerating, this);
                proc_worker_ = std::thread(&SystemClient<T>::threadEntryProcessing, this);
                callback_worker_ = std::thread(&SystemClient<T>::threadEntryRunningCallbacks, this);
            }
        }

        /**
        * @brief Function for stopping the client. Sets running flag to false and
        * stops all workers (threads).
        */
        auto stop() -> void {
            if (is_running_) {
                is_running_ = false;

                if (gen_worker_.joinable()) gen_worker_.join();
                if (proc_worker_.joinable()) proc_worker_.join();
                if (callback_worker_.joinable()) callback_worker_.join();
            }
        }

        auto processCallbacks() -> void  {
            std::optional<__callback_struct_data> data = callback_buffer_->read();
            if (data == std::nullopt) return;
             
            std::vector<std::shared_ptr<Callback<T>>> cb_snap;
            {
                /* Taking snapshot of callbacks collection */
                std::lock_guard<std::mutex> cb_collection_guard(callback_mutex_);
                cb_snap = callbacks_;
            }   

            for (std::shared_ptr<Callback<T>> cb_ptr : cb_snap) {
                try {
                    const auto& val = data.value();
                    val.is_on_send_ ? 
                    cb_ptr->executeOnSend(std::move(val.data_)) : cb_ptr->executeOnReceive(std::move(val.data_));
                } catch (std::exception& e) {
                    std::cout << "Callback: " << cb_ptr->getName() << " failed!" << std::endl;
                    std::cout << e.what() << std::endl;
                    continue;
                }    
            }
        }

        virtual auto generateData() -> void = 0;
        virtual auto processData() -> void = 0;

        auto addCallback(Callback<T>* cb_ptr) -> void { callbacks_.push_back(std::move(cb_ptr)); }
        [[nodiscard]] auto getReadBuf() -> std::shared_ptr<RingBuffer<T>> { return read_buffer_; }
        [[nodiscard]] auto getWriteBuf() -> std::shared_ptr<RingBuffer<T>> { return write_buffer_; }
        [[nodiscard]] auto getReadMutex() -> std::shared_ptr<std::mutex> { return read_mutex_; }
        [[nodiscard]] auto getWriteMutex() -> std::shared_ptr<std::mutex> { return writer_mutex_; }
        
        [[nodiscard]] auto getClientId() const -> std::string { return client_id_; }
    };

}
