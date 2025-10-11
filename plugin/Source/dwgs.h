#ifndef DWGS_H
#define DWGS_H

#include "filter.h"
#include "AlignedArray.hpp"

class dwgs;

enum {
    DelaySize = 4096,
    nMaxLongModes = 128
};

class dwgs
{
public:
    dwgs();
    ~dwgs();
    
    int getMaxDecimation(int downsample, int upsample, float Fs, float f, float magic);
    int getMinUpsample(int downsample, float Fs, float f, float inpos, float B);
    void set(float Fs, int longmodes, int downsample, int upsample, float f, float c1, float c3, float B, float L, float longFreq1, float gammaL, float gammaL2, float inpos, float Z);
    void damper(float c1, float c3, float gammaL, float gammaL2, int nDamper);
    
    float input_velocity();
    float next_input_velocity();
    float go_string();
    float go_soundboard(float hload, float sbload);
    float longTran();
    float tran2long(int delay);
    void init_string1();
    
    void init_string4();
    vec4 go_string4();
    vec4 go_soundboard4(vec4 sbload);
    vec4 longTran4();
    vec4 tran2long4(int delay);
    int getDel2();
    
    int downsample{};
    int delTab{};
    // float *wave;
    // float *wave0;
    // float *wave1;
    // float *Fl;
    AlignedArray<float, 32> wave;
    AlignedArray<float, 32> wave0;
    AlignedArray<float, 32> wave1;
    AlignedArray<float, 32> Fl;
    
    float F[nMaxLongModes]{};
    vec4 F4[nMaxLongModes]{};
    
    float L{};
    float omega{};
    float f{};
    float inpos{};
    float B{};
    float longFreq1{};
    int nLongModes{};
    int upsample{};
    
    int nDamper{};
    float c1{};
    float c3{};
    float c1M{};
    float c3M{};
    
    Loss loss{};
    Thiran fracDelayTop;
    Thiran fracDelayBottom;
    Thiran hammerDelay;
    ThiranDispersion dispersion[4];
    
    // float *modeTable[nMaxLongModes];
    // float *modeTable4[nMaxLongModes];
    AlignedArray<float, 32> modeTable[nMaxLongModes];
    float fLong[nMaxLongModes]{};
    DWGResonator longModeResonator[nMaxLongModes];
    
    float dDispersion{};
    float dTop{};
    float dHammer{};
    float dBottomAndLoss{};
    
    float a0_0{};
    float a0_1{}, a0_2{}, a0_3{}, a0_4{}, a0_5{};
    float a1_1{}, a1_2{}, a1_3{}, a1_4{}, a1_5{};
    
    vec4 v0_0{};
    vec4 v0_1{}, v0_2{}, v0_3{}, v0_4{}, v0_5{};
    vec4 v1_1{}, v1_2{}, v1_3{}, v1_4{}, v1_5{};
    
    
    int del0{}, del1{}, del2{}, del3{}, del4{}, del5{};
    Delay<DelaySize> d0;
    Delay<DelaySize> d1;
    Delay<DelaySize> d2;
    Delay<DelaySize> d3;
    
    
    int M{};
};

#endif
