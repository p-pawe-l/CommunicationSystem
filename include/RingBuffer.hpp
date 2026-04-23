#pragma once 

#include <optional>
#include <utility>
#include <memory>


namespace droning {

    template <typename T>
    class RingBuffer {
    private:
        std::size_t buffer_size_;
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
        ~RingBuffer() = default;

        auto write(const T& data) -> void {
            if (is_full_) {
                read_ptr_ = (read_ptr_ + 1) % buffer_size_;
                is_full_ = false;
            }
            buffer_[write_ptr_] = data;
            
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

            write_ptr_ = (write_ptr_ + 1) % buffer_size_;
            is_full_ = (write_ptr_ == read_ptr_);
            is_empty_ = false;
        } 

        [[nodiscard]] auto read() -> std::optional<T> {
            if (is_empty_) return std::nullopt;

            auto data = buffer_[read_ptr_];
            read_ptr_ = (read_ptr_ + 1) % buffer_size_;
            is_full_ = false;
            is_empty_ = (read_ptr_ == write_ptr_);

            return data;
        }

        [[nodiscard]] auto isEmpty() const -> bool { return is_empty_; }
        [[nodiscard]] auto isFull() const -> bool { return is_full_; }
    };
}