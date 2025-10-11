#pragma once
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <memory>

template<class T, size_t aligenment>
class AlignedArray {
public:
    ~AlignedArray() {
        Free();
    }
    void Free() {
        if (ptr_) {
#ifdef _WIN32
            _aligned_free(ptr_);
#else
            free(ptr_);
#endif
            ptr_ = nullptr;
            size_ = 0;
        }
    }

    void Reset(size_t size) {
        Free();
#ifdef _WIN32
        ptr_ = (T*)_aligned_malloc(size * sizeof(T), aligenment);
        if (!ptr_) assert(false);
#else
        auto result = posix_memalign((void**)&ptr_, aligenment, size * sizeof(T));
        if (result != 0) assert(false);
#endif
        std::fill_n(ptr_, size, T{});
        size_ = size;
    }

    T& operator[](size_t i) {
        assert(i < size_);
        return ptr_[i];
    }

    T operator[](size_t i) const {
        assert(i < size_);
        return ptr_[i];
    }

    T* Get() {
        return ptr_;
    }

    const T* Get() const {
        return ptr_;
    }

private:
    T* ptr_{ nullptr };
    size_t size_{ 0 };
};
