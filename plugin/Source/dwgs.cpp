#include "dwgs.h"
#include <math.h>
#include <stdio.h>
#include "utils.h"
#include <stdlib.h>
#include <cassert>
#include <cstdint>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define DEBUG_OUTPUT(msg) do { OutputDebugStringA(msg); fprintf(stderr, "%s", msg); fflush(stderr); } while(0)
#else
#define DEBUG_OUTPUT(msg) do { fprintf(stderr, "%s", msg); fflush(stderr); } while(0)
#endif

using namespace std;

// Debug assertion macro - uses abort() so it works in Release builds too
#define DWGS_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        char _buf[512]; \
        snprintf(_buf, sizeof(_buf), "DWGS ASSERT FAILED: %s\n  File: %s, Line: %d\n  Condition: %s\n", \
                msg, __FILE__, __LINE__, #cond); \
        DEBUG_OUTPUT(_buf); \
        abort(); \
    } \
} while(0)

// Check if pointer is valid (not null, not small int, not -1/high invalid)
#define DWGS_CHECK_PTR(ptr, name) do { \
    uintptr_t p = (uintptr_t)(ptr); \
    if (p == 0 || p < 0x10000 || p > 0x00007FFFFFFFFFFF) { \
        char _buf[512]; \
        snprintf(_buf, sizeof(_buf), "DWGS BAD PTR: %s = %p (0x%llx) - invalid pointer!\n  File: %s, Line: %d\n", \
                name, (void*)(ptr), (unsigned long long)p, __FILE__, __LINE__); \
        DEBUG_OUTPUT(_buf); \
        abort(); \
    } \
} while(0)

// Helper to check if a pointer is within a buffer range
#define DWGS_CHECK_PTR_RANGE(ptr, base, size, msg) do { \
    if ((ptr) < (base) || (ptr) >= (base) + (size)) { \
        char _buf[512]; \
        snprintf(_buf, sizeof(_buf), "DWGS PTR RANGE FAILED: %s\n  ptr=%p, base=%p, size=%d, offset=%td\n  File: %s, Line: %d\n", \
                msg, (void*)(ptr), (void*)(base), (int)(size), (ptrdiff_t)((ptr) - (base)), __FILE__, __LINE__); \
        DEBUG_OUTPUT(_buf); \
        abort(); \
    } \
} while(0)


/*

 pin - 0-|--1 -- 2 bridge - soundboard
 |
 3
 hammer

 The hammer is imparted with an initial velocity.
 The hammer velocity becomes an input load using the impedances at the junction.
 The 0 & 1 strings carry the velocity wave.
 The 0 string is terminated at an infinite impedance pin.
 The 1 string interacts with the soundboard by inducing a load
 There are no additional ingoing waves into the strings from either the hammer or the bridge

 */

//#define MERGE_FILTER 1
//#define LONGMODE_DEBUG 1
//#define LONG_DEBUG 1
//#define STRING_DEBUG 1
//#define DEBUG_4


