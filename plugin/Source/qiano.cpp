#include <cassert>
#include <cmath>
#include <cstddef>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include "reverb.h"
#include "utils.h"
#include "qiano.h"
#include <string.h>

#define HSTRING 1

// The dwgs uses velocity waves dy/dt,  which reflect with -1
// These wave directly interacts with the hammer
// which has incoming string impedance Z = sqrt(T mu)
// velocity waves are converted to slope waves as
// s = dy/dx = dy/dt / sqrt(T/mu)
// Total velocity = sTop + sBottom
// Total slope = sTop - sBottom

//XXX delay clear opt

float TUNE[3][3] = {
    {1.0f, 0.0f, 0.0f },
    {0.9997f, 1.0003f, 0.0f},
    { 1.0001f, 1.0003f, 0.9996f }
};


Param params[NumParams] =
{
    { pYoungsModulus, "E", "GPa"},
    { pStringDensity, "rho", "kg / m^3" },
    { pHammerMass, "m", "kg" },
    { pStringTension, "T", "kg m / s^2" },
    { pStringLength, "L", "m" },
    { pStringRadius, "r", "m" },
    { pHammerCompliance, "p", "" },
    { pHammerSpringConstant, "K", "kg / s^2" },
    { pHammerHysteresis, "alpha", "" },
    { pBridgeImpedance, "Zb", "" },
    { pBridgeHorizontalImpedance, "ZbH", "" },
    { pVerticalHorizontalImpedance, "Zvh", "" },
    { pHammerPosition, "pos", "" },
    { pSoundboardSize, "size", "" },
    { pStringDecay, "c1", "" },
    { pStringLopass, "c3", "" },
    { pDampedStringDecay, "d1", "" },
    { pDampedStringLopass, "d3", "" },
    { pSoundboardDecay, "s1", "" },
    { pSoundboardLopass, "s3", "" },
    { pLongitudinalGamma, "gammaL", "" },
    { pLongitudinalGammaQuadratic, "gammaL2", "" },
    { pLongitudinalGammaDamped, "gammaLDamped", "" },
    { pLongitudinalGammaQuadraticDamped, "gammaL2Damped", "" },
    { pLongitudinalMix, "lmix", "" },
    { pLongitudinalTransverseMix, "ltmix", "" },
    { pVolume, "volume", "" },
    { pMaxVelocity, "maxv", "m/s" },
    { pStringDetuning, "detune", "%" },
    { pBridgeMass, "mb", "kg" },
    { pBridgeSpring, "kb", "kg/s^2" },
    { pDwgs4, "dwgs4", "" },
    { pDownsample, "downsample", "" },
    { pLongModes, "longmodes", "" }
};

int getParameterIndex (const char *key)
{
    for (int i = 0; i < NumParams; i++)
        if (! strcmp (key,params[i].name))
            return i;

    return -1;
}

double PianoNote::freqTable[NUM_NOTES];

void PianoNote::fillFrequencyTable()
{
    double NOTE_UP_SCALAR = pow (2.0, 1.0 / 12.0);
    double A = 6.875;	// A
    A *= NOTE_UP_SCALAR;	// A#
    A *= NOTE_UP_SCALAR;	// B
    A *= NOTE_UP_SCALAR;	// C, frequency of midi note 0

    for (int i = 0; (i < NUM_NOTES); i++)	// 128 midi notes
    {
        freqTable[i] = A;
        A *= NOTE_UP_SCALAR;
    }
}

float PianoNote::goUp()
{
    while (upSampleDelayNeeded)
    {
        goUpDelayed();
        upSampleDelayNeeded--;
    }

    return goUpDelayed();
}

