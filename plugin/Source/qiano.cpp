#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "utils.h"
#include "qiano.h"

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
        alignas(32) float in[8];

        for (int i=0; i<8; i++) 
        {
            if (i & 1) 
            {
                if (downsample == 1) 
                    in[i] = goDown();
                else 
                    in[i] = 0;
            } 
            else 
            {
                in[i] = goDown();
            }
        }
        vec8 out8 = upSampleFilter.filter8 (simde_mm256_load_ps(in));
        simde_mm256_store_ps (outUp, out8);
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
        if(piano->USE_DWGS4 && hammer->isEscaped())
        {
            simde_mm_store_ps(in, go4());
            simde_mm_store_ps(in + 4, go4());
        }
        else
        {
            for(int i=0; i<8; i++) {
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
    Value *v = piano->vals;

    float longForce = 0;

    while(tLong <= 0) {
        float vstringT0 = 0.0;
        float vstringT1 = 0.0;

        for(int k=0;k<nstrings;k++) {
            vstringT0 += stringT[k]->input_velocity();
            vstringT1 += stringT[k]->next_input_velocity();
        }
        float vin0 = vstringT0 * nstringsi;
        float vin1 = vstringT1 * nstringsi;
        float hload = hammer->load(vin0,vin1)*Z2i;

        float sbloadT = 0.0;
        float sbloadHT = 0.0;

        for(int k=0;k<nstrings;k++) {
            sbloadT += stringT[k]->go_string();
#ifdef HSTRING
            sbloadHT += stringHT[k]->go_string();
#endif
        }

        float in[2] = {sbloadT, sbloadHT};
        float out[2];
        bridge.filter(in, out);

        float tranForce = 0.0f;
        float tranForceH = 0.0f;
        float longTranForce = 0.0f;
        for(int k=0;k<nstrings;k++) {
            tranForce += stringT[k]->go_soundboard(hload,out[0]);
#ifdef HSTRING
            tranForceH += stringHT[k]->go_soundboard(0,out[1]);
#endif
            longTranForce += stringT[k]->longTran();
        }

        //longTranForce = 0;
        longTranForces[tTran] = longTranForce;
        tranForces[tTran] = tranForce;
        tranForcesH[tTran] = tranForceH;
        tTran = (tTran + 1)%TranBufferSize;

        if(tLong >= 0) {
            longForce = stringT[0]->tran2long(longDelay);
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

    tTranRead = (tTranRead + 1)%TranBufferSize;

    return output;
}

vec4 PianoNote::go4()
{
    Value *v = piano->vals;

    if(!bInit4) {
        for(int k=0;k<nstrings;k++) {
            stringT[k]->init_string4();
#ifdef HSTRING
            stringHT[k]->init_string4();
#endif
        }
        // Align indices to 4-byte boundaries for SIMD operations
        // Without this, switching from go() to go4() can cause buffer overruns
        tTran = (tTran + 3) & ~3;
        if (tTran >= TranBufferSize) tTran = 0;
        tTranRead = (tTranRead + 3) & ~3;
        if (tTranRead >= TranBufferSize) tTranRead = 0;
        bInit4 = true;
    }

    vec4 sbloadT = {0};
    vec4 sbloadHT = {0};

    for(int k=0;k<nstrings;k++)
	{
        sbloadT = simde_mm_add_ps (sbloadT, stringT[k]->go_string4());
#ifdef HSTRING
        sbloadHT = simde_mm_add_ps (sbloadHT, stringHT[k]->go_string4());
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
        tranForce = simde_mm_add_ps (tranForce, stringT[k]->go_soundboard4(out[0]));
#ifdef HSTRING
        tranForceH = simde_mm_add_ps (tranForceH, stringHT[k]->go_soundboard4(out[1]));
#endif
        longTranForce = simde_mm_add_ps (longTranForce, stringT[k]->longTran4());
    }

    simde_mm_store_ps (longTranForces + tTran, longTranForce);
    simde_mm_store_ps (tranForces + tTran, tranForce);
    simde_mm_store_ps (tranForcesH + tTran, tranForceH);
    tTran = (tTran + 4)%TranBufferSize;
    
    vec4 longForce = stringT[0]->tran2long4 (longDelay);
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

void Piano::addVoice(PianoNote *v)
{ 
    if(voiceList)
	{
        v->next = voiceList;
        v->prev = voiceList->prev;
        voiceList->prev->next = v;
        voiceList->prev = v;
    }
	else
	{
        voiceList = v;
        v->prev = v;
        v->next = v;
    }
}

void Piano::removeVoice(PianoNote *v)
{	
    if (v == voiceList)
    {
        if(v == v->next)
            voiceList = nullptr;
        else
            voiceList = v->next;
    }
    PianoNote *p = v->prev;
    PianoNote *n = v->next;
    p->next = n;
    n->prev = p;
}

void Piano::init (float Fs_, int blockSize_)
{
    Fs = Fs_;
    blockSize = blockSize_;
    voiceList = nullptr;
    PianoNote::fillFrequencyTable();

    for (int k = PIANO_MIN_NOTE; k <= PIANO_MAX_NOTE; k++)
        noteArray[k] = std::make_unique<PianoNote>(k, int (Fs), this);

    input.resize(size_t (blockSize));

#ifdef FDN_REVERB
    soundboard = std::make_unique<Reverb>(Fs);
#else
    soundboard = std::make_unique<ConvolveReverb<revSize>>(blockSize);
#endif
}

Piano::Piano()
{
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

    for (int k = 0; k < nstrings; k++)
    {
        stringT[k] = std::make_unique<dwgs>();
        stringHT[k] = std::make_unique<dwgs>();
    }
    hammer = std::make_unique<Hammer>();

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
    return (energy < 1e-8 * maxEnergy);
}

void PianoNote::triggerOn (float velocity, float* tune)
{
    //logf("note = %d velocity =  %g\n",note,velocity);
    Value *v = piano->vals;
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
            stringT[k]->init_string1();
#ifdef HSTRING
            stringHT[k]->init_string1();
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
        
        upsampleMin = std::max(upsampleMin, stringT[k]->getMinUpsample(downsample, Fs, fk, hp, B));
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

        stringT[k]->set(Fs,longmodes,downsample,upsample,fk,v[pStringDecay],v[pStringLopass],B,L,longFreq1,gammaL,gammaL2,hp,Z);
#ifdef HSTRING
        stringHT[k]->set(Fs,longmodes,downsample,upsample,fk,v[pStringDecay],v[pStringLopass],B,L,longFreq1,gammaL,gammaL2,hp,Z);
#endif

        maxDel2 = std::max(stringT[k]->getDel2(), maxDel2);
    }

    hammer->set(resample*Fs,m,K,p,Z,alpha,maxDel2+128);
    hammer->strike(velocity);
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
    Value *v = piano->vals;
    float gammaLDamped = v[pLongitudinalGammaDamped];
    float gammaL2Damped = v[pLongitudinalGammaQuadraticDamped];
	
    for (int k = 0; k < nstrings; k++)
	{
        stringT[k]->damper (v[pDampedStringDecay],v[pDampedStringLopass],gammaLDamped,gammaL2Damped,128);
        stringHT[k]->damper (v[pDampedStringDecay],v[pDampedStringLopass],gammaLDamped,gammaL2Damped,128);
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
}

void Piano::process (float* out, int samples)
{
    for (int i = 0; i < samples; i++)
    {
        PianoNote* v = voiceList;
        float output = 0;
        do
        {
            if (v)
                output += v->goUp();

        } while (v && (v=v->next) && (v != voiceList));
#ifdef FDN_REVERB
        out[i] = vals[pVolume] * soundboard->reverb(output);
#else
        out[i] =  vals[pVolume] * output;
        input[size_t(i)] =  vals[pVolume] * output;
#endif
    }


#ifndef FDN_REVERB
    soundboard->fft_conv(input.data(), out, samples);
#endif
}

void Piano::process (float** out, int samples, int offset)
{
    process (out[0] + offset, samples);
    memcpy (out[1] + offset, out[0] + offset, size_t (samples) * sizeof (float));
}

void Piano::process (float** outS, int sampleFrames, juce::MidiBuffer& midi)
{ 
    int delta = 0;

    PianoNote* v = voiceList;
    PianoNote* remove[NUM_NOTES];

    int k = 0;

    do
    {
        if (v && v->isDone())
        {
            v->deActivate();
            remove[k++] = v;
        }
    } while (v && (v=v->next) && (v!=voiceList));

    for (int j = 0; j < k; j++)
        removeVoice (remove[j]);

    for (auto meta : midi)
    {
        auto m = meta.getMessage();

        if (! m.isNoteOnOrOff())
            continue;

        int nextDelta = int (m.getTimeStamp());
        nextDelta = std::min (nextDelta, sampleFrames);
        process (outS, nextDelta - delta, delta);

        if (m.getNoteNumber() >= PIANO_MIN_NOTE && m.getNoteNumber() <= PIANO_MAX_NOTE)
        {
            PianoNote* v = noteArray[size_t(m.getNoteNumber())].get();
            if (m.isNoteOn())
            {
                if (! (noteArray[size_t(m.getNoteNumber())]->isActive()))
                    addVoice (v);

                float velocity = vals[pMaxVelocity] * pow ((float)(m.getVelocity() / 127.0f), 2.0f);
                v->triggerOn (velocity, nullptr);
            }
            else
            {
                if (v)
                    v->triggerOff();
            }
        }

        delta = nextDelta;
    }

    process (outS, sampleFrames - delta, delta);
}

void Piano::triggerOn (int note, float velocity, float* tune)
{
    PianoNote* v = noteArray[size_t(note)].get();
    addVoice (v);
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
#ifdef FDN_REVERB
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
#ifdef FDN_REVERB
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
#ifdef FDN_REVERB
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
    }
}

//-----------------------------------------------------------------------------------------
float Piano::getParameter (int32_t index)
{
    return vals[index].f;
}