vec4 dwgs::tran2long4 (int delay)
{
    if(nLongModes == 0) {
        vec4 out = {0};
        return out;
    }

    // Defensive checks: ensure buffers are allocated and valid
    DWGS_CHECK_PTR(wave0.get(), "wave0");
    DWGS_CHECK_PTR(wave.get(), "wave");
    DWGS_CHECK_PTR(Fl.get(), "Fl");

    // Verify delay buffer pointers are valid (not null and reasonable)
    DWGS_ASSERT(d1.x != nullptr, "d1.x is null");
    DWGS_ASSERT(d2.x != nullptr, "d2.x is null");

    // Check that x points to x_ + 8 (sanity check for Delay object integrity)
    DWGS_ASSERT(d1.x == d1.x_ + 8, "d1.x doesn't point to d1.x_ + 8 - object may be corrupted");
    DWGS_ASSERT(d2.x == d2.x_ + 8, "d2.x doesn't point to d2.x_ + 8 - object may be corrupted");

    // Check delay parameters are reasonable
    DWGS_ASSERT(delTab >= 0 && delTab < DelaySize, "delTab out of range");
    DWGS_ASSERT(del0 >= 0 && del0 < DelaySize, "del0 out of range");
    DWGS_ASSERT(del1 >= 0 && del1 < DelaySize, "del1 out of range");
    DWGS_ASSERT(del2 >= 0 && del2 < DelaySize, "del2 out of range");
    DWGS_ASSERT(del4 >= 0 && del4 < DelaySize, "del4 out of range");
    DWGS_ASSERT(delay >= 0, "delay is negative");

    // Check cursor positions
    DWGS_ASSERT(d1.cursor >= 0 && d1.cursor < DelaySize, "d1.cursor out of range");
    DWGS_ASSERT(d2.cursor >= 0 && d2.cursor < DelaySize, "d2.cursor out of range");

    alignas(32) float out[4];
    float *x;
    int cur;
    int n;
    float *wave10;

    /********* bottom *********/
    x = d1.x;
    /* wave10[-del0] = x[cursor - del1] */
    n = delTab + 4;
    cur = (d1.cursor + DelaySize - delay + del0 - del1 + 1 ) % DelaySize;
    wave10 = wave0 + delTab + 4;

    DWGS_ASSERT(n > 0, "n (bottom) must be positive");
    DWGS_ASSERT(cur >= 0 && cur < DelaySize, "cur (bottom) out of DelaySize range");

    if(n <= cur)
    {
        float *x1 = x + cur;
        // Check bounds: reading x1[-n] to x1[-1], i.e., x[cur-n] to x[cur-1]
        DWGS_ASSERT(cur - n >= 0, "bottom memcpy: cur-n underflow");
        DWGS_CHECK_PTR_RANGE(x1 - n, x, DelaySize, "bottom x1-n out of d1.x range");
        // Check wave10 bounds: writing wave10[-n] to wave10[-1], i.e., wave0[delTab+4-n] to wave0[delTab+3]
        DWGS_ASSERT(wave10 - n >= wave0, "bottom memcpy: wave10-n underflow");
        memcpy (wave10-n, x1-n, size_t (n) * sizeof(float));
    }
    else
    {
        float *x1 = x + cur;
        DWGS_ASSERT(cur >= 0, "bottom else: cur must be non-negative");
        DWGS_CHECK_PTR_RANGE(x1 - cur, x, DelaySize, "bottom else x1-cur out of range");
        memcpy(wave10-cur, x1-cur, size_t (cur) * sizeof(float));

        int cur2 = cur + DelaySize;
        x1 = x + cur2;
        DWGS_ASSERT(cur2 >= n, "bottom else: cur2 must be >= n");
        DWGS_CHECK_PTR_RANGE(x1 - n, x, DelaySize + 8, "bottom else x1-n (wrapped) out of range");
        memcpy(wave10-n, x1-n, size_t (n-cur) * sizeof(float));
    }

    /********* top *********/

    for(int j=0; j<4; j++) {

        x = d2.x;
        cur = (d2.cursor + DelaySize - delay - 4 + j + del4) % DelaySize;

        DWGS_ASSERT(cur >= 0 && cur < DelaySize, "top cur out of range");

        float* wave10 = wave0 + j;
        DWGS_ASSERT(wave10 >= wave0, "top wave10 underflow");

        n = del0 + del2 + del4 + 5;
        DWGS_ASSERT(n > 0, "top n must be positive");
        DWGS_ASSERT(n < DelaySize, "top n exceeds DelaySize");

        if (n <= cur)
        {
            float *x1 = x + cur;
            // ms4 reads from x1-n+1 backwards for n elements
            // Check: x1-n+1 must be >= x (i.e., cur-n+1 >= 0)
            DWGS_ASSERT(cur - n + 1 >= 0, "top ms4: cur-n+1 underflow");
            DWGS_CHECK_PTR_RANGE(x1 - n + 1, x, DelaySize, "top ms4 x1-n+1 out of range");
            DWGS_CHECK_PTR_RANGE(x1, x, DelaySize, "top ms4 x1 out of range");
            ms4 (wave10, x1 - n + 1, wave, n);
        }
        else
        {
            float *x1 = x + cur;
            // ms4 reads from x1-cur for cur+1 elements
            DWGS_ASSERT(cur >= 0, "top else: cur must be non-negative for ms4");
            DWGS_CHECK_PTR_RANGE(x1 - cur, x, DelaySize, "top else ms4 x1-cur out of range");
            ms4(wave10,x1-cur,wave,cur+1);

            int cur2 = cur + DelaySize;
            x1 = x + cur2;
            DWGS_ASSERT(cur2 >= 0 && cur2 < 2 * DelaySize, "top else cur2 out of range");

            int n4 = std::min((((cur+1)>>2)<<2) + 3,n);

            for(int i=cur+1; i<=n4; i++) {
                DWGS_ASSERT(i >= 0 && i < delTab + 32, "top else loop: wave index out of range");
                DWGS_ASSERT(cur2 - i >= 0, "top else loop: x1[-i] underflow");
                wave[i] = square(wave10[i] - x1[-i]);
                cur++;
            }

            if(cur + 1 < n) {
                DWGS_ASSERT(cur + 1 >= 0, "top else ms4 #2: cur+1 must be non-negative");
                DWGS_CHECK_PTR_RANGE(x1 - n + 1, x, DelaySize + 8, "top else ms4 #2 x1-n+1 out of range");
                ms4(wave10+cur+1,x1-n+1,wave+cur+1,n-cur-1);
            }
        }

        diff4(wave, Fl, delTab+1);

        out[j] = 0;
        for(int k=1; k<=nLongModes; k++) {
            DWGS_ASSERT(k >= 0 && k < nMaxLongModes, "modeTable index out of range");
            float *tab = modeTable[size_t(k)].get();
            DWGS_ASSERT(tab != nullptr, "modeTable[k] is null");
            float F = sse_dot (delTab + 4, tab, Fl);
            float Fbl = longModeResonator[k].go (F);
            out[j] += Fbl;
        }
    }

    return simde_mm_load_ps(out);
}