float PianoNote::goUpDelayed()
{
    if (tUp == 0)
    {
        if (downsample == 2)
        {
            alignas(32) float in[8];
            for (int i = 0; i < 8; i += 2) 
            {
                in[i] = goDown();
                in[i + 1] = 0;
            }
            vec8 out8 = upSampleFilter.filter8 (simde_mm256_load_ps(in));
            simde_mm256_store_ps (outUp, out8);
        }
        else
        {
            // no downsample here
            for (int i = 0; i < 8; i++) 
            {
                outUp[i] = goDown();
            }
        }
    }

    float out = outUp[tUp];
    tUp = (tUp + 1) & 7;
    return out;
}

float PianoNote::goDown()
{
    while(downSampleDelayNeeded)
    {
        goDownDelayed();
        downSampleDelayNeeded--;
    }
    return goDownDelayed();
}

float PianoNote::goDownDelayed()
{
    if (tDown == 0)
    {
        alignas(32) float in[8];
        if(piano->USE_DWGS4 && hammer.isEscaped())
        {
            auto vec = go4();
            simde_mm_store_ps(in, vec);
            vec = go4();
            simde_mm_store_ps(in + 4, vec);
        }
        else
        {
            for(int i = 0; i < 8; i++)
            {
                in[i] = go();
            }
        }
        vec8 out8 = downSampleFilter.filter8 (simde_mm256_load_ps(in));
        simde_mm256_store_ps (outDown, out8);
    }

    float out = outDown[tDown];
    tDown = (tDown + upsample) & 7;

    return out;
}


/* The note's string outputs sum to give a load at the bridge
 The bridge is treated seperately for each note,
 the sum of all incoming waves at the bridge becomes the
 load for the soundobard.
 The magnitude of the tarnsverse oscillations is set by K
 The output of the soundboard reflects back into the bridge junction,
 and the soundboard output is also the output of the qiano
 */

float PianoNote::go()
{
    float output;
    auto& v = piano->vals;

    float longForce = 0;

    while(tLong <= 0)
    {
        float vstringT0 = 0.0;
        float vstringT1 = 0.0;

        for(int k = 0; k < nstrings; k++)
        {
            vstringT0 += stringT[k].input_velocity();
            vstringT1 += stringT[k].next_input_velocity();
        }
        float vin0 = vstringT0 * nstringsi;
        float vin1 = vstringT1 * nstringsi;
        float hload = hammer.load(vin0, vin1) * Z2i;

        float sbloadT = 0.0;
        float sbloadHT = 0.0;

        for(int k=0;k<nstrings;k++) {
            sbloadT += stringT[k].go_string();
#ifdef HSTRING
            sbloadHT += stringHT[k].go_string();
#endif
        }

        float in[2] = {sbloadT, sbloadHT};
        float out[2];
        bridge.filter(in, out);

        float tranForce = 0.0f;
        float tranForceH = 0.0f;
        float longTranForce = 0.0f;
        for(int k=0;k<nstrings;k++) {
            tranForce += stringT[k].go_soundboard(hload,out[0]);
#ifdef HSTRING
            tranForceH += stringHT[k].go_soundboard(0,out[1]);
#endif
            longTranForce += stringT[k].longTran();
        }

        //longTranForce = 0;
        longTranForces[tTran] = longTranForce;
        tranForces[tTran] = tranForce;
        tranForcesH[tTran] = tranForceH;
        tTran = (tTran + 1)%TranBufferSize;

        if(tLong >= 0)
        {
            longForce = stringT[0].tran2long(longDelay);
        }
        tLong++;
    }
    tLong = 0;

    //longForce = 0;

    output = (tranForces[tTranRead] + tranForcesH[tTranRead]) * tranBridgeForce + /*longHP.filter*/(longTranForces[tTranRead] * longTranBridgeForce * v[pLongitudinalTransverseMix] + longForce * longBridgeForce * v[pLongitudinalMix]);

    //cout << output << "\n";
    float delayed = outputdelay.goDelay(output);
    energy = energy + output*output - delayed*delayed;
    if(energy>maxEnergy)
        maxEnergy = energy;

    tTranRead = (tTranRead + 1) % TranBufferSize;

    return output;
}

