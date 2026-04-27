#pragma once 

#include <chrono>
#include <utility>
#include <memory>
#include <mutex>
#include <optional>
#include <condition_variable>

namespace droning {

    template <typename T>
    class RingBuffer {
    protected:
        std::size_t buffer_size_;
        std::size_t size_;
        std::unique_ptr<T[]> buffer_;

        std::size_t write_ptr_;
        std::size_t read_ptr_;

        bool is_full_;
        bool is_empty_;

    public:
        RingBuffer(std::size_t buf_size = 1024): write_ptr_(0), read_ptr_(0), is_full_(false), is_empty_(true) {
            buffer_ = std::make_unique<T[]>(buf_size);
            buffer_size_ = buf_size;
        }
        virtual ~RingBuffer() = default;

        auto write(const T& data) -> void {
            if (is_full_) {
                read_ptr_ = (read_ptr_ + 1) % buffer_size_;
                is_full_ = false;
            }
            buffer_[write_ptr_] = data;
            ++size_;

            write_ptr_ = (write_ptr_ + 1) % buffer_size_;
            is_full_ = (write_ptr_ == read_ptr_);
            is_empty_ = false;
        }


        auto write(T&& data) -> void {
            if (is_full_) {
                read_ptr_ = (read_ptr_ + 1) % buffer_size_;
                is_full_ = false;
            }
            buffer_[write_ptr_] = std::move(data);
            ++size_;

            write_ptr_ = (write_ptr_ + 1) % buffer_size_;
            is_full_ = (write_ptr_ == read_ptr_);
            is_empty_ = false;
        } 

        [[nodiscard]] auto read() -> std::optional<T> {
            if (is_empty_) return std::nullopt;

            auto data = buffer_[read_ptr_];
            read_ptr_ = (read_ptr_ + 1) % buffer_size_;
            --size_;
            is_full_ = false;
            is_empty_ = (read_ptr_ == write_ptr_);

            return data;
        }

        [[nodiscard]] virtual auto isEmpty() const noexcept -> bool { return is_empty_; }
        [[nodiscard]] virtual auto isFull() const noexcept -> bool { return is_full_; }
        [[nodiscard]] virtual auto size() const noexcept -> std::size_t { return size_; }
    };


    template <typename T>
    class SafeRingBuffer : public RingBuffer<T> {
        using cond_v_t = std::condition_variable;

    private:
        mutable std::mutex buf_mutex_;
        // Condition variables for signaling state of the queue
        cond_v_t full_;
        cond_v_t not_empty_;

        bool is_closed_;

    public:
        SafeRingBuffer(std::size_t buf_size):
        RingBuffer<T>(std::move(buf_size))
        {}
        ~SafeRingBuffer() override {}

        /**
        * @brief Sets new value to access flag 
        * Broadcast new state of the buffer to all managing threads
        */
        auto setAccessFlag(const bool is_closed) noexcept -> void {
            {
                std::lock_guard<std::mutex> guard(buf_mutex_);
                is_closed_ = is_closed;
            }
            full_.notify_all();
            not_empty_.notify_all();
        }

        /** 
        * @brief Writes data into buffer and notifies thread that data is inside.
        * CAN OVERWRITE DATA
        */
        auto safeWrite(const T& data) noexcept -> void {
            {
                std::lock_guard<std::mutex> guard(buf_mutex_);
                RingBuffer<T>::write(data);
            }
            // Telling one thread that buffer is not empty and can read from it  
            not_empty_.notify_one(); // or notify_all() ?
        }

        /**
        * @brief Writes data into buffer (waits for data to not bu full).
        * CHILL, WILL NOT OVERWRTIE DATA
        */
        auto safeWaitWrite(const T& data) noexcept -> bool {
            std::unique_lock<std::mutex> uq_guard(buf_mutex_);
            full_.wait(uq_guard, [this]() {return is_closed_ || size() < RingBuffer<T>::buffer_size_; });
            
            if (is_closed_) { return false; }

            RingBuffer<T>::write(data);
            uq_guard.unlock();
            not_empty_.notify_one();
            
            return true;
        }

        auto safeWaitForWrite(const T& data, std::chrono::milliseconds&& timeout) -> bool {
            std::unique_lock<std::mutex> uq_guard(buf_mutex_);

            bool is_ready = full_.wait_for(uq_guard, timeout, [this]() {
                return is_closed_ || size() < RingBuffer<T>::buffer_size_;
            });
            
            if (!is_ready || is_closed_) { return false; }

            RingBuffer<T>::write(data);
            uq_guard.unlock();
            not_empty_.notify_one();

            return true;
        } 

        /** 
        * @brief Writes data into buffer and notifies thread that data is inside.
        * CAN OVERWRITE DATA
        */
        auto safeWrite(T&& data) -> void {
            {
                std::lock_guard<std::mutex> guard(buf_mutex_);
                RingBuffer<T>::write(std::move(data));
            }
        }

        /**
        * @brief Writes data into buffer (waits for data to not bu full).
        * CHILL, WILL NOT OVERWRTIE DATA
        */
        auto safeWaitWrite(T&& data) noexcept -> void {
            safeWrite(data);
        }

        auto safeRead() -> std::optional<T> {
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::read();
        }

        auto safeWriteRead() -> std::optional<T> {

        }
        
        [[nodiscard]] auto isEmpty() const noexcept -> bool override { 
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::is_empty_; 
        }

        [[nodiscard]] auto isFull() const noexcept -> bool override { 
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::is_full_; 
        }

        [[nodiscard]] auto size() const noexcept -> std::size_t override {
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::size_;
        }
    };

}