dwgs::dwgs() :
fracDelayTop(8), fracDelayBottom(16), hammerDelay(8), d2(3)

{
    // AlignedBuffer members are already default-initialized to empty
    c1 = 0;
    c3 = 0;
    nDamper = 0;
    nLongModes = 0;
    delTab = 0;

    // Initialize delay variables to prevent garbage values in tran2long4
    del0 = 0;
    del1 = 0;
    del2 = 0;
    del3 = 0;
    del4 = 0;
    del5 = 0;

    // Initialize other numeric members
    downsample = 1;
    upsample = 1;
    M = 1;
    L = 0;
    omega = 0;
    f = 0;
    inpos = 0;
    B = 0;
    longFreq1 = 0;
    dDispersion = 0;
    dTop = 0;
    dHammer = 0;
    dBottomAndLoss = 0;
    c1M = 0;
    c3M = 0;

    vec4 z = {0};
    v0_0 = z;
    v0_1 = z;
    v0_2 = z;
    v0_3 = z;
    v0_4 = z;
    v0_5 = z;
    v1_1 = z;
    v1_2 = z;
    v1_3 = z;
    v1_4 = z;
    v1_5 = z;

    a0_0 = 0.0f;
    a0_1 = 0.0f;
    a0_2 = 0.0f;
    a0_3 = 0.0f;
    a0_4 = 0.0f;
    a0_5 = 0.0f;
    a1_1 = 0.0f;
    a1_2 = 0.0f;
    a1_3 = 0.0f;
    a1_4 = 0.0f;
    a1_5 = 0.0f;

    // Verify Delay buffers are properly constructed
    DWGS_ASSERT(d0.x == d0.x_ + 8, "dwgs ctor: d0.x not properly initialized");
    DWGS_ASSERT(d1.x == d1.x_ + 8, "dwgs ctor: d1.x not properly initialized");
    DWGS_ASSERT(d2.x == d2.x_ + 8, "dwgs ctor: d2.x not properly initialized");
    DWGS_ASSERT(d3.x == d3.x_ + 8, "dwgs ctor: d3.x not properly initialized");
}

