#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <stdexcept>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        std::swap(rhs.buffer_, buffer_);
        std::swap(rhs.capacity_, capacity_);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& v) : data_(v.size_), size_(v.size_) {
        std::uninitialized_copy_n(v.data_.GetAddress(), size_, data_.GetAddress());
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> copy(rhs);
                Swap(copy);
            }
            else {
                if (rhs.size_ < size_) {
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                    size_ = rhs.size_;
                }
                else {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                    size_ = rhs.size_;
                }
            }
        }
        return *this;
    }

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)) {
        std::swap(other.size_, size_);
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Reserve(rhs.size_);
                Swap(rhs);
            }
            else {
                Swap(rhs);
            }
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(other.size_, size_);
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    void PopBack() /* noexcept */ {
        if (size_ == 0) {
            return;
        }
        Destroy(data_ + size_ - 1);
        size_--;
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> tmp(size_ == 0 ? 1 : size_ * 2);
            new (tmp.GetAddress() + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_nothrow_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, tmp.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, tmp.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            size_++;
            data_ = std::move(tmp);
        }
        else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
            size_++;
        }
        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        int index = pos - begin();
        if (size_ == Capacity()) {
            RawMemory<T> tmp(size_ == 0 ? 1 : size_ * 2);
            new (tmp.GetAddress() + index) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_nothrow_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), index, tmp.GetAddress());
                try {
                    std::uninitialized_move_n(begin() + index, size_ - index, tmp.GetAddress() + index + 1);
                }
                catch (const std::exception&) {
                    DestroyN(tmp.GetAddress(), index);
                }
            }
            else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), index, tmp.GetAddress());
                }
                catch (const std::exception&) {
                    Destroy(tmp.GetAddress() + index);
                }
                try {
                    std::uninitialized_copy_n(begin() + index, size_ - index, tmp.GetAddress() + index + 1);
                }
                catch (const std::exception&) {
                    DestroyN(tmp.GetAddress(), index);
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            size_++;
            data_ = std::move(tmp);
        }
        else {
            if (size_ != 0) {
                T tmp(std::forward<Args>(args)...);
                new (end()) T(std::move(data_[size_]));
                std::move_backward(begin() + index, end() - 1, end());
                data_[index] = std::forward<T>(tmp);
            }
            else {
                new (begin() + index) T(std::forward<Args>(args)...);
            }
            size_++;
        }
        return begin() + index;
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        int index = pos - begin();
        std::move(begin() + index + 1, end(), begin() + index);
        Destroy(end() - 1);
        size_--;
        return begin() + index;
    }
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};