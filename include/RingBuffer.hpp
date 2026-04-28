#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

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
        /**
         * @brief Creates a ring buffer with fixed capacity.
         *
         * @param buf_size: Maximum number of elements stored by the buffer.
         */
        RingBuffer(std::size_t buf_size = 1024): 
            write_ptr_(0), 
            read_ptr_(0), 
            size_(0), 
            buffer_size_(buf_size),
            buffer_(std::make_unique<T[]>(buffer_size_)),
            is_full_(false), 
            is_empty_(true) 
        {}

        /**
         * @brief Destroys the ring buffer.
         */
        virtual ~RingBuffer() = default;
        
        /**
         * @brief Writes data into buffer by copying it.
         *
         * If the buffer is full, the oldest element is overwritten.
         *
         * @param data: Data to be written into buffer.
         */
        auto write(const T &data) -> void {
            if (is_full_) {
                read_ptr_ = (read_ptr_ + 1) % buffer_size_;
                is_full_ = false;
            } else {
                ++size_;
            }
            buffer_[write_ptr_] = data;

            write_ptr_ = (write_ptr_ + 1) % buffer_size_;
            is_full_ = (write_ptr_ == read_ptr_);
            is_empty_ = false;
        }

        /**
         * @brief Writes data into buffer by moving it.
         *
         * If the buffer is full, the oldest element is overwritten.
         *
         * @param data: Data to be moved into buffer.
         */
        auto write(T &&data) -> void {
            if (is_full_) {
                read_ptr_ = (read_ptr_ + 1) % buffer_size_;
                is_full_ = false;
            } else {
                ++size_;
            }
            buffer_[write_ptr_] = std::move(data);

            write_ptr_ = (write_ptr_ + 1) % buffer_size_;
            is_full_ = (write_ptr_ == read_ptr_);
            is_empty_ = false;
        }

        /**
         * @brief Reads oldest data from buffer.
         *
         * If the buffer is empty, no value is returned.
         *
         * @return std::optional<T>: Oldest stored element or std::nullopt when
         * buffer is empty.
         */
        [[nodiscard]] auto read() -> std::optional<T> {
            if (is_empty_) return std::nullopt;

            auto data = std::move(buffer_[read_ptr_]);
            read_ptr_ = (read_ptr_ + 1) % buffer_size_;
            --size_;
            is_full_ = false;
            is_empty_ = (read_ptr_ == write_ptr_);

            return data;
        }

        /**
         * @brief Checks whether buffer has no elements.
         *
         * @return bool: True if buffer is empty, otherwise false.
         */
        [[nodiscard]] virtual auto isEmpty() const noexcept -> bool {
            return is_empty_;
        }

        /**
         * @brief Checks whether buffer has reached its capacity.
         *
         * @return bool: True if buffer is full, otherwise false.
         */
        [[nodiscard]] virtual auto isFull() const noexcept -> bool {
            return is_full_;
        }

        /**
         * @brief Returns current number of elements stored in buffer.
         *
         * @return std::size_t: Current buffer size.
         */
        [[nodiscard]] virtual auto size() const noexcept -> std::size_t {
            return size_;
        }
    };

    template <typename T> 
    class SafeRingBuffer : public RingBuffer<T> {
        using cond_v_t = std::condition_variable;

    private:
        mutable std::mutex buf_mutex_;
        // Condition variables for signaling state of the queue
        cond_v_t not_full_;
        cond_v_t not_empty_;

        bool is_closed_;

    public:
        /**
         * @brief Creates a thread-safe ring buffer with fixed capacity.
         *
         * @param buf_size: Maximum number of elements stored by the buffer.
         */
        SafeRingBuffer(std::size_t buf_size) : is_closed_(false), RingBuffer<T>(std::move(buf_size)) {}

        /**
         * @brief Destroys the thread-safe ring buffer.
         */
        ~SafeRingBuffer() override {}

        /**
         * @brief Sets new value to access flag.
         *
         * Broadcasts the new state of the buffer to all threads waiting for
         * data or free space.
         *
         * @param is_closed: New closed state of the buffer.
         */
        auto setAccessFlag(const bool is_closed) noexcept -> void {
            {
                std::lock_guard<std::mutex> guard(buf_mutex_);
                is_closed_ = is_closed;
            }
            not_full_.notify_all();
            not_empty_.notify_all();
        }

        /**
         * @brief Writes data into buffer and notifies one waiting reader.
         *
         * This function does not wait for free space. If the base ring buffer
         * is full, the oldest element may be overwritten.
         *
         * @param data: Data to be written into buffer.
         */
        auto safeWrite(const T &data) noexcept -> void {
            {
                std::lock_guard<std::mutex> guard(buf_mutex_);
                RingBuffer<T>::write(data);
            }
            not_empty_.notify_one();
        }

        /**
         * @brief Writes data into buffer. Waits until free space is available.
         *
         * This function does not overwrite data. If the buffer is full, the
         * calling thread waits until a reader removes an element or the buffer
         * is closed.
         *
         * @param data: Data to be written into buffer.
         *
         * @return uint8_t: Status of the function.
         * * 0x00 - Buffer is closed.
         * * 0x01 - Success.
         */
        auto safeWaitWrite(const T &data) noexcept -> uint8_t {
            std::unique_lock<std::mutex> uq_guard(buf_mutex_);
            not_full_.wait(uq_guard, [this]() {
                 return is_closed_ || !RingBuffer<T>::isFull(); 
            });

            if (is_closed_) return 0x00;

            RingBuffer<T>::write(data);
            uq_guard.unlock();
            not_empty_.notify_one();

            return 0x01;
        }
        
        /**
         * @brief Writes data into buffer. Waits until free space is available
         * or timeout happens.
         *
         * This function does not overwrite data. If the buffer is full, the
         * calling thread waits until a reader removes an element, the buffer is
         * closed, or the timeout expires.
         *
         * @param data: Data to be written into buffer.
         * @param timeout: Timeout after which thread abandons waiting.
         * 
         * @return uint8_t: Status of the function.
         * * 0x00 - Timeout expired or buffer is closed.
         * * 0x01 - Success.
         */ 
        auto safeWaitForWrite(const T &data, std::chrono::milliseconds &&timeout) noexcept -> uint8_t {
            std::unique_lock<std::mutex> uq_guard(buf_mutex_);

            auto is_ready = not_full_.wait_for(uq_guard, timeout, [this]() { 
                return is_closed_ || (RingBuffer<T>::size_ < RingBuffer<T>::buffer_size_); 
            });

            if (!is_ready || is_closed_) return 0x00;

            RingBuffer<T>::write(data);
            uq_guard.unlock();
            not_empty_.notify_one();

            return 0x01;
        }
       
        /**
         * @brief Reads data from buffer and notifies one waiting writer.
         *
         * This function does not wait for data to appear. The caller must pass
         * a valid pointer where the read element will be stored.
         *
         * @param placeholder: Destination pointer for the read value.
         *
         * @return uint8_t: Status of the function.
         * * 0x01 - Success.
         */
        auto safeRead(T* placeholder) noexcept -> uint8_t {
            uint8_t res = 0x01;
            {
                std::lock_guard<std::mutex> guard(buf_mutex_);
                auto data = RingBuffer<T>::read();
                if (data.has_value()) {
                    *placeholder = std::move(data.value());
                } else {
                    res = 0x02;
                }
            } 
            if (res == 0x01) not_full_.notify_one();
            return res;
        }

        /**
         * @brief Reads data from buffer. Waits until data is available.
         *
         * If the buffer is empty, the calling thread waits until a writer adds
         * an element or the buffer is closed.
         *
         * @param placeholder: Destination pointer for the read value.
         *
         * @return uint8_t: Status of the function.
         * * 0x00 - Buffer is closed.
         * * 0x01 - Success.
         */
        auto safeWaitRead(T* placeholder) noexcept -> uint8_t {
            std::unique_lock<std::mutex> uq_guard(buf_mutex_);

            not_empty_.wait(uq_guard, [this]() {
                return is_closed_ || RingBuffer<T>::size_ > 0; 
            });

            
            if (is_closed_) { return 0x00; }
            
            *placeholder = RingBuffer<T>::read().value();
            uq_guard.unlock();
            not_full_.notify_one();
            return 0x01;
        }

        /**
         * @brief Reads data from buffer. Waits until data is available or
         * timeout happens.
         *
         * If the buffer is empty, the calling thread waits until a writer adds
         * an element, the buffer is closed, or the timeout expires.
         *
         * @param placeholder: Destination pointer for the read value.
         * @param timeout: Timeout after which thread abandons waiting.
         *
         * @return uint8_t: Status of the function.
         * * 0x00 - Timeout expired or buffer is closed or internal inconsistency.
         * * 0x01 - Success.
         */
        auto safeWaitForRead(T* placeholder, std::chrono::milliseconds&& timeout) noexcept -> uint8_t {
            std::unique_lock<std::mutex> uq_guard(buf_mutex_);

            auto is_ready = not_empty_.wait_for(uq_guard, timeout, [this]() {
                return is_closed_ || RingBuffer<T>::size_ > 0;
            });

            if (!is_ready || is_closed_) { return 0x00; }

            // raw data there should not be std::nullopt but 
            // even though we can check 
            std::optional<T> raw_data = RingBuffer<T>::read();
            if (!raw_data.has_value()) return 0x00;
            *placeholder = std::move(raw_data.value());

            uq_guard.unlock();
            not_full_.notify_one();
            return 0x01;
        }

        /**
         * @brief Checks whether buffer has no elements.
         *
         * @return bool: True if buffer is empty, otherwise false.
         */
        [[nodiscard]] auto isEmpty() const noexcept -> bool override {
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::is_empty_;
        }

        /**
         * @brief Checks whether buffer has reached its capacity.
         *
         * @return bool: True if buffer is full, otherwise false.
         */
        [[nodiscard]] auto isFull() const noexcept -> bool override {
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::is_full_;
        }

        /**
         * @brief Returns current number of elements stored in buffer.
         *
         * @return std::size_t: Current buffer size.
         */
        [[nodiscard]] auto size() const noexcept -> std::size_t override {
            std::lock_guard<std::mutex> guard(buf_mutex_);
            return RingBuffer<T>::size_;
        }
    };

} 