void dwgs::set(float Fs, int longmodes, int downsample, int upsample, float f, float c1, float c3, float B, float L, float longFreq1, float gammaL, float gammaL2, float inpos, float Z)
{
    this->downsample = downsample;
    this->upsample = upsample;
    float resample = (float)upsample / (float)downsample;


    this->L = L;
    this->f = f;
    this->inpos = inpos;

    if(f > 400) {
        M = 1;
    } else {
        M = 4;
    }

    for(int m=0;m<M;m++) {
        dispersion[m].create(B,f,M,downsample,upsample);
    }
    this->B = B;
    this->longFreq1 = longFreq1;


    float deltot = Fs/downsample/f*upsample;
    this->omega = float (TWOPI) / deltot;
    dDispersion = M*dispersion[0].phasedelay(omega);

    //logf("hammer delay = %g\n", inpos*deltot);
    del0 = (int)(0.5 * (inpos*deltot));
    del1 = (int)((inpos*deltot) - 1.0);
    if(del1 < 2) abort();
    dHammer = inpos * deltot - del1;
    int dd = std::min(4, del1 - 2);
    dHammer += dd;
    del1 -= dd;

    hammerDelay.create(dHammer,(int)dHammer);
    del5 = (int)dHammer;

    // XXX resize arrays
    del2 = (int)(0.5*(deltot - inpos * deltot) - 1.0);
    del3 = (int)(0.5*(deltot - inpos * deltot) - dDispersion - 2.0);

    if(del2 < 1) abort();
    if(del3 < 1) abort();

    float delHalf = 0.5f * deltot;

    float delHammerHalf = 0.5f * inpos * deltot;

    dTop = delHalf - delHammerHalf - del2;
    dd = std::min(4, del2 - 1);
    dTop += dd;
    del2 -= dd;
    //logf("hammer = %g\n",dHammer);
    //logf("top = %g\n",dTop);
    //logf("dispersion(%g) = %g\n",omega,dDispersion);

    del4 = (int)dTop;

    fracDelayTop.create(dTop,(int)dTop);

    dBottomAndLoss = delHalf - delHammerHalf - del3 - dDispersion;
    dd = std::min(4, del3 - 1);
    dBottomAndLoss += dd;
    del3 -= dd;

    d0.setDelay(0);
    d1.setDelay(del1-1);
    d2.setDelay(del2-1);
    d3.setDelay(del3-1);

    delTab = del0 + del2 + del4;
    float delta = dTop - (int)dTop;
    float delta2 = delHalf - delTab - delta;

    //logf("%d %d %d %d %g %g\n", del0, del1, del2, del3, delta, delta2);

    wave0.allocate(size_t(delTab + 32));
    wave1.allocate(size_t(delTab + 32));
    wave.allocate(size_t(delTab + 32));
    Fl.allocate(size_t(delTab + 32));
    wave0.clear();
    wave1.clear();
    Fl.clear();

    //logf("dwgs top %d %d %d %d %g %g %g %g %g %g %g %g\n",del0,del1,del2,del3,dHammer+1+del2+dTop,del1+del3+dDispersion+lowpassdelay+dBottom, dTop, dBottom, dHammer, inpos*deltot, lowpassdelay, dDispersion);

    nLongModes = (int)(0.5f * Fs / downsample / longFreq1 - 0.5);
    nLongModes = (int)(0.5f * Fs / longmodes / longFreq1 - 0.5);
    //logf("nlongmodes = %d\n", nLongModes);
    if(nLongModes >= nMaxLongModes) abort();

    //nLongModes = 1;
    gammaL /= resample;

    for(int k=1; k <= nLongModes; k++) {
        float omegak = float (TWOPI) * k * longFreq1 / (Fs * resample);
        float gammak = gammaL * (1.0f + gammaL2 * k * k);
        fLong[k] = omegak / float (TWOPI);
        longModeResonator[k].create(omegak, gammak);

        //logf("fL%d=%g L%d/fT = %g\n",k,k*longFreq1,k,k*longFreq1/f );
#ifdef LONGMODE_DEBUG
        printf("%g ",omegak);
#endif
        if(delTab) {
            float n = float (PI) * k / delHalf;
            modeTable[size_t(k)].allocate(size_t(delTab + 8));

            for (int i = 0; i <= delTab; i++)
            {
                float d = i + delta;
                float s = sin(d*n);
                modeTable[size_t(k)][size_t(i+3)] = s;
            }
            //logf("maxd = %g/ %g\n",delTab+delta,delHalf);
            modeTable[size_t(k)][0] = 0;
            modeTable[size_t(k)][1] = 0;
            modeTable[size_t(k)][2] = 0;
            modeTable[size_t(k)][3] *= (0.5f + delta);
            modeTable[size_t(k)][size_t(delTab+3)] *= (0.5f + delta2);
        }
    }
#ifdef LONGMODE_DEBUG
    printf("\n");
    for(int k=1; k <= nLongModes; k++) {
        float gammak = gammaL * (1.0 + gammaL2 * k * k);
        printf("%g ",gammak);
    }
    printf("\n");
#endif

    damper(c1,c3,gammaL,gammaL2,128);
}

