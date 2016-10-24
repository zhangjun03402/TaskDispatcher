#pragma once

enum VADEvent
{
    DETECTOR_EVENT_NONE,       /* no event occurred */
    DETECTOR_EVENT_ACTIVITY,   /* voice activity (transition to activity from inactivity state) */
    DETECTOR_EVENT_INACTIVITY, /* voice inactivity (transition to inactivity from activity state) */
    DETECTOR_EVENT_NOINPUT     /* no input event occurred */          
};



class VoiceActivityDetector
{
public:

    /* Create activity detector */
    VoiceActivityDetector();

    ~VoiceActivityDetector();

    /**
     * Set threshold of voice activity (silence) level
     */
    VoiceActivityDetector SetLevelThreshold(int new_threshold); 

    /**
     * Set timeout required to trigger speech (transition from inactive to active state)
     */
    VoiceActivityDetector SetSpeechTimeout(int new_timeout);  

    /**
     * Set timeout required to trigger silence (transition from active to inactive state)
     */
    VoiceActivityDetector SetSilenceTimeout(int new_timeout); 

    /**
     * Set noinput timeout
     */
    VoiceActivityDetector SetNoInputTimeout(int new_timeout); 

    int GetCurrentLevel(); 

    /**
     * Reset activity detector
     */
    void Reset(); 

     /**
     * Process current frame, return detected event if any
     *
     * @param samples   a short integer array to hold the voice samples
     * //@param n_samples the length (number of samples) in array "samples"
     */
    VADEvent Process(short* samples,int len );

    virtual short getShort( char* buf, bool asc ) final; 
    static char* getWaveFileHeader( long totalAudioLen );



private:
        /* enumerating the event of voice activity detection */
        enum VADState {
            DETECTOR_STATE_INACTIVITY,           /* inactivity detected */
            DETECTOR_STATE_ACTIVITY_TRANSITION,  /* activity detection is in-progress */
            DETECTOR_STATE_ACTIVITY,             /* activity detected */
            DETECTOR_STATE_INACTIVITY_TRANSITION /* inactivity detection is in-progress */
        };

        int LevelCalculate(short* samples, int n_samples); 

        void StateChange(VADState new_state); 

        /*current vad state*/
        VADState vad_state = DETECTOR_STATE_INACTIVITY;

        /* duration spent in current state  */
        int vad_state_duration = 0;

        /* voice activity (silence) level threshold */
		int level_threshold = 50;// 300;

        int level_current = 0;

        /* period of activity required to complete transition to active state */
        int speech_timeout = 200 * 16;

        /* period of inactivity required to complete transition to inactive state */
        int silence_timeout = 200 * 16;

        /* noinput timeout */
        int noinput_timeout = 5000 * 16;

};
