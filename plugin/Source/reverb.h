#ifndef REVERB_H
#define REVERB_H

#include "utils.h"
#include "filter.h"
#include "FFTConvolver.h"
#include "TwoStageFFTConvolver.h"

#include <algorithm>
#include <array>
#include <memory>

using namespace fftconvolver;

inline constexpr int revSize = 65536;

template<int size>
class ConvolveReverb {

    enum {
        delaySize = 2*size,
        resSize = size,
        tailBlockSize = 512
    };
    
    int headBlockSize;
    
public:
    alignas(32) static float res[resSize];
    int k;
    Delay<delaySize> d;
    //TwoStageFFTConvolver fftConvolver;
    FFTConvolver fftConvolver;
    
    
    ConvolveReverb(int blockSize) : k(0) {
        headBlockSize = std::min(blockSize, static_cast<int>(tailBlockSize));
        //fftConvolver.init(headBlockSize, tailBlockSize, res, size);
        fftConvolver.init(headBlockSize, res, size);
    } 
    
    void fft_conv(float *in, float *out, int N) {
        //fprintf(stderr,"%d %d\n",headBlockSize,N); 
        fftConvolver.process(in,out,N);
    }
    
};


inline constexpr int ReverbTaps = 12;
inline constexpr int NumLengths = 23;


class Reverb {
public:
    Reverb(float Fs);
    void set(float size, float c1, float c3);
    float reverb(float in); 
    float probe();
    
protected:
    int getLength(int k);
    static int allLengths[NumLengths];
    int lengths[ReverbTaps];
    float Fs;
    Delay<1024> d[ReverbTaps];
    float o[ReverbTaps];
    float b[ReverbTaps];
    float c[ReverbTaps];
    
    vec4 o4[ReverbTaps];
    vec4 b4[ReverbTaps];
    vec4 c4[ReverbTaps];
    
    float scale;
    
    std::array<Loss, ReverbTaps> decay;
    std::unique_ptr<ConvolveReverb<revSize>> conv;
    float out;
};

#endif
