#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>
#include <algorithm>

template <typename T>
class RawMemory
{

public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity) : buffer_(Allocate(capacity)), capacity_(capacity)
    {
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept
    {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept
    {
        return buffer_;
    }

    T* GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

private:
    static T* Allocate(size_t n)
    {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept
    {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector
{
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }

    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept
    {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept
    {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept
    {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        assert(pos >= begin() && pos <= end());
        
        size_t insert_index = std::distance(cbegin(), pos);

        if(size_ == Capacity())
        {
            size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_capacity);
            new(new_data.GetAddress() + insert_index) T(std::forward<Args>(args)...);

            try
            {
                if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                {
                    std::uninitialized_move(begin(), begin() + insert_index, new_data.GetAddress());
                }
                else
                {
                    std::uninitialized_copy(begin(), begin() + insert_index, new_data.GetAddress());
                }
            }
            catch(...)
            {
                std::destroy_at(pos);
            }

            try
            {
                if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                {
                    std::uninitialized_move_n(begin() + insert_index, size_ - insert_index, new_data.GetAddress() + insert_index + 1);
                }
                else
                {
                    std::uninitialized_copy_n(begin() + insert_index, size_ - insert_index, new_data.GetAddress() + insert_index + 1);
                }
            }
            catch(...)
            {
                std::destroy_n(new_data.GetAddress(), pos - begin() + 1);
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
            ++size_;
        }
        else
        {
            if(pos == end())
            {
                EmplaceBack(std::forward<Args>(args)...);
            }
            else
            {
                T inserting_value_tmp = T(std::forward<Args>(args)...);
                new(data_ + size_) T(std::move(data_[size_ - 1]));
                std::move_backward(begin() + insert_index, begin() + size_ - 1, begin() + size_);
                data_[insert_index] = std::move(inserting_value_tmp);
                ++size_;
            }
        }

        return begin() + insert_index;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/
    {
        assert(pos >= begin() && pos < end());
        
        size_t del_index = std::distance(cbegin(), pos);
        std::move(begin() + del_index + 1, end(), begin() + del_index);
        PopBack();

        return begin() + del_index;
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value)
    {
        return Emplace(pos, std::move(value));
    }

    Vector() = default;

    explicit Vector(size_t size) : data_(size), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) : data_(other.size_), size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }


    void Reserve(size_t new_capacity)
    {
        if(new_capacity <= data_.Capacity())
        {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size)
    {
        if(new_size < size_)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }

        if(new_size > size_)
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }

        size_ = new_size;
    }

    template<class U>
    void PushBack(U&& value)
    {
        if(size_ == Capacity())
        {
            size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_capacity);
            new (new_data + size_) T(std::forward<U>(value));

            if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else
            {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else
        {
            new(data_ + size_) T(std::forward<U>(value));
        }

        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        if(size_ == Capacity())
        {
            size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_capacity);
            new (new_data + size_) T(std::forward<Args>(args)...);

            if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else
            {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else
        {
            new(data_ + size_) T(std::forward<Args>(args)...);
        }

        ++size_;
        return *(data_.GetAddress() + size_ - 1);
    }

    void PopBack()
    {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs)
    {
        if(this != &rhs)
        {
            if(rhs.size_ > data_.Capacity())
            {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else
            {
                if(rhs.size_ < size_)
                {
                    auto* src_data = rhs.data_.GetAddress();
                    auto* dst_data = data_.GetAddress();
                    for(size_t i = 0; i < rhs.size_; ++i)
                    {
                        *(dst_data++) = *src_data;
                        ++src_data;
                    }

                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else
                {
                    auto* src_data = rhs.data_.GetAddress();
                    auto* dst_data = data_.GetAddress();
                    for(size_t i = 0; i < size_; ++i)
                    {
                        *(dst_data++) = *src_data;
                        ++src_data;
                    }

                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }

                size_ = rhs.size_;
            }
        }

        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        if(this != &rhs)
        {
            Swap(rhs);
        }

        return *this;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static void DestroyN(T* buf, size_t n) noexcept
    {
        for(size_t i = 0; i != n; ++i)
        {
            Destroy(buf + i);
        }
    }

    static void CopyConstruct(T* buf, const T& elem)
    {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept
    {
        buf->~T();
    }
};
