#ifndef FILTER_H
#define FILTER_H

#include "utils.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include "AlignedArray.hpp"

enum {
    MaxFilterUpsample = 8
};


class Filter {
public:
    Filter(int n);
    ~Filter();

    void merge(const Filter &f);
    vec4 filter4(vec4 in);
    float filter(float in);
    float phasedelay(float omega);
    void init(int upsample = 1);

    AlignedArray<float, 32> x;
    AlignedArray<float, 32> y;
    AlignedArray<float, 32> b;
    AlignedArray<float, 32> a;

    float *bend{};
    float *aend{};
    float *aend4{};

    float *xc{};
    float *yc{};

    float *xend{};
    float *yend{};

    vec4 a0;
    vec4 a1;
    vec4 a2;
    vec4 a3;

    int upsample{};
    int xskip{};
    int n{};
    int nmax{};
};

class BiquadHP : public Filter 
{
public:
    BiquadHP() : Filter(2) {}
    void create(float omega, float Q);
};

class Thiran : public Filter
{
public:
    Thiran (int n_) : Filter (n_) {}
    void create (float D, int N, int upsample = 1);
};

// nmax = 16 (max upsample is 8)
class ThiranDispersion : public Thiran {
public:
    ThiranDispersion() : Thiran(16) {}
    void create(float B, float f, int M, int downsample = 1, int upsample = 1);
};


class Loss : public Filter
{
public:
    Loss() : Filter(1) {}
    /*
     float filter(float in) {
     float out = b[1] * in + a[0] * y[0];
     y = out;
     return out;
     }
     */

    void create(float f0, float c1, float c3, float upsample = 1) {
        f0 /= upsample;
        c1 /= upsample;
        c3 *= upsample;
        float g = 1.0f - c1 / f0;
        float c = 4.0f * c3 + f0;
        float a1 = (-c + sqrt (c * c - 16.0f * c3 *c3)) / (4.0f * c3);
        b[0] = g*(1+a1);
        b[1] = 0;
        a[0] = -1;
        a[1] = -a1;
        n = 1;
        init(1);
    }
};

template <int size>
class Delay
{
public:
    int di{};
    int d1{};
    int cursor{};

    float* x;
    alignas(32) float x_[size+16];

    enum {
        mask = size-1 };

    Delay(int offset = 0) {
        x = x_ + 8;
        cursor = offset;
        clear();
    }

    void setDelay (int di_)
    {
        di = di_;
        d1 = (cursor+size-di)&(mask);
    }

    void clear()
    {
        memset(x_,0,(size+16)*sizeof(float));
    }

    float probe()
    {
        float y0;
        y0 = x[d1];
        return y0;
    }

    float goDelay(float in)
    {
        float y0;
        x[cursor] = in;
        if(cursor <= 3) {
            x[cursor + size] = in;
        }
        cursor = (cursor + 1) & mask;
        y0 = x[d1];
        d1 = (d1 + 1) & mask;
        return y0;
    }

    vec4 probe4()
    {
        alignas(32) float out[4];
        for (int i = 2; i < 4; i++)
        {
            int d = d1 + i - 2;
            out[i] = x[(d + size) & mask];
        }
        d1 = (d1 + 2) & mask;

        return simde_mm_load_ps (out);
    }

    void backup()
    {
        d1 = (d1 - 1 + size) & mask;
    }

    void backupCursor()
    {
        cursor = (cursor - 1 + size) & mask;
    }

    vec4 goDelay4(vec4 in)
    {
        if (cursor == 0)
            simde_mm_store_ps((x + size), in);

        simde_mm_store_ps((x + cursor), in);
        vec4 y0 = simde_mm_loadu_ps(x + d1);
        d1 = (d1 + 4) & mask;
        cursor = (cursor + 4) & mask;
        return y0;
    }

};


class DWGResonator {
public:
    DWGResonator();
    void create(float omega, float gamma);
    float go(float in);
    vec4 go4(vec4 vin);

    // variables
    float g;
    float c;
    float b1t;

    // state
    float x1;
    float x2;
};

class MSD2Filter {
public:
    MSD2Filter() : f11(4), f12(4), f21(4), f22(4) {}
    void filter(float in[2], float out[2]);
    void filter4(vec4 in[2], vec4 out[2]);

    void create(float Fs,
                float m1, float k1, float R1,
                float m2, float k2, float R2,
                float R12, float k12, float Zn, float Z);
    
    float f11,f12,f21,f22;
};

enum {
    ResampleFilterSize = 64
};

class ResampleFIR {
public:
    ResampleFIR();
    bool isCreated();
    int getDelay();
    void create(int resample);
    vec8 filter8(vec8 in);
    alignas(32) float b[ResampleFilterSize];
    alignas(32) float x[ResampleFilterSize*4+16];
    float *xc;
    float *xend;
    float *bend;
    int xsize;
    bool bInit;
};

#endif