vec4 PianoNote::go4()
{
    auto& v = piano->vals;

    if(!bInit4) {
        for(int k=0;k<nstrings;k++) {
            // stringT[k]->init_string4();
            stringT[k].init_string4();
#ifdef HSTRING
            // stringHT[k]->init_string4();
            stringHT[k].init_string4();
#endif
        }
        bInit4 = true;
    }

    vec4 sbloadT = {0};
    vec4 sbloadHT = {0};

    for(int k=0;k<nstrings;k++)
	{
        sbloadT = simde_mm_add_ps (sbloadT, stringT[k].go_string4());
#ifdef HSTRING
        sbloadHT = simde_mm_add_ps (sbloadHT, stringHT[k].go_string4());
#endif
    }

    vec4 in[2] = {sbloadT, sbloadHT};
    vec4 out[2];
    bridge.filter4(in, out);

    vec4 tranForce = {0};
    vec4 tranForceH = {0};
    vec4 longTranForce = {0};
	
    for(int k=0;k<nstrings;k++)
	{
        tranForce = simde_mm_add_ps (tranForce, stringT[k].go_soundboard4(out[0]));
#ifdef HSTRING
        tranForceH = simde_mm_add_ps (tranForceH, stringHT[k].go_soundboard4(out[1]));
#endif
        longTranForce = simde_mm_add_ps (longTranForce, stringT[k].longTran4());
    }

    simde_mm_store_ps (longTranForces + tTran, longTranForce);
    simde_mm_store_ps (tranForces + tTran, tranForce);
    simde_mm_store_ps (tranForcesH + tTran, tranForceH);
    tTran = (tTran + 4)%TranBufferSize;
    
    vec4 longForce = stringT[0].tran2long4 (longDelay);
    //vec4 longForce = {0};

    float longMix = v[pLongitudinalMix];
    auto s1 = simde_mm_add_ps (simde_mm_load_ps(tranForces + tTranRead), simde_mm_load_ps(tranForcesH + tTranRead));
    auto s2 = simde_mm_add_ps (simde_mm_mul_ps (simde_mm_load_ps(longTranForces + tTranRead), simde_mm_broadcast_ss (&longTranBridgeForce)), simde_mm_mul_ps (longForce, simde_mm_broadcast_ss (&longBridgeForce)));

    vec4 output = simde_mm_add_ps (simde_mm_mul_ps (s1, simde_mm_broadcast_ss(&tranBridgeForce)), simde_mm_mul_ps (s2, simde_mm_broadcast_ss (&longMix)));

    vec4 delayed = outputdelay.goDelay4(output);
    energy = energy + sum4 (simde_mm_sub_ps (simde_mm_mul_ps (output, output), simde_mm_mul_ps (delayed, delayed)));
    if(energy>maxEnergy)
        maxEnergy = energy;

    tTranRead = (tTranRead + 4)%TranBufferSize;

    return output;
}

void Piano::init (float Fs_, int blockSize_)
{
    Fs = Fs_;
    blockSize = blockSize_;
    PianoNote::fillFrequencyTable();

    for (int k = PIANO_MIN_NOTE; k <= PIANO_MAX_NOTE; k++)
    {
        noteArray[k] = std::make_unique<PianoNote>(k, int (Fs), this);
    }

    input.resize(blockSize);

#if FDN_REVERB
    soundboard = std::make_unique<Reverb>(Fs);
    soundboard->set (vals[pSoundboardSize], vals[pSoundboardDecay], vals[pSoundboardLopass]);
#else
    soundboard = std::make_unique<ConvolveReverb<revSize>>(blockSize);
#endif

    if (auxThread)
    {
        auxThread->signalThreadShouldExit();
    }
    auxThreadData.size = 0;
    auxThreadData.waiter.release();
    if (auxThread)
    {
        auxThread->waitForThreadToExit(1000);
    }
    auxThreadData.buffer.resize(blockSize);
    auxThreadData.complete = false;

    auxThread = std::make_unique<RenderingThread>(*this);
    auxThread->startRealtimeThread(juce::Thread::RealtimeOptions{});
}

