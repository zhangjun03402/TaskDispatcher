//package com.lingban.server.AudioStateChecker;
#include <math.h>
#include <iostream>
#include "..\inc\libFFT.h"
#include "..\inc\CommonFunc.h"
using namespace std;

libFFT::libFFT()
{

}

libFFT::libFFT( int n )
{
    m_iN = n;
    m_iM = (int) (log(m_iN) / log(2));

    // Make sure n is a power of 2
    if ( m_iN != (1 << m_iM) )
		cout<<"FFT length must be power of 2"<<endl;

    // pre-computed tables
    m_cos = new float[m_iN / 2];
    m_sin = new float[m_iN / 2];

    for ( int i = 0; i < m_iN / 2; i++ )
    {
        m_cos[i] = (float) cos( -2 * M_PI * i / m_iN );
        m_sin[i] = (float) sin( -2 * M_PI * i / m_iN );
    }
}

libFFT::~libFFT()
{
	delete [] m_cos;
	delete [] m_sin;

	m_cos = nullptr;
	m_sin = nullptr;
}

void libFFT::fft( float* x, float* y )
{
    int i, j, k, n1, n2, a;
    float c, s, t1, t2;

    // Bit-reverse
    j = 0;
    n2 = m_iN / 2;
    for (i = 1; i < m_iN - 1; i++)
    {
        n1 = n2;
        while ( j >= n1 )
        {
            j = j - n1;
            n1 = n1 / 2;
        }
        j = j + n1;

        if ( i < j )
        {
            t1 = x[i];
            x[i] = x[j];
            x[j] = t1;
            t1 = y[i];
            y[i] = y[j];
            y[j] = t1;
        }
    }

    // FFT
    n1 = 0;
    n2 = 1;

    for ( i = 0; i < m_iM; i++ )
    {
        n1 = n2;
        n2 = n2 + n2;
        a = 0;

        for (j = 0; j < n1; j++)
        {
            c = m_cos[a];
            s = m_sin[a];
            a += 1 << (m_iM - i - 1);

            for (k = j; k < m_iN; k = k + n2)
            {
                t1 = c * x[k + n1] - s * y[k + n1];
                t2 = s * x[k + n1] + c * y[k + n1];
                x[k + n1] = x[k] - t1;
                y[k + n1] = y[k] - t2;
                x[k] = x[k] + t1;
                y[k] = y[k] + t2;
            }
        }
    }
}
