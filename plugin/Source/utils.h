#ifndef UTILS_H
#define UTILS_H

#ifndef __has_extension
#define __has_extension(x) 0
#endif
#define vImage_Utilities_h
#define vImage_CVUtilities_h

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "../../modules/simde/x86/avx.h"
#include "../../modules/simde/x86/sse3.h"
#include "../../modules/simde/x86/svml.h"

#define vec4 simde__m128
#define vec8 simde__m256

#ifdef _WIN32
#include <malloc.h>
#define posix_memalign(p, a, s) (((*(p)) = _aligned_malloc((s), (a))), *(p) ? 0 :errno)
#define aligned_free(p) _aligned_free(p)
#else
#define aligned_free(p) free(p)
#endif

inline constexpr double HALFPI = 1.5707963267948966192313216916398;
inline constexpr double PI = 3.1415926535897932384626433832795;
inline constexpr double TWOPI = 6.283185307179586476925286766559;

// Modern C++ aligned memory buffer with RAII
template<typename T, size_t Alignment = 32>
class AlignedBuffer
{
public:
    AlignedBuffer() = default;

    explicit AlignedBuffer(size_t count) { allocate(count); }

    ~AlignedBuffer() { deallocate(); }

    // Non-copyable
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;

    // Movable
    AlignedBuffer(AlignedBuffer&& other) noexcept
        : data_(other.data_), size_(other.size_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept
    {
        if (this != &other)
        {
            deallocate();
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void allocate(size_t count)
    {
        deallocate();
        if (count > 0)
        {
            size_ = count;
#ifdef _WIN32
            data_ = static_cast<T*>(_aligned_malloc(count * sizeof(T), Alignment));
#else
            void* ptr = nullptr;
            if (posix_memalign(&ptr, Alignment, count * sizeof(T)) == 0)
                data_ = static_cast<T*>(ptr);
#endif
        }
    }

    void deallocate()
    {
        if (data_)
        {
#ifdef _WIN32
            _aligned_free(data_);
#else
            free(data_);
#endif
            data_ = nullptr;
            size_ = 0;
        }
    }

    void clear()
    {
        if (data_ && size_ > 0)
            std::memset(data_, 0, size_ * sizeof(T));
    }

    T* get() noexcept { return data_; }
    const T* get() const noexcept { return data_; }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return data_ == nullptr; }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    // Allow implicit conversion to raw pointer for compatibility
    operator T*() noexcept { return data_; }
    operator const T*() const noexcept { return data_; }

private:
    T* data_ = nullptr;
    size_t size_ = 0;
};

std::ostream& operator<<(std::ostream& os, const vec4 &v);

float dot (int N, float *A, float *B);
float sum8 (simde__m256 x);
float sse_dot (int N, float *A, float *B);
float sum4 (vec4 x);
void ms4 (float *x, float *y, float *z, int N);
void diff4 (float *x, float *y, int N);

static inline float square(float x)
{
    return x * x;
}

static inline float cube(float x)
{
    return x * x * x;
}

#endif