Piano::Piano()
{
    for (int k = PIANO_MIN_NOTE; k <= PIANO_MAX_NOTE; k++)
        noteArray[k] = nullptr;

    // input = nullptr;
    soundboard = nullptr;
}

PianoNote::PianoNote (int note_, int Fs_, Piano* piano_)
{
    Fs = Fs_;
    note = note_;
    f = float (freqTable[note]);
    piano = piano_;

    if (note<31)
        nstrings = 1;
    else if (note<41)
        nstrings = 2;
    else
        nstrings = 3;

    //nstrings = 1;
    nstringsi = 1.0f / (float)nstrings;

    outputdelay.setDelay(Fs);

    longDelay = 8;
    tUp = 0;
    tDown = 0;
    tTran = 0;
    tLong = -longDelay;
    tTranRead = 0;
    bInit4 = false;
    bActive = false;
}

bool PianoNote::isDone()
{
    //return false;
    float e = maxEnergy * 1e-8f;
    return energy < e;
    return (energy < 1e-8 * maxEnergy);
}

void PianoNote::triggerOn (float velocity, float* tune)
{
    triggerOn_ = true;
    activeSampleAfterOff = 0;
    //logf("note = %d velocity =  %g\n",note,velocity);
    auto& v = piano->vals;
    float f0 = 27.5; //A0 note 21
    float L = 0.14f + 1.4f / (1 + exp (-3.4f + 1.4f * log (f / f0)));
    L = 0.04f + 2.0f / (1.0f + exp (-3.2f + 1.4f * log (f / f0)));
    L *= v[pStringLength];
    //L = .115;
    float p = 2.0f + 1.0f * log (f / f0) / log (4192 / f0);
    p *= v[pHammerCompliance];


    //float m = .06 * (1.0 - 0.9*pow((float)log(f/f0)/log(4192/f0),(float)0.1));
    //m=.018 * (1.0 - .7*pow(log(f/f0)/log(4192/f0),0.8));
    float m = 0.02f * pow (0.5f - log (f / 4192), 0.3f);
    m = 0.013f - 0.005f * log (f / f0) / log (4192 / f0);
    m *= v[pHammerMass];
    //m = .0092;

    //float K,p;
    float K = 40/pow((float).7e-3,(float)p);
    float alpha = 0.1e-4f * log (f / f0) / log (4192 / f0);
    //K *= 1.;

    int N = note - 20;
    //float eps =  0.9894f + 8.8e-5f * N;
    //p = 3.7 + .015 * N;
    //p = 3.7;
    //K = 15.5e3 / pow(1e-3,p) * exp(.059*N) * (1 - eps);
    //K = 183.0 / pow(1e-3,p) * exp(.045*N);
    //float tau = 1e-6 * (2.72 - 0.02 * N + 9e-5 * N * N);
    //alpha = tau / (1 - eps);
    //alpha = 1e-6 * (148 + 1.83 * N - 5.5e-2 * N*N + 8.5e-4 * N*N*N);
    K *= v[pHammerSpringConstant];
    alpha *= v[pHammerHysteresis];


    float r = 0.008f * pow ((float)(3.0f + 1.5f * log (f / f0)), (float)-1.4f);
    r *= v[pStringRadius];

    float S = float (PI) * r * r;
    mu = S*v[pStringDensity];

    T = (2*L*f)*(2*L*f)*mu;
    //T = v[pStringTension];
    //L = sqrt(T/mu) / (2*f);

    float rcore = (r < 0.0006f) ? r : 0.0006f;

    float Score = float (PI) * rcore * rcore;
    float mucore = Score * v[pStringDensity];
    Z = sqrt(T*mu);
    float E = v[pYoungsModulus] * 1e9f;
    float B = float (PI*PI*PI) * E * (rcore*rcore*rcore*rcore)/(4.0f*L*L*T);
    //B *= 5;
    //B = 1e-6;

    //cout << B << "\n";
    //cout << B << " " << r << " " << L << "\n";
    //float vLong = sqrt(E*S/mu);
    float vLong = sqrt(E*S/mu);
    vTran = sqrt(T/mu);
    float longFreq1 = vLong / (2 * L);
    //float Zlong = sqrt(E*S*mu);
    float Zlong = sqrt(E*Score*mucore);

    Z2i = 1.0f / (2 * Z);
    alphasb = (2 * Z) / (Z * nstrings + v[pBridgeImpedance]);

    float Zb = v[pBridgeImpedance];
    float hp = v[pHammerPosition];
    hp = hp / (1 + 0.01f * square (log (f / f0)));
    //hp = .0081/L;


    float mBridge = v[pBridgeMass];
    float kBridge = v[pBridgeSpring];


    //logf("f = %g, r = %g mm, L = %g, T = %g, hpos = %g, hm = %g, Z = %g, K = %g, B = %g, Zb = %g, alpha = %g, p = %g, vTran=%g, vLong=%g, mBridge=%G, kBridge=%g\n",f,1000*r,L,T,hp,m,Z,K,B,Zb,alpha,p,vTran,vLong,mBridge,kBridge);

    float ZbH = v[pBridgeHorizontalImpedance];
    float Zhv = v[pVerticalHorizontalImpedance];
    float khv = kBridge * 0.0f;

    float ES = E*S;
    float gammaL = v[pLongitudinalGamma];
    float gammaL2 = v[pLongitudinalGammaQuadratic];


    if (piano->USE_DWGS4 && bInit4)
    {
        for (int k=0;k<nstrings;k++)
        {
            stringT[k].init_string1();
#ifdef HSTRING
            stringHT[k].init_string1();
#endif
        }
    }
    bInit4 = false;

    int upsampleMin = 1;
    int downsample = (int)lrintf(v[pDownsample]);
    int longmodes = (int)lrintf(v[pLongModes]);

    longmodes = 2;
    /*
     if(note < 45) {
     downsample = 2;
     } else {
     downsample = 1;
     }
     */
    if (downsample == 2)
		longmodes = 2;

    for (int k = 0; k < nstrings; k++)
	{
        float fk;
        if (tune)
            fk = f*(1 + (tune[k]-1) * v[pStringDetuning]);
		else
            fk = f*(1 + (TUNE[nstrings-1][k]-1) * v[pStringDetuning]);
        
        upsampleMin = max(upsampleMin, stringT[k].getMinUpsample(downsample, Fs, fk, hp, B));
    }
    upsample = upsampleMin;


    if(downsample == 2)
	{
        if(upsample > 1)
		{
            upsample /= 2;
            downsample = 1;
        }
		else
		{
            downsample = 2;
        }
    }
	else
	{
        downsample = 1;
    }
    this->downsample = downsample;

    float resample = (float)upsample / (float)downsample;
    bridge.create(resample*Fs,
                  mBridge,kBridge,Zb,
                  mBridge,kBridge,ZbH,
                  Zhv,khv,
                  Z*nstrings,
                  Z);

    int maxDel2 = 1;
    for(int k=0;k<nstrings;k++) {
        float fk;
        if(tune) {
            fk = f*(1 + (tune[k]-1) * v[pStringDetuning]);
        } else {
            fk = f*(1 + (TUNE[nstrings-1][k]-1) * v[pStringDetuning]);
        }

        stringT[k].set(Fs,longmodes,downsample,upsample,fk,v[pStringDecay],v[pStringLopass],B,L,longFreq1,gammaL,gammaL2,hp,Z);
#ifdef HSTRING
        stringHT[k].set(Fs,longmodes,downsample,upsample,fk,v[pStringDecay],v[pStringLopass],B,L,longFreq1,gammaL,gammaL2,hp,Z);
#endif

        maxDel2 = std::max(stringT[k].getDel2(), maxDel2);
    }

    hammer.set(resample*Fs,m,K,p,Z,alpha,maxDel2+128);
    hammer.strike(velocity);
    maxEnergy = 0.0;
    energy = 0.0;


    tranBridgeForce = Z;
    longBridgeForce = Zlong*square(vLong/vTran) / L / (Fs * resample) * nstrings;
    longTranBridgeForce = 0.5f * Zlong * (vLong / vTran) / vTran;

    tranBridgeForce /= resample;
    longBridgeForce /= resample;
    longTranBridgeForce /= resample;

    outputdelay.clear();
    longHP.create (0.15f * downsample, 0.5f);

    // XXX should only create once
    if(downSampleFilter.isCreated())
    {
        upSampleDelayNeeded = 0;
        downSampleDelayNeeded = 0;
    }
    else
    {
        downSampleFilter.create(upsample);
        upSampleFilter.create(downsample);
        upSampleDelayNeeded = upSampleFilter.getDelay();
        downSampleDelayNeeded = downSampleFilter.getDelay() / upsample;
    }

    bActive = true;

    //logf("upsample/downsample  %d %d\n",upsample, downsample);

}

