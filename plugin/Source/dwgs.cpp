#include "dwgs.h"
#include <math.h>
#include <stdio.h>
#include "utils.h"
#include <stdlib.h>
using namespace std;


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

    if(n <= cur)
    {
        float *x1 = x + cur;
        memcpy (wave10-n, x1-n, size_t (n) * sizeof(float));
        /*
         for(int i=-1; i>=-n; i--) {
         wave10[i] = x1[i];
         }
         */
    }
    else
    {
        float *x1 = x + cur;
        memcpy(wave10-cur, x1-cur, size_t (cur) * sizeof(float));
        /*
         for(int i=-1; i>=-cur; i--) {
         wave10[i] = x1[i];
         }
         */

        int cur2 = cur + DelaySize;
        x1 = x + cur2;
        memcpy(wave10-n, x1-n, size_t (n-cur) * sizeof(float));
        /*
         for(int i=-cur-1; i>=-n; i--) {
         wave10[i] = x1[i];
         }
         */
    }

    /********* top *********/

    for(int j=0; j<4; j++) {

        x = d2.x;
        cur = (d2.cursor + DelaySize - delay - 4 + j + del4) % DelaySize;

        float* wave10 = wave0 + j;
#ifdef STRING_DEBUG
        for(int i=0; i<=delTab; i++)
            printf("%g ",wave10[i]);
        printf("\n");
#endif


        n = del0 + del2 + del4 + 5;
        if (n <= cur)
        {
            float *x1 = x + cur;
            ms4 (wave10, x1 - n + 1, wave, n);

            /*
             for(int i=0; i<n; i++) {
             wave1[i] = x1[-i];
             }
             */
        }
        else
        {
            float *x1 = x + cur;
            ms4(wave10,x1-cur,wave,cur+1);
            /*
             for(int i=0; i<=cur; i++) {
             wave1[i] = x1[-i];
             }
             */
            int cur2 = cur + DelaySize;
            x1 = x + cur2;

            int n4 = std::min((((cur+1)>>2)<<2) + 3,n);
            //cerr << "n4/n/cursor = " << n4 << "/" << n <<  "/" << cur << "\n";

            for(int i=cur+1; i<=n4; i++) {
                //wave1[i] = x1[-i];
                wave[i] = square(wave10[i] - x1[-i]);
                cur++;
            }

            if(cur + 1 < n) {
                ms4(wave10+cur+1,x1-n+1,wave+cur+1,n-cur-1);
                /*
                 for(int i=cur+1; i<n; i++) {
                 wave1[i] = x1[-i];
                 }
                 */
            }
        }

#ifdef STRING_DEBUG
        for(int i=0; i<=delTab; i++) {
            printf("%g ",wave1[i]);
        }
        printf("\n");
#endif

        diff4(wave, Fl, delTab+1);

#ifdef LONG_DEBUG
        for(int i=0; i<=delTab; i++) {
            printf("%g ",Fl[i+3]);
        }
        printf("\n");
#endif

        out[j] = 0;
        for(int k=1; k<=nLongModes; k++) {
            float *tab = modeTable[k];
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
    memset(modeTable,0,nMaxLongModes*sizeof(float*));
    c1 = 0;
    c3 = 0;
    nDamper = 0;

    vec4 z = {0};
    v0_2 = z;
    v1_3 = z;
    v0_4 = z;

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

    posix_memalign ((void**)&wave0, 32, size_t (delTab + 32) * sizeof(float));
    posix_memalign ((void**)&wave1, 32, size_t (delTab + 32) * sizeof(float));
    posix_memalign ((void**)&wave, 32, size_t (delTab + 32) * sizeof(float));
    posix_memalign ((void**)&Fl, 32, size_t (delTab + 32) * sizeof(float));
    memset (wave0, 0, size_t (delTab + 32) * sizeof(float));
    memset (wave1, 0, size_t(delTab + 32) * sizeof(float));
    memset (Fl, 0, size_t (delTab + 32) * sizeof(float));

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
			if (modeTable[k]) free_memalign(modeTable[k]);
            posix_memalign ((void**)&modeTable[k], 32, size_t (delTab + 8) * sizeof (float));

            for (int i = 0; i <= delTab; i++)
            {
                float d = i + delta;
                float s = sin(d*n);
                modeTable[k][i+3] = s;
            }
            //logf("maxd = %g/ %g\n",delTab+delta,delHalf);
            modeTable[k][0] = 0;
            modeTable[k][1] = 0;
            modeTable[k][2] = 0;
            modeTable[k][3] *= (0.5f + delta);
            modeTable[k][delTab+3] *= (0.5f + delta2);
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
    
    float *x = d2.x;
    int cur = (d2.cursor + DelaySize - delay + del4) % DelaySize;

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
