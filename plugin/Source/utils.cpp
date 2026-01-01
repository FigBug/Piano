#include "utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define DEBUG_OUTPUT(msg) do { OutputDebugStringA(msg); fprintf(stderr, "%s", msg); fflush(stderr); } while(0)
#else
#define DEBUG_OUTPUT(msg) do { fprintf(stderr, "%s", msg); fflush(stderr); } while(0)
#endif

// Assertion macro for utils - prints pointer values before aborting
#define UTILS_ASSERT(cond, msg, ...) do { \
    if (!(cond)) { \
        char _buf[512]; \
        snprintf(_buf, sizeof(_buf), "UTILS ASSERT FAILED: " msg "\n  File: %s, Line: %d\n  Condition: %s\n", \
                ##__VA_ARGS__, __FILE__, __LINE__, #cond); \
        DEBUG_OUTPUT(_buf); \
        abort(); \
    } \
} while(0)

// Check if pointer looks valid (not null, not small integer, not -1/high invalid)
#define UTILS_CHECK_PTR(ptr, name) do { \
    volatile uintptr_t p = (uintptr_t)(ptr); \
    volatile bool bad = (p == 0 || p < 0x10000 || p > 0x00007FFFFFFFFFFF); \
    if (bad) { \
        char _buf[512]; \
        snprintf(_buf, sizeof(_buf), "UTILS BAD PTR: %s = %p (0x%llx) - invalid pointer!\n  File: %s, Line: %d\n", \
                name, (void*)(ptr), (unsigned long long)p, __FILE__, __LINE__); \
        DEBUG_OUTPUT(_buf); \
        __debugbreak(); \
    } \
} while(0)

float dot(int N, float *A, float *B) {
    float dot = 0;
    for(int i=0; i<N; i++) {
        dot += A[i] * B[i];
    }
    return dot;
}

float sum8(simde__m256 x) {
    float sumAVX = 0;
    simde__m256 hsum = simde_mm256_hadd_ps(x, x);
    hsum = simde_mm256_add_ps (hsum, simde_mm256_permute2f128_ps (hsum, hsum, 0x1));
    simde_mm_store_ss (&sumAVX, simde_mm_hadd_ps (simde_mm256_castps256_ps128 (hsum), simde_mm256_castps256_ps128 (hsum)));

    return sumAVX;
}

float sse_dot(int N, float* A, float* B) {
    vec8 temp0 = {0};
    vec8 temp1 = {0};
    vec8 temp2 = {0};
    vec8 temp3 = {0};

    int Ai = 0;
    int Bi = 0;

    vec8 Bv0;
    vec8 Bv1;
    vec8 Bv2;
    vec8 Bv3;

    int N32 = N / (4 * 8);
    N -= 4*8*N32;
    int N8 =  N / 8;
    N -= 8*N8;

    for(int i = 0; i < N32; i++) {
        Bv0 = simde_mm256_loadu_ps(B + Bi);
        Bv1 = simde_mm256_loadu_ps(B + Bi + 8);
        Bv2 = simde_mm256_loadu_ps(B + Bi + 16);
        Bv3 = simde_mm256_loadu_ps(B + Bi + 24);
        temp0 = simde_mm256_fmadd_ps(simde_mm256_loadu_ps(A + Ai), Bv0, temp0);
        temp1 = simde_mm256_fmadd_ps(simde_mm256_loadu_ps(A + Ai + 8), Bv1, temp1);
        temp2 = simde_mm256_fmadd_ps(simde_mm256_loadu_ps(A + Ai + 16), Bv2, temp2);
        temp3 = simde_mm256_fmadd_ps(simde_mm256_loadu_ps(A + Ai + 24), Bv3, temp3);

        Ai += 32;
        Bi += 32;
    }

    for(int i = 0; i < N8; i++) {
        temp0 = simde_mm256_fmadd_ps(simde_mm256_loadu_ps(A + Ai), simde_mm256_loadu_ps(B + Bi), temp0);
        Ai += 8;
        Bi += 8;
    }

    float dotval = 0;
    for(int i = 0; i < N; i++) {
        dotval += A[Ai + i] * B[Bi + i];
    }

    if (N32 || N8)
    {
        temp0 = simde_mm256_add_ps (temp0, simde_mm256_add_ps (temp1, simde_mm256_add_ps (temp2, temp3)));
        dotval += sum8(temp0);
    }

    return dotval;
}

float sum4 (vec4 x)
{
    x = simde_mm_hadd_ps (x, x);
    x = simde_mm_hadd_ps (x, x);
    return simde_mm_cvtss_f32 (x);
}

void ms4(float *x, float *y, float *z, int N)
{
    int N4 = N >> 2;
    int iterations = N4 + 1;

    int xi = 0;
    int zi = 0;
    y += N - 4;

    for (int i = 0; i < iterations; i++)
    {
        vec4 w = simde_mm_loadu_ps(y);
        vec4 xv = simde_mm_loadu_ps(x + xi);
        vec4 v = simde_mm_sub_ps(simde_mm_shuffle_ps(w, w, SIMDE_MM_SHUFFLE(0, 1, 2, 3)), xv);
        simde_mm_storeu_ps(z + zi, simde_mm_mul_ps(v, v));
        xi += 4;
        zi += 4;
        y -= 4;
    }
}

// 0 0 0 x ...
void diff4 (float *x, float *y, int N)
{
    vec4 v0 = simde_mm_loadu_ps(x);
    int xi = 4;
    int yi = 4;
    int N4 = N >> 2;

    for (int i = 1; i < N4; i++)
    {
        vec4 x4 = simde_mm_loadu_ps(x + xi);
        vec4 result = simde_mm_sub_ps(simde_mm_shuffle_ps(v0, x4, SIMDE_MM_SHUFFLE(1, 0, 3, 2)), v0);
        simde_mm_storeu_ps(y + yi, result);
        v0 = x4;
        xi += 4;
        yi += 4;
    }
    y[3] = 2 * (x[1] - x[0]);
    y[N] = (x[N-2] - x[N-4]);
    y[N+1] = (x[N-1] - x[N-3]);
    y[N+2] = 2 * (x[N-1] - x[N-2]);
}

std::ostream& operator<<(std::ostream& os, const vec4 &v)
{
    alignas(16) float tempf[4];
    simde_mm_store_ps(tempf, v);
    for(int i=0; i<4; i++) {
        os << tempf[i] << " ";
    }
    return os;
}