// XXX dwgresonator create 
void dwgs::damper (float c1, float c3, float gammaL, float gammaL2, int nDamper)
{
    if (this->c1 == 0)
    {
        this->c1 = c1;
        this->c3 = c3;
        this->nDamper = 0;

        loss.create(f,c1,c3,(float)upsample/(float)downsample);
        float lowpassdelay = loss.phasedelay(omega);
        float dBottom = dBottomAndLoss - lowpassdelay;
        //logf("bottom = %g\n",dBottom);
        fracDelayBottom.create(dBottom,std::min(5,(int)dBottom));

    }
    else
    {
        c1M = pow (c1 / this->c1, 4.0 / nDamper);
        c3M = pow (c3 / this->c3, 4.0 / nDamper);
        this->nDamper = nDamper;
    }


#ifdef MERGE_FILTER
    fracDelayBottom.merge(loss);
    for(int m=0;m<M;m++) {
        fracDelayBottom.merge(dispersion[m]);
    }
#endif


}

dwgs::~dwgs()
{
    // AlignedBuffer members automatically deallocate in their destructors
    // Poison delay buffer pointers to detect use-after-free
    d0.x = reinterpret_cast<float*>(0xDEADBEEFDEADBEEF);
    d1.x = reinterpret_cast<float*>(0xDEADBEEFDEADBEEF);
    d2.x = reinterpret_cast<float*>(0xDEADBEEFDEADBEEF);
    d3.x = reinterpret_cast<float*>(0xDEADBEEFDEADBEEF);

    nLongModes = -1;  // Poison this too
}

int dwgs::getMaxDecimation(int downsample, int upsample, float Fs, float f, float magic)
{
    //float magic = 120;
    return 1;
    int decimation = 1;
    do {
        float deltot = (Fs/downsample)*upsample / f;
        //printf("%g %g\n",deltot, decimation * magic);
        if(deltot > decimation * magic) {
            decimation *= 2;
        } else {
            break;
        }
    } while(true);

    return decimation;
}

int dwgs::getMinUpsample(int downsample, float Fs, float f, float inpos, float B)
{
    int upsample = 1;

    int M;
    if(f > 400) {
        M = 1;
    } else {
        M = 4;
    }

    do {
        float resample = (float)upsample / (float)downsample;
        float deltot = (Fs*resample) / f;
        del1 = (int)((inpos*deltot) - 1.0);

        for (int m = 0; m < M; m++)
            dispersion[m].create(B,f,M,downsample,upsample);

        float omega = float (TWOPI) / deltot;
        dDispersion = M * dispersion[0].phasedelay (omega);
        del3 = (int)(0.5f * (deltot - inpos * deltot) - dDispersion - 2.0);

        if (del1 < 2 || del3 < 4)
            upsample *= 2;
        else
            break;

    } while (true);

    return upsample;
}

float dwgs::input_velocity()
{
    return a0_3 + a1_2;
}

float dwgs::next_input_velocity()
{
    return d2.probe() + d1.probe();
}