void PianoNote::triggerOff()
{
    activeSampleAfterOff = 0;
    triggerOn_ = false;

    auto& v = piano->vals;
    float gammaLDamped = v[pLongitudinalGammaDamped];
    float gammaL2Damped = v[pLongitudinalGammaQuadraticDamped];
	
    for (int k = 0; k < nstrings; k++)
	{
        stringT[k].damper (v[pDampedStringDecay],v[pDampedStringLopass],gammaLDamped,gammaL2Damped,128);
        stringHT[k].damper (v[pDampedStringDecay],v[pDampedStringLopass],gammaLDamped,gammaL2Damped,128);
    }
}

void PianoNote::deActivate()
{
    bActive = false;
}

bool PianoNote::isActive()
{
    return bActive;
}

PianoNote::~PianoNote()
{
}

Piano::~Piano()
{
    auxThread->signalThreadShouldExit();
    auxThreadData.waiter.release();
    auxThread->waitForThreadToExit(1000);
}


template<>
void Piano::process<true> (std::span<float> block)
{
    // the block is always zero because juce::AudioBuffer::clear called outside
    threadVoiceIndex_ = numActiveVoices;

    auxThreadData.size = block.size();
    auxThreadData.complete = false;
    auxThreadData.waiter.release();

    int idx = threadVoiceIndex_.fetch_sub(1);
    while (idx > 0)
    {
        PianoNote* note = voiceList[idx - 1];
        for (size_t i = 0; i < block.size(); ++i)
        {
            block[i] += note->goUp();
        }
        idx = threadVoiceIndex_.fetch_sub(1);
    }

    while (!auxThreadData.complete) {}
    for (size_t i = 0; i < block.size(); ++i)
    {
        block[i] += auxThreadData.buffer[i];
    }

    for (size_t i = 0; i < block.size(); ++i)
    {
#ifdef FDN_REVERB
        block[i] = vals[pVolume] * soundboard->reverb(block[i]);
#else
        block[i] *= vals[pVolume];
#endif
    }

#if !FDN_REVERB
    soundboard->fft_conv(block.data(), out, block.size());
#endif
}

