#include "utils.h"


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
    
    vec8 *Av = (vec8 *)A;
    vec8 *Bv = (vec8 *)B;

    vec8 Bv0;
    vec8 Bv1;
    vec8 Bv2;
    vec8 Bv3;


    int N32 = N / (4 * 8);
    N -= 4*8*N32;
    int N8 =  N / 8;
    N -= 8*N8;

    for(int i = 0; i < N32; i++) {

        Bv0 = *(Bv);
        Bv1 = *(Bv+1);
        Bv2 = *(Bv+2);
        Bv3 = *(Bv+3);
        temp0 = simde_mm256_fmadd_ps(*Av,Bv0,temp0);
        temp1 = simde_mm256_fmadd_ps(*(Av+1),Bv1,temp1);
        temp2 = simde_mm256_fmadd_ps(*(Av+2),Bv2,temp2);
        temp3 = simde_mm256_fmadd_ps(*(Av+3),Bv3,temp3);

        Av+=4;
        Bv+=4;
    }

    for(int i = 0; i < N8; i++) {
        temp0 = simde_mm256_fmadd_ps(*Av,*Bv,temp0);
        Av++;
        Bv++;
    }

    A = (float*)Av;
    B = (float*)Bv;

    float dot = 0;
    for(int i=0; i<N; i++) {
        dot += A[i] * B[i];
    }
    
    if (N32 || N8) 
    {
        temp0 = simde_mm256_add_ps (temp0, simde_mm256_add_ps (temp1, simde_mm256_add_ps (temp2, temp3)));
        dot += sum8(temp0);
    }

    return dot;
}

float sum4 (vec4 x)
{
    x = simde_mm_hadd_ps (x, x);
    x = simde_mm_hadd_ps (x, x);
    return simde_mm_cvtss_f32 (x);
}

void ms4(float *x, float *y, float *z, int N)
{
    // vec4 *x4 = (vec4*) x;
    // vec4 *z4 = (vec4*) z;
    vec4 x4vec = simde_mm_loadu_ps(x);
    vec4 *x4 = &x4vec;

    vec4 z4vec = simde_mm_loadu_ps(z);
    vec4 *z4 = &z4vec;

    int N4 = N >> 2;
    vec4 *zend = z4 + N4 + 1;
    vec4 v;

    y += N - 4;
    while (z4 < zend)
    {
        vec4 w = simde_mm_loadu_ps(y);
        // v = simde_mm_sub_ps (simde_mm_shuffle_ps (w, w, SIMDE_MM_SHUFFLE(0, 1, 2, 3)), *x4);
        // *z4 = simde_mm_mul_ps(v,v);
        v = simde_mm_sub_ps(simde_mm_shuffle_ps(w, w, SIMDE_MM_SHUFFLE(0, 1, 2, 3)), *x4);
        z4vec = simde_mm_mul_ps(v, v);
        x4++;
        y -= 4;
        z4++;
    }
}

// 0 0 0 x ...
void diff4 (float *x, float *y, int N)
{
    vec4 v0;
    vec4 v1;
    vec4 *x4 = (vec4*) x;
    vec4 *y4 = (vec4*) y;
    v0 = *x4;
    x4++;
    y4++;
    int N4 = N >> 2;
    vec4 *yend = y4 + N4 ;
    while (y4 < yend)
    {
        *y4 = simde_mm_sub_ps (simde_mm_shuffle_ps (v0, *x4, SIMDE_MM_SHUFFLE (1,0,3,2)), v0);
        v0 = *x4;
        x4++;
        y4++;
    }
    y[3] = 2 * (x[1] - x[0]);
    y[N] = (x[N-2] - x[N-4]);
    y[N+1] = (x[N-1] - x[N-3]);
    y[N+2] = 2 * (x[N-1] - x[N-2]);
}

std::ostream& operator<<(std::ostream& os, const vec4 &v)
{
    union {
        vec4 tempv;
        float tempf[4];
    };
    tempv = v;
    for(int i=0; i<4; i++) {
        os << tempf[i] << " ";
    }
    return os;
}