// returns input to soundboard
float dwgs::go_string()
{
    // Verify delay buffer integrity at start
    DWGS_ASSERT(d0.x == d0.x_ + 8, "go_string: d0.x corrupted");
    DWGS_ASSERT(d1.x == d1.x_ + 8, "go_string: d1.x corrupted");
    DWGS_ASSERT(d2.x == d2.x_ + 8, "go_string: d2.x corrupted");
    DWGS_ASSERT(d3.x == d3.x_ + 8, "go_string: d3.x corrupted");

    if (nDamper > 0)
    {
        if((nDamper & 3) == 0)
        {
            c1 *= c1M;
            c3 *= c3M;
            loss.create(f,c1,c3,(float)upsample/(float)downsample);
            float lowpassdelay = loss.phasedelay(omega);
            float dBottom = dBottomAndLoss - lowpassdelay;
            fracDelayBottom.create(dBottom,std::min(5,(int)dBottom));
        }
        nDamper--;
    }
    float a;

    a = d0.goDelay(a0_2);
    a = hammerDelay.filter(a);
    a1_2 = d1.goDelay(-a);
    a0_3 = d2.goDelay(a0_4);
    a1_4 = d3.goDelay(a1_3);

    //cout << "0 del =" << a1_2 << " / " << a0_3 << " / " << a1_4 << "\n";

    a = a1_4;
#ifndef MERGE_FILTER
    for(int m=0;m<M;m++) {
        a = dispersion[m].filter(a);
    }
    a = loss.filter(a);
#endif
    a1_5 = fracDelayBottom.filter(a);


    return a1_5;
}

float dwgs::go_soundboard (float load_h, float load_sb)
{
    a0_2 = a0_3 + load_h;
    a1_3 = a1_2 + load_h;
    a0_5 = load_sb - a1_5;
    a0_4 = fracDelayTop.filter(a0_5);

    //cout << a1_4 << " " << "\n";
#ifdef DEBUG_4
    //cout << "1 load_sb=" << load_sb << " / " << a0_2 << " / " << a0_4 << " / " << a1_3 << "\n";
    //cout << "1 load_sb=" << load_sb << " / " << a0_2 << " / " << a1_4 << " / " << a0_5 << " / " << a1_5 << "\n";
#endif

    return a0_5 - a1_5;
}


/* 
 v1_5 is left unused by string4
 string1 requires a0_2, a0_4, a1_3

 the last 4 outputs of dispersion, loss, bottom filters are unused
 a0_2 = v0_2[0] but last 3 outputs of del2 are unused
 a0_4 = v0_4[3]

 del1 is used
 del0 is used
 topFilter is used
 hammer is used
 */


void dwgs::init_string1()
{
    a0_4 = simde_mm_cvtss_f32 (simde_mm_shuffle_ps (v0_4, v0_4, SIMDE_MM_SHUFFLE (0, 1, 2, 3)));
    a0_3 = simde_mm_cvtss_f32 (v0_2);
    a0_2 = a0_3;
    a1_2 = d1.probe();
    a1_3 = a1_2;

    d2.backupCursor();
    //cout << "1 init " <<  a0_2 << " / " << a0_4 << " / " << a1_3 << "\n";
}

/* a1_3 a0_2 a0_4 are left unused by string1 
 string4 requires v0_2

 del1+del2+del3-3
 loop delay: +4
 a0_2 : d2.probe4, -2
 a1_3 : d1.backup(), +1
 a0_4 : d2.delay(), +0
 */

void dwgs::init_string4()
{
    alignas(32) float v[4];
    v[0] = a0_2;
    a0_3 = d2.goDelay(a0_4);
    a0_2 = a0_3;
    v[1] = a0_2;

    v0_2 = simde_mm_blend_ps (simde_mm_load_ps(v), d2.probe4(), 12);
    d1.backup();

#ifdef DEBUG_4
    //cout << "v0_2 = " << v0_2 << " / " << a0_2 << "\n";
#endif
}