template<>
void Piano::process<false> (std::span<float> block)
{
    // the block is always zero because juce::AudioBuffer::clear called outside
    for (size_t i = 0; i < numActiveVoices; ++i)
    {
        PianoNote* note = voiceList[i];
        for (size_t i = 0; i < block.size(); ++i)
        {
            block[i] += note->goUp();
        }
    }

    for (size_t i = 0; i < block.size(); ++i)
    {
#ifdef FDN_REVERB
        block[i] = vals[pVolume] * soundboard->reverb(block[i]);
#else
        block[i] *= vals[pVolume];
#endif
    }

#if !FDN_REVERB
    soundboard->fft_conv(block.data(), out, block.size());
#endif
}

void Piano::process (std::span<float> block, juce::MidiBuffer& midi)
{
    int delta = 0;
    float maxSamples = vals[pMaxTime] * Fs;
    for (size_t i = 0; i < numActiveVoices;)
    {
        PianoNote* v = voiceList[i];
        ++i;
        if (!v->triggerOn_)
        {
            // force stop when a note continue too long
            // the long force ends with a DC offset
            // cause the energy condition can not stop the note
            if (v->activeSampleAfterOff > maxSamples)
            {
                v->deActivate();
                --i;
                std::swap(voiceList[i], voiceList[--numActiveVoices]);
            }
            v->activeSampleAfterOff += block.size();
        }
        if (v->isDone())
        {
            v->deActivate();
            --i;
            std::swap(voiceList[i], voiceList[--numActiveVoices]);
        }
    }

    for (auto meta : midi)
    {
        auto m = meta.getMessage();

        if (! m.isNoteOnOrOff())
            continue;

        size_t nextDelta = static_cast<size_t>(m.getTimeStamp());
        nextDelta = std::min (nextDelta, block.size());
        if (wasMultiThread) {
            process<true> (block.subspan(delta, nextDelta - delta));
        }
        else {
            process<false> (block.subspan(delta, nextDelta - delta));
        }

        if (m.getNoteNumber() >= PIANO_MIN_NOTE && m.getNoteNumber() <= PIANO_MAX_NOTE)
        {
            PianoNote* v = noteArray[m.getNoteNumber()].get();
            if (m.isNoteOn())
            {
                if (! (noteArray[m.getNoteNumber()]->isActive()))
                {
                    voiceList[numActiveVoices++] = v;
                }

                float velocity = vals[pMaxVelocity] * pow ((float)(m.getVelocity() / 127.0f), 2.0f);
                v->triggerOn (velocity, nullptr);
            }
            else
            {
                v->triggerOff();
            }
        }

        delta = nextDelta;
    }

    if (wasMultiThread) {
        process<true> (block.subspan(delta));
    }
    else {
        process<false> (block.subspan(delta));
    }
}

