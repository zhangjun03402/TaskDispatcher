#pragma once

class libFFT
{
private:
    int m_iN, m_iM; //type int varible

    /* Lookup tables. Only need to recompute when size of FFT changes. */
    float* m_cos;
    float* m_sin;

public:
	libFFT();
    libFFT( int n ); 
	~libFFT();

    void fft( float* x, float* y ); 
};