vec4 dwgs::go_string4()
{
    // Verify delay buffer integrity at start
    DWGS_ASSERT(d0.x == d0.x_ + 8, "go_string4: d0.x corrupted");
    DWGS_ASSERT(d1.x == d1.x_ + 8, "go_string4: d1.x corrupted");
    DWGS_ASSERT(d2.x == d2.x_ + 8, "go_string4: d2.x corrupted");
    DWGS_ASSERT(d3.x == d3.x_ + 8, "go_string4: d3.x corrupted");
    DWGS_ASSERT(d0.cursor >= 0 && d0.cursor < DelaySize, "go_string4: d0.cursor out of range");
    DWGS_ASSERT(d1.cursor >= 0 && d1.cursor < DelaySize, "go_string4: d1.cursor out of range");
    DWGS_ASSERT(d2.cursor >= 0 && d2.cursor < DelaySize, "go_string4: d2.cursor out of range");
    DWGS_ASSERT(d3.cursor >= 0 && d3.cursor < DelaySize, "go_string4: d3.cursor out of range");

    if(nDamper > 0) {
        if((nDamper & 3) == 0) {
            c1 *= c1M;
            c3 *= c3M;
            loss.create(f,c1,c3,(float)upsample/(float)downsample);
            float lowpassdelay = loss.phasedelay(omega);
            float dBottom = dBottomAndLoss - lowpassdelay;
            fracDelayBottom.create(dBottom,std::min(5,(int)dBottom));
        }
        nDamper-=4;
    }

    vec4 v;

    v = d0.goDelay4(v0_2);
    v = hammerDelay.filter4(v);
    v1_2 = d1.goDelay4 (simde_x_mm_negate_ps (v));
    v1_3 = v1_2;
    v1_4 = d3.goDelay4(v1_3);
    v = v1_4;
#ifndef MERGE_FILTER
    for(int m=0;m<M;m++) {
        v = dispersion[m].filter4(v);
    }
    v = loss.filter4(v);
#endif
    v1_5 = fracDelayBottom.filter4(v);

    return v1_5;
}

vec4 dwgs::go_soundboard4(vec4 load_sb)
{
    v0_5 = simde_mm_sub_ps (load_sb, v1_5);
    v0_4 = fracDelayTop.filter4(v0_5);
    v0_3 = d2.goDelay4(v0_4);
    v0_2 = v0_3;

#ifdef DEBUG_4  
    //cout << "4 load_sb=" << load_sb << " / " << v0_2 << " / " << v0_4 <<  " / " << v1_3 << "\n";
    //cout << "4 load_sb=" << load_sb << " / " << v0_2 << " / " << v1_4 << " / " << v0_5 << " / " << v1_5 << "\n";
#endif
    return simde_mm_sub_ps (v0_5, v1_5);
}

vec4 dwgs::longTran4() {
    vec4 v = simde_mm_sub_ps (v0_5, v1_5);
    return simde_mm_mul_ps (v, v);
}

float dwgs::longTran() {
    return square(a0_5 - a1_5);
}

//0 2
//1 3
/* wave is the difference of top and bottom strings 
 wave[0] is on the right side, so samples are delayed by frac(topDelay)
 */
// d/dx (dy/dx)^2
// dy/dx = dy/dt / dx/dt = v / vTran