void Piano::triggerOn (int note, float velocity, float* tune)
{
    PianoNote* v = noteArray[note].get();
    v->triggerOn (velocity,tune);
}

//-----------------------------------------------------------------------------------------
void Piano::setParameter (int32_t index, float value)
{
    Value &p = vals[index];
    p.f = value;

    switch (index)
    {
        case pYoungsModulus:
            p.v = 200 * exp (4.0f * (value - 0.5f));
            break;
        case pStringDensity:
            p.v = 7850 * exp (4.0f * (value - 0.5f));
            break;
        case pHammerMass:
            p.v = exp (4.0f * (value - 0.5f));
            break;
        case pStringTension:
            p.v = 800.0f * exp (3.0f * (value - 0.5f));
            break;
        case pStringLength:
            p.v = exp (2.0f * (value - 0.25f));
            break;
        case pStringRadius:
            p.v = exp (2.0f * (value - 0.25f));
            break;
        case pHammerCompliance:
            p.v = 2.0f * value;
            break;
        case pHammerSpringConstant:
            p.v = (2.0f * value);
            break;
        case pHammerHysteresis:
            p.v = exp (4.0f * (value - 0.5f));
            break;
        case pBridgeImpedance:
            p.v = 8000.0f * exp (12.0f * (value - 0.5f));
            break;
        case pBridgeHorizontalImpedance:
            p.v = 60000.0f * exp (12.0f * (value - 0.5f));
            break;
        case pVerticalHorizontalImpedance:
            p.v = 400.0f * exp (12.0f * (value - 0.5f));
            break;
        case pHammerPosition:
            p.v = 0.05f + value * 0.15f;
            break;
        case pSoundboardSize:
            if (p.v != value)
            {
                p.v = value;
#if FDN_REVERB
                soundboard->set (vals[pSoundboardSize], vals[pSoundboardDecay], vals[pSoundboardLopass]);
#endif
            }
            break;
        case pStringDecay:
            p.v = 0.25f * exp (6.0f * (value - 0.25f));
            break;
        case pStringLopass:
            p.v = 5.85f * exp (6.0f * (value - 0.5f));
            break;
        case pDampedStringDecay:
            p.v = 8.0f * exp (6.0f * (value - 0.5f));
            break;
        case pDampedStringLopass:
            p.v = 25.0f * exp (6.0f * (value - 0.5f));
            break;
        case pSoundboardDecay:
        {
            auto n = 20.0f * exp (4.0f * (value - 0.5f));
            if (std::abs (n - p.v) > 0.0001f)
            {
                p.v = n;
#if FDN_REVERB
                soundboard->set (vals[pSoundboardSize], vals[pSoundboardDecay], vals[pSoundboardLopass]);
#endif
            }
            break;
        }
        case pSoundboardLopass:
        {
            auto n = 20.0f * exp (4.0f * (value - 0.5f));
            if (std::abs (n - p.v) > 0.0001f)
            {
                p.v = n;
#if FDN_REVERB
                soundboard->set (vals[pSoundboardSize], vals[pSoundboardDecay], vals[pSoundboardLopass]);
#endif
            }
            break;
        }
        case pLongitudinalGamma:
            p.v = 1e-2f * exp (10.0f * (value - 0.5f));
            break;
        case pLongitudinalGammaQuadratic:
            p.v = 1.0e-2f * exp (8.0f * (value - 0.5f));
            break;
        case pLongitudinalGammaDamped:
            p.v = 5e-2f * exp (10.0f * (value - 0.5f));
            break;
        case pLongitudinalGammaQuadraticDamped:
            p.v = 3.0e-2f * exp (8.0f * (value - 0.5f));
            break;
        case pLongitudinalMix:
            p.v = (value == 0.0f) ? 0.0f : 1e0f * exp (16.0f * (value - 0.5f));
            break;
        case pLongitudinalTransverseMix:
            p.v = (value == 0.0f) ? 0.0f : 1e0f * exp (16.0f * (value - 0.5f));
            break;
        case pVolume:
            p.v = 5e-3f * exp (8.0f * (value - 0.5f));
            break;
        case pMaxVelocity:
            p.v = 10 * exp (8.0f * (value - 0.5f));
            break;
        case pStringDetuning:
            p.v = 1.0f * exp (10.0f * (value - 0.5f));
            break;
        case pBridgeMass:
            p.v = 10.0f * exp (10.0f * (value - 0.5f));
            break;
        case pBridgeSpring:
            p.v = 1e5f * exp (20.0f * (value - 0.5f));
            break;
        case pDwgs4:
            p.v = lrintf (value);
            USE_DWGS4 = int (lrintf (value));
            break;
        case pDownsample:
            p.v = 1 + lrintf(value);
            break;
        case pLongModes:
            p.v = 1 + lrintf(value);
            break;
        case pMaxTime:
            // seconds
            p.v = std::lerp(3.0f, 10.0f, value);
            break;
        case pMultiThread:
        {
            bool multiThread = value > 0.5f;
            if (wasMultiThread != multiThread) {
                if (wasMultiThread) {
                    auxThreadData.size = 0;
                    if (auxThread) {
                        auxThread->signalThreadShouldExit();
                    }
                    auxThreadData.waiter.release();
                    auxThread->waitForThreadToExit(1000);
                }
                else {
                    auxThread = std::make_unique<RenderingThread>(*this);
                    auxThread->startRealtimeThread(juce::Thread::RealtimeOptions{});
                }
                wasMultiThread = multiThread;
            }
        }
            break;
    }
}

//-----------------------------------------------------------------------------------------
float Piano::getParameter (int32_t index)
{
    return vals[index].f;
}
