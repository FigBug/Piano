#ifndef UTILS_H
#define UTILS_H

#ifndef __has_extension
#define __has_extension(x) 0
#endif
#define vImage_Utilities_h
#define vImage_CVUtilities_h

#include "../../modules/simde/x86/avx.h"
#include "../../modules/simde/x86/sse3.h"
#include "../../modules/simde/x86/svml.h"

typedef simde__m128 vec4;
typedef simde__m256 vec8;

#define HALFPI 1.5707963267948966192313216916398
#define PI 3.1415926535897932384626433832795
#define TWOPI 6.283185307179586476925286766559

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