float dwgs::tran2long(int delay)
{
    if(nLongModes == 0) return 0;

    // Defensive checks: ensure buffers are allocated and valid
    DWGS_CHECK_PTR(wave.get(), "tran2long: wave");
    DWGS_CHECK_PTR(Fl.get(), "tran2long: Fl");

    // Verify delay buffer integrity
    DWGS_ASSERT(d0.x != nullptr, "tran2long: d0.x is null");
    DWGS_ASSERT(d1.x != nullptr, "tran2long: d1.x is null");
    DWGS_ASSERT(d2.x != nullptr, "tran2long: d2.x is null");
    DWGS_ASSERT(d3.x != nullptr, "tran2long: d3.x is null");
    DWGS_ASSERT(d0.x == d0.x_ + 8, "tran2long: d0.x corrupted");
    DWGS_ASSERT(d1.x == d1.x_ + 8, "tran2long: d1.x corrupted");
    DWGS_ASSERT(d2.x == d2.x_ + 8, "tran2long: d2.x corrupted");
    DWGS_ASSERT(d3.x == d3.x_ + 8, "tran2long: d3.x corrupted");

    // Check delay parameters
    DWGS_ASSERT(del0 >= 0 && del0 < DelaySize, "tran2long: del0 out of range");
    DWGS_ASSERT(del2 >= 0 && del2 < DelaySize, "tran2long: del2 out of range");
    DWGS_ASSERT(del4 >= 0 && del4 < DelaySize, "tran2long: del4 out of range");
    DWGS_ASSERT(delTab >= 0 && delTab < DelaySize, "tran2long: delTab out of range");

    float *x = d2.x;
    int cur = (d2.cursor + DelaySize - delay + del4) % DelaySize;
    DWGS_ASSERT(cur >= 0 && cur < DelaySize, "tran2long: d2 cur out of range");

    int n = del2 + del4;
    if(n <= cur) {
        float *x1 = x + cur;
        for(int i=0; i<n; i++) {
            wave[i] = x1[-i];
        }
    } else {
        float *x1 = x + cur;
        for(int i=0; i<=cur; i++) {
            wave[i] = x1[-i];
        }
        int cur2 = cur + DelaySize;
        x1 = x + cur2;
        for(int i=cur+1; i<n; i++) {
            wave[i] = x1[-i];
        }
    }

    x = d0.x;
    cur = (d0.cursor + DelaySize - delay) % DelaySize;
    float *wave10 = wave + del2 + del4;

    if(del0 <= cur) {
        float *x1 = x + cur;
        for(int i=0; i<=del0; i++) {
            wave10[i] = x1[-i];
        }
    } else {
        float *x1 = x + cur;
        for(int i=0; i<=cur; i++) {
            wave10[i] = x1[-i];
        }
        int cur2 = cur + DelaySize;
        x1 = x + cur2;
        for(int i=cur+1; i<=del0; i++) {
            wave10[i] = x1[-i];
        }
    }

#ifdef STRING_DEBUG
    for(int i=0; i<=delTab; i++) {
        printf("%g ",wave[i]);
    }
    printf("\n");
#endif


    /********* bottom *********/

    x = d1.x;
    /* wave10[-del0] = x[cursor - del1] */

    cur = (d1.cursor + DelaySize - delay + del0 - del1) % DelaySize;
    wave10 = wave + delTab;
    if(del0 < cur) {
        float *x1 = x + cur;
        for(int i=0; i>=-del0; i--) {
            wave10[i] -= x1[i];
        }
    } else {
        float *x1 = x + cur;
        for(int i=0; i>=-cur; i--) {
            wave10[i] -= x1[i];
        }
        int cur2 = cur + DelaySize;
        x1 = x + cur2;
        for(int i=-cur-1; i>=-del0; i--) {
            wave10[i] -= x1[i];
        }
    }

    x = d3.x;
    cur = (d3.cursor + DelaySize - delay) % DelaySize;
    n = del2 + del4;
    wave10 = wave + n;
    if(n < cur) {
        float *x1 = x + cur;
        for(int i=-1; i>=-n; i--) {
            wave10[i] -= x1[i];
        }
    } else {
        float *x1 = x + cur;
        for(int i=-1; i>=-cur; i--) {
            wave10[i] -= x1[i];
        }
        int cur2 = cur + DelaySize;
        x1 = x + cur2;
        for(int i=-cur-1; i>=-n; i--) {
            wave10[i] -= x1[i];
        }
    }

#ifdef STRING_DEBUG
    for(int i=0; i<=delTab; i++) {
        printf("%g ",wave[i]);
    }
    printf("\n");
#endif

    for(int i=0; i<=delTab; i++) {
        wave[i] = square(wave[i]);
    }

    Fl[3] = 2.0f * (wave[1] - wave[0]);
    for(int i=1; i<delTab; i++) {
        Fl[3+i] = (wave[i+1] - wave[i-1]);
    }
    Fl[delTab+3] = 2.0f * (wave[delTab] - wave[delTab-1]);

#ifdef LONG_DEBUG
    for(int i=0; i<=delTab; i++) {
        printf("%g ",Fl[i+3]);
    }
    printf("\n");
#endif

    float Fbl = 0;
    for(int k=1; k<=nLongModes; k++) {
        float *tab = modeTable[k];
        float F = sse_dot (delTab + 4, tab, Fl);
#ifdef LONGMODE_DEBUG
        cout << F << " ";
#endif
        Fbl += longModeResonator[k].go(F);
    }
#ifdef LONGMODE_DEBUG
    cout << "\n";
#endif

    return Fbl;
}

int dwgs::getDel2()
{
    return del2;
}

/*

 | a0_1 --- del0 ---                 a0_2 | a0_3 --- del2 --- a0_4 --- fracDelayTop                            |
 |                                        H                                                                    |
 | a1_1 --- hammerDelay --- del1 --- a1_2 | a1_3  ... del3 ... dispersion + loss + fracDelayBottom --- a1_4 |
 */
