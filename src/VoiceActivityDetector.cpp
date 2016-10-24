//#include "CommonFunc.h"
#include "..\inc\VoiceActivityDetector.h"

#include <iostream>
#include <string.h>
using namespace std;


/* Set threshold of voice activity (silence) level */
VoiceActivityDetector VoiceActivityDetector::SetLevelThreshold( int new_threshold )
{
    level_threshold = new_threshold; 
    return *this;
}

/* Set timeout required to trigger speech (transition from inactive to active state) */
VoiceActivityDetector VoiceActivityDetector::SetSpeechTimeout( int new_timeout )
{
    speech_timeout = new_timeout;
    return *this;
}

/* Set timeout required to trigger silence (transition from active to inactive state) */
VoiceActivityDetector VoiceActivityDetector::SetSilenceTimeout( int new_timeout )
{
    silence_timeout = new_timeout;
    return *this;
}

/* Set noinput timeout */
VoiceActivityDetector VoiceActivityDetector::SetNoInputTimeout( int new_timeout )
{
    noinput_timeout = new_timeout;
    return *this;
}

int VoiceActivityDetector::GetCurrentLevel()
{
    return level_current;
}

/* Reset activity detector */
void VoiceActivityDetector::Reset()
{
    StateChange( DETECTOR_STATE_INACTIVITY ); 
}

/* Create activity detector */
VoiceActivityDetector::VoiceActivityDetector()
{
}

VoiceActivityDetector::~VoiceActivityDetector()
{

}


/**
 * Process current frame, return detected event if any
 *
 * @param samples   a short integer array to hold the voice samples
 * //@param n_samples the length (number of samples) in array "samples"
 */
VADEvent VoiceActivityDetector::Process(short* samples,int len ) 
{
    VADEvent event = DETECTOR_EVENT_NONE;
    int n_samples = len;

    int level = 0;
    level_current = 0;

    level_current = level = LevelCalculate( samples, n_samples );

    switch ( vad_state )
    {
        case DETECTOR_STATE_INACTIVITY:
            if ( level >= level_threshold )
            {
            /* start to detect activity */
                StateChange(DETECTOR_STATE_ACTIVITY_TRANSITION);
            }
            else
            {
                vad_state_duration += n_samples;
                if (vad_state_duration >= noinput_timeout)
                {
                    event = DETECTOR_EVENT_NOINPUT;
                }
            }
            break;
        case DETECTOR_STATE_ACTIVITY_TRANSITION:
            if ( level >= level_threshold )
            {
                vad_state_duration += n_samples;
                if ( vad_state_duration >= speech_timeout )
                {
                    /* finally detected activity */
                    event = DETECTOR_EVENT_ACTIVITY;
                    StateChange( DETECTOR_STATE_ACTIVITY );
                }
            }
            else
            {
                /* fallback to inactivity */
                StateChange(DETECTOR_STATE_INACTIVITY);
            }
            break;
        case DETECTOR_STATE_ACTIVITY:
            if ( level >= level_threshold )
            {
                vad_state_duration += n_samples;
            }
            else
            {
			    /* start to detect inactivity */
                StateChange(DETECTOR_STATE_INACTIVITY_TRANSITION);
            }
            break;
        case DETECTOR_STATE_INACTIVITY_TRANSITION:
            if ( level >= level_threshold )
            {
			    /* fallback to activity */
                StateChange(DETECTOR_STATE_ACTIVITY);
            }
            else
            {
                vad_state_duration += n_samples;
                if ( vad_state_duration >= silence_timeout )
                {
				    /* detected inactivity */
                    event = DETECTOR_EVENT_INACTIVITY;
                    StateChange(DETECTOR_STATE_INACTIVITY);
                }
            }
            break;
    }
    return event;
}

short VoiceActivityDetector::getShort( char *buf, bool asc ) 
{
	size_t len = strlen( buf );
	
    if ( buf == nullptr )
    {
        //throw new IllegalArgumentException("char array is null!");
        cout<<"char array is null!"<<endl;
    }
    if (len > 2)
    {
        //throw new IllegalArgumentException("char array size > 2 !");
        cout<<"char array size > 2 !"<<endl;
    }

    short r = 0;
    if (asc)
        for (int i = len - 1; i >= 0; i--)
        {
            r <<= 8;
            r |= (buf[i] & 0x00ff);
        }
    else
        for (int i = 0; i < len; i++)
        {
            r <<= 8;
            r |= (buf[i] & 0x00ff);
        }
    return r;
}

/*************************************************************************** 
 * 需要在外部释放new出的header 
 *
 ***************************************************************************/
char *VoiceActivityDetector::getWaveFileHeader(long totalAudioLen) 
{
    long longSampleRate = 8000;
    int channels = 1;
    long byteRate = 16 * longSampleRate * channels / 8;
    long totalDataLen = totalAudioLen + 36;

    char* header = new char[44];
    header[0] = 'R'; // RIFF/WAVE header
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    header[4] = (char) (totalDataLen & 0xff);
    header[5] = (char) ((totalDataLen >> 8) & 0xff);
    header[6] = (char) ((totalDataLen >> 16) & 0xff);
    header[7] = (char) ((totalDataLen >> 24) & 0xff);
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f'; // 'fmt ' chunk
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    header[16] = 16; // 4 bytes: size of 'fmt ' chunk
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;
    header[20] = 1; // format = 1
    header[21] = 0;
    header[22] = (char) channels;
    header[23] = 0;
    header[24] = (char) (longSampleRate & 0xff);
    header[25] = (char) ((longSampleRate >> 8) & 0xff);
    header[26] = (char) ((longSampleRate >> 16) & 0xff);
    header[27] = (char) ((longSampleRate >> 24) & 0xff);
    header[28] = (char) (byteRate & 0xff);
    header[29] = (char) ((byteRate >> 8) & 0xff);
    header[30] = (char) ((byteRate >> 16) & 0xff);
    header[31] = (char) ((byteRate >> 24) & 0xff);
    header[32] = (char) (1 * 16 / 8); // block align
    header[33] = 0;
    header[34] = 16; // bits per sample
    header[35] = 0;
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    header[40] = (char) (totalAudioLen & 0xff);
    header[41] = (char) ((totalAudioLen >> 8) & 0xff);
    header[42] = (char) ((totalAudioLen >> 16) & 0xff);
    header[43] = (char) ((totalAudioLen >> 24) & 0xff);
    return header;
}

int VoiceActivityDetector::LevelCalculate( short *samples, int n_samples )
{
    int sum = 0;
    for( int i = 0; i < n_samples; i++ )
    {
        if( samples[i] < 0 )
        {
            sum -= samples[i];
        }
        else
        {
            sum += samples[i];
        }
    }
	if (n_samples==0)
	{
		return 0;
	}
    return sum / n_samples; 
}

void VoiceActivityDetector::StateChange( VADState new_state )
{
    vad_state = new_state;
    vad_state_duration = 0;
}

