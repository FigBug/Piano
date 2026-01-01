#include "hammer.h"
#include <math.h>
#include <stdio.h>
#include "utils.h"

enum {
    S = 3
};

Hammer::Hammer()
{
    x = 0;
    v = 0;
    a = 0.0;
    F = 0.0;
    upprev = 0;
    bEscaped = false;
    escapeDelay = 0;
    escapeCount = 0;
    dt = 0;
    dti = 0;
    mi = 0;
    K = 0;
    p = 0;
    alpha = 0;
    Z2i = 0;
}

void Hammer::set (float Fs_, float m_, float K_, float p_, float Z_, float alpha_, int escapeDelay_)
{
    escapeDelay = escapeDelay_;
    dt = 1.0f / (Fs_ * S);
    dti = 1.0f / dt;
    p = p_;
    K = K_;
    mi = 1.0f / m_;
    alpha = alpha_;
    Z2i = 1.0f / (2.0f * Z_);
}

Hammer::~Hammer()
{
}

void Hammer::strike (float v_)
{
    x = 0;
    v = v_;
    bEscaped = false;
    escapeCount = 0;
}

float Hammer::load (float vin0, float vin1)
{
    //static FILE *fp = fopen("hammer","w");
    if (bEscaped)
        return 0.0;
    
    float vin = vin0;
    float dvin = (vin1 - vin0) / S;
    //printf("hammer %g %g\n",vin0, vin1);
    for(int j=0;j<S;j++)
    {
        float up;
        up = (x>0)?pow((float)x,(float)p):0;
        float dupdt = (up-upprev)*dti;
        float v1;
        float x1;
        for(int k=0;k<S;k++)
        {
            F = K*(up+alpha*dupdt);
            if(F<=0)
            {
                F = 0;
            }       
            a = -F*mi;
            v1 = v + a * dt;
            x1 = x + (v1-(vin+F*Z2i)) * dt;
            float upnew = (x1>0)?pow((float)x1,(float)p):0;
            float dupdtnew = (upnew - upprev) * 0.5f * dti;
            float change = dupdtnew - dupdt;
            dupdt = dupdt + (float)0.5*change;
        } 
        vin += dvin;
        v = v1;
        x = x1;		
        upprev = up;
    }

    if (F == 0)
    {
        escapeCount++;
        if(escapeCount > escapeDelay)
        {
            bEscaped = true;
        }
    }
    else
    {
        escapeCount = 0;
    }
    //fprintf(fp,"%g\n",F);
    return F;
}

float Hammer::getX()
{
    return x;
}

bool Hammer::isEscaped()
{
    return bEscaped;
}
