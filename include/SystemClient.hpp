#pragma once

#include "RingBuffer.hpp"
#include "Utils.hpp"
#include "Config.hpp"

#include <atomic>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <memory>


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
    class SystemClient : public SystemSubscriber {
    private:
        /**
        * @brief Wrapper for callback`s function execution
        */
        struct __callback_struct_data {
            T data_;            /** < Data to be processed by callback */
            bool is_on_send_;   /** < Flag signaling is data was on send or on receive */
        };

	/**
	* @brief Thread-Safely loads all callbacks stored for system client
	*/
	[[nodiscard]] auto getCallbacksSnap() const -> std::vector<std::shared_ptr<Callback<T>>> {
		std::lock_guard<std::mutex> callbacks_collection_guard(callback_mutex_);
		return callbacks_;
	}

    protected:
        std::shared_ptr<SafeRingBuffer<T>> read_buffer_;
        std::shared_ptr<SafeRingBuffer<T>> write_buffer_;
        std::unique_ptr<SafeRingBuffer<__callback_struct_data>> callback_buffer_;

        std::vector<std::shared_ptr<Callback<T>>> callbacks_;

        mutable std::mutex callback_mutex_;
	    mutable std::mutex runtime_mutex_;

        std::thread gen_worker_;
        std::thread proc_worker_;
        std::thread callback_worker_;

        std::string client_id_;

        std::atomic<bool> is_running_;

        auto threadEntryGenerating() -> void { while (is_running_) generateData(); }
        auto threadEntryProcessing() -> void { while (is_running_) processData(); }
        auto threadEntryRunningCallbacks() -> void { while (is_running_) processCallbacks(); }

        auto notifySystem() -> void {
            // any shared buffer with conditional variable that informs system
            // that client want to notify system
            // Like notification buffer
        }

        auto routeToCallbackBuffer() -> void {
            // TODO
            // Routes data to callback buffer
        }

        /**
         * @brief Notifies callback worker to wake_up when callbacks collection is not empty
         */
        auto notifyCallbackWorker() -> void {
            // TODO
            // Implement sleeping for callback worker when there is no callbacks to process
        }

        auto processCallbacks() -> void  {
            std::optional<__callback_struct_data> data = callback_buffer_->safeRead();
            if (data == std::nullopt) return;

            for (std::shared_ptr<Callback<T>>& cb_ptr : getCallbacksSnap()) {
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

    public:
        SystemClient(std::string client_id): client_id_{std::move(client_id)}, is_running_(false) {
            read_buffer_ = std::make_shared<SafeRingBuffer<T>>(Config::getInstance()->getRingBufSize());
            write_buffer_ = std::make_shared<SafeRingBuffer<T>>(Config::getInstance()->getRingBufSize());
            callback_buffer_ = std::make_unique<SafeRingBuffer<__callback_struct_data>>(Config::getInstance()->getRingBufSize() * 2);
        }

        virtual ~SystemClient() {}

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
                notifySystem();
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
                notifySystem();
            }
        }

        virtual auto generateData() -> void = 0;
        virtual auto processData() -> void = 0;

        /**
        * @brief Function used for automatically starting and
        * stoping clients of the system
        */
        auto update(const system_notification notification) -> void override {
            switch (notification) {
                case droning::system_notification::TURN_ON:
                    start();
                    break;
                case droning::system_notification::TURN_OFF:
                    stop();
                    break;
            }
        }

        auto addCallback(Callback<T>* cb_ptr) -> void {
            std::lock_guard<std::mutex> cb_guard(callback_mutex_);
            callbacks_.push_back(std::move(cb_ptr));
            notifyCallbackWorker();
	    }
        [[nodiscard]] auto getReadBuf() -> std::shared_ptr<SafeRingBuffer<T>>& {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            return read_buffer_;
	    }
        [[nodiscard]] auto getWriteBuf() -> std::shared_ptr<SafeRingBuffer<T>>& {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            return write_buffer_;
        }
        [[nodiscard]] auto getClientId() const -> std::string {
            std::lock_guard<std::mutex> guard(runtime_mutex_);
            return client_id_;
        }

    };

}
