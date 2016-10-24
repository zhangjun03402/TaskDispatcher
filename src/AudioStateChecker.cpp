#include <math.h>
#include "..\inc\PublicHead.h"
//#include "..\inc\CommonFunc.h"
#include "..\inc\AudioStateChecker.h"
#include <iostream>
#include <algorithm>
using namespace std;


// mSTAL: how many frames are used in the statistics
AudioStateChecker::AudioStateChecker(int FS, int FFTL, int STAL)
{
	mFS 			= FS;
	mFFTL 			= FFTL;
	mFFTL2 			= FFTL / 2 + 1;
	mSTAL			= STAL;
	mHopSize		= FFTL >> 2;
	mLoBin			= round(50.0f / (float)mFS * (float)mFFTL) - 1;
	mHiBin			= round(3000.0f / (float)mFS * (float)mFFTL);
	mFFT            = new libFFT(mFFTL);
	GetSymHann( mWin, mFFTL );
}

AudioStateChecker::~AudioStateChecker()
{
	delete mFFT;
	mFFT = nullptr;
}

// r[0] 
int AudioStateChecker::PullResult(float* r)
{
	float *mu = new float[mFFTL2];
	if (nullptr == mu)
	{
		return -1;
	}

	float *sigma = new float[mFFTL2];
	if ( nullptr == sigma ) 
	{
		return -1;
	}
	// 10 * the percentage of the clipped non-overlapped frames within the stat window
	// > 10% would get the maximum output value of 10 * 10% = 100
	int clip = 0;
	for (int i = 0; i < mSTAL; i++)
		if (mClipDetBuf[i] > 0)
			clip++;
	r[0] = min(100.0f, ((float) clip / (float) mSTAL) * 1000.0f);

	// for all the frequency bands , calculate the standard variation of the log amp spec across the stat window
	for (int i = 0; i < mFFTL2; i++)
	{
		mu[i] = mean(mStatSpBuf[i], mSTAL);
		sigma[i] = stdvar(mStatLogSpBuf[i], mSTAL);
	}

	// find max
	float speech = 1e-10f;
	float noise = 1e-10f;
	for (int i = mLoBin; i < mHiBin; i++)
    {
		if (sigma[i] > 1.0f)
        {
			// speech part
			if (mu[i] > speech)
				speech = mu[i];
		}
        else
        {
			// stationary noise part
			if (mu[i] > noise)
				noise = mu[i];
		}
	}
	r[1] = (float) (20.0f * log10(speech / (float) mFFTL2)); // norm to [-90, -40]
	r[1] = max(0.0f, min(100.0f, (max(-90.0f, min(-40.0f, r[1])) + 90.0f) * 2.0f));
	r[2] = (float) (20.0f * log10(noise / (float) mFFTL2)); // norm to [-70, -40]
	r[2] = max(0.0f, min(100.0f, (max(-65.0f, min(-40.0f, r[2])) + 65.0f) * 4.0f));

	delete[] mu;
	delete[] sigma;
	mu    = nullptr;
	sigma = nullptr;
	_ASSERTE(_CrtCheckMemory());
	return 0;
}

void AudioStateChecker::FeedBuffer(float* x, int length)
{
	for (int i=0; i<length; i++)
    {
		mInputBuffer[mBufPointer++] = x[i] + (float) rand() * 1e-10f;			
		if (mBufPointer % mHopSize == 0)
        {
			// move data from input buffer to FFT buffer
			for (int j=0; j<mFFTL; j++)
            {
				mFFTRe[j] = mInputBuffer[(mBufPointer + j) % mFFTL];
			}
			Process();
		}			
		mBufPointer = mBufPointer % mFFTL;
	}
}

void AudioStateChecker::Process()
{
	// fill in energy buffer
	/*
	mStatEngBuf[mStatPointer] = 0.0f;
	for (int j=mFFTL-mHopSize; j<mFFTL; j++){
		mStatEngBuf[mStatPointer] += mFFTRe[j] * mFFTRe[j];
	}
	mStatEngBuf[mStatPointer] = (float) (10.0f * Math.log10(mStatEngBuf[mStatPointer] / mHopSize));
	*/
	
	// clipping detection
	mClipDetBuf[mStatPointer] = 0;
	for (int j=mFFTL-mHopSize; j<mFFTL; j++)
    {
		if (fabs(mFFTRe[j]) > 0.95f)
			mClipDetBuf[mStatPointer] ++;
	}
	
	// perform CFFT
	for (int j=0; j<mFFTL; j++)
    {
		mFFTRe[j] *= mWin[j];
		mFFTIm[j] = 0.0f;
	}

	mFFT->fft(mFFTRe, mFFTIm);
	
	// fill in statistic buffer (log amp spectrum)
	for (int j=0; j<mFFTL2; j++)
    {
		mStatSpBuf[j][mStatPointer] = (float) hypot(mFFTRe[j], mFFTIm[j]);
		mStatLogSpBuf[j][mStatPointer] = (float) log(mStatSpBuf[j][mStatPointer]);
	}
	mStatPointer = (mStatPointer + 1) % mSTAL;
}

void AudioStateChecker::GetSymHann(float* w, int length)
{
	int half;
	if (length % 2 != 1)
    {
		half = length / 2;
		Hann(w, half, length);
		for (int i = length - 1; i != half - 1; i--)
        {
			w[i] = w[length - i - 1];
		}
	}
    else
    {
		half = (length + 1) / 2;
		Hann(w, half, length);
		for (int i = length - 1; i != half - 1; i--)
        {
			w[i] = w[length - i - 1];
		}
	}
}

void AudioStateChecker::Hann(float* w, int mm, int nn)
{
	for (int i = 0; i != mm; i++)
    {
		w[i] = (float) (0.5 * (1.0 - cos(2.0 * M_PI * ((i + 1.0) / (nn + 1.0)))));
	}
}

float AudioStateChecker::stdvar(float* x, int len)
{
	float xbar = mean(x, len);
	float denom = 1.0f / ((float)len - 1.0f);
	float cum = 0.0f;
	//float r[] = new float[2];

	for (int i = 0; i != len; i++)
	{
		float t = x[i] - xbar;
		//x[i] = t * t;
		cum += (t*t);
	}
	//r[0] = xbar;

	//r[1] = (float) Math.sqrt(sum(x) * denom);
	//float temp = sum(x, len);
	return (float)sqrt(cum * denom);
}


float AudioStateChecker::sum(float* x, int len )
{
	float y = 0.0f;
	for (int i=0; i!=len; i++)
		y += x[i];
	return y;
}

float AudioStateChecker::mean(float* x, int len )
{
	float temp = sum(x,len);
	return temp/(float)len;
}

