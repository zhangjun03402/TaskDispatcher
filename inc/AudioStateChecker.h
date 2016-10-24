#pragma once
#include "libFFT.h"

const int FS_LEN=8000;  
const int FFTL_LEN=512;
const int STAL_LEN=1875;


class AudioStateChecker
{
public:
    explicit AudioStateChecker( int FS, int FFTL, int STAL );
    ~AudioStateChecker();
    int PullResult( float* r );
    void FeedBuffer(float* x, int length);

private:
    int    mHopSize, mFFTL, mFFTL2, mSTAL, mFS, mLoBin, mHiBin; 
    int    mBufPointer     = 0;
    int    mStatPointer    = 0;
    float  mInputBuffer[FFTL_LEN] ;
    float  mStatSpBuf[FFTL_LEN/2+1][STAL_LEN]   ;
    float  mStatLogSpBuf[FFTL_LEN/2+1][STAL_LEN];
    int    mClipDetBuf[STAL_LEN];
    float  mFFTRe[FFTL_LEN]    ;
    float  mFFTIm[FFTL_LEN]    ;
    float  mWin  [FFTL_LEN]    ;
    libFFT* mFFT;

    void Process();
    void GetSymHann(float* w, int length);
    void Hann(float* w, int mm, int nn);
    float stdvar(float* x, int len );
    float sum(float* x, int len );
    float mean(float* x, int len );
};
