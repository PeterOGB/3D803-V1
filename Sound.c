/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#include <math.h>
#include <alsa/asoundlib.h>
#include <gtk/gtk.h>

#include "Common.h"
#include "Sound.h"
#include "Wiring.h"
#include "Cpu.h"
#include "wg-definitions.h"


#define SOUNDDEBUG 0

extern GMutex SoundEffectsQueueMutex;
extern GSList  *runingSndEffects;

/* Various ALSA configuration values */
static snd_pcm_format_t soundSampleFormat = SND_PCM_FORMAT_S16_LE;
static unsigned int soundChannelCount = 2;   /* count of channels, stereo */
static const char *soundDevice = "default";
static snd_pcm_t *AlsaHandle = NULL;
int16_t *periodBuffer;                   /* Global buffer for effects samples */
static unsigned int FramesPerPeriod;
static unsigned int PeriodBufferSizeInBytes;
static int iFramesPerWordTime;
static int iWordTimesPerPeriod;

int callCount = 0;

// Alsa configurator !



void setDevice(char *deviceName)
{
    soundDevice = strdup(deviceName);
}


static int set_hwparamsV3(snd_pcm_t *handle,
			  snd_pcm_hw_params_t *params,
			  snd_pcm_access_t access,
			  unsigned int periods,
			  snd_pcm_uframes_t period_size,
			  unsigned int rate)

{
    unsigned int rrate;
    int err;

    /* choose all parameters */
    if((err = snd_pcm_hw_params_any(handle, params)) < 0) 
    {
	g_error("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
	return err;
    }
    /* set the interleaved read/write format */
    if((err = snd_pcm_hw_params_set_access(handle, params, access)) < 0) 
    {
	g_error("Access type not available for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the sample format */
    if((err = snd_pcm_hw_params_set_format(handle, params,  soundSampleFormat)) < 0) 
    {
	g_error("Sample format not available for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the count of channels */
    if((err = snd_pcm_hw_params_set_channels(handle, params, soundChannelCount)) < 0) 
    {
	g_error("Channels count (%i) not available for playbacks: %s\n",  soundChannelCount, snd_strerror(err));
	return err;
    }
    /* set the stream rate */
    rrate = rate;
    if((err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0)) < 0) 
    {
	g_error("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
	return err;
    }
    if (rrate != rate) {
	g_error("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
	return -EINVAL;
    }
    /* set the buffer size */

    if((err = snd_pcm_hw_params_set_buffer_size(handle, params, period_size * periods)) < 0) 
    {
	g_error("Unable to set buffer size %lu  for playback: %s\n", period_size * periods , snd_strerror(err));
	return err;
    }

    /* set the period size */

    if((err = snd_pcm_hw_params_set_period_size(handle, params, period_size, 0)) < 0) 
    {
	g_error("Unable to set period size %lu for playback: %s\n", period_size, snd_strerror(err));
	return err;
    }

    /* write the parameters to device */
    if((err = snd_pcm_hw_params(handle, params)) < 0) 
    {
	g_error("Unable to set hw params for playback: %s\n", snd_strerror(err));
	return err;
    }
    return 0;
}



static int set_swparamsV3(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams, snd_pcm_uframes_t PeriodSizeInFrames,int threshold)
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
	g_error("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, PeriodSizeInFrames *  (snd_pcm_uframes_t)threshold);
    if (err < 0) {
	g_error("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, PeriodSizeInFrames);
    if (err < 0) {
	g_error("Unable to set avail min for playback: %s\n", snd_strerror(err));
	return err;
    }

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
	g_error("Unable to set sw params for playback: %s\n", snd_strerror(err));
	return err;
    }
    return 0;
}

/* Always writes the first "period_size" bytes in the snd_buffer.
   Returns the number of "spare" samples */

extern unsigned char *snd_buffer_p;
int snd_buffer_size;

static snd_pcm_sframes_t xrun_recovery(snd_pcm_t *handle, snd_pcm_sframes_t err)
{
    g_info(" xrun_recovery \n");

    if (err == -EPIPE) {    /* under-run */
	err = snd_pcm_prepare(handle);
	if (err < 0)
	    g_error("Can't recovery from underrun, prepare failed: %s\n", snd_strerror((int)err));
	return 0;
    } else if (err == -ESTRPIPE) {
	while ((err = snd_pcm_resume(handle)) == -EAGAIN)
	    sleep(1);       /* wait until the suspend flag is released */
	if (err < 0) {
	    err = snd_pcm_prepare(handle);
	    if (err < 0)
		g_error("Can't recovery from suspend, prepare failed: %s\n", snd_strerror((int)err));
	}
	return 0;
    }
    return err;
}

/*
 * frameRate = sampling frequency
 * periodRate = How many times a second ALSA will ask for the next buffer.
 * periodCount = How many buffers ALSA will use
 */

static gboolean soundInitV3(snd_pcm_format_t SampleFormat,
			      unsigned int FrameRate,
			      int PeriodRate,
			      unsigned int PeriodCount)

{
    int BitsPerSample;
    double WordTime,ModifiedWordTime;
    double PeriodTime;
    double FramesPerWordTime;
    double  WordTimesPerPeriod ;
    int BytesPerFrame;
    int err;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_output_t *output = NULL;

    WordTime = 288.0E-6;
    PeriodTime =  1.0 / PeriodRate;
  
    BitsPerSample = snd_pcm_format_width(SampleFormat);
    BytesPerFrame = (((BitsPerSample-1)/8)+1) * (int) soundChannelCount;
    g_debug("BytesPerFrame=%d\n",BytesPerFrame);


    /*  FramesPerWordTime needs to be nearest integer value.  
	All wordtimes need to produce the same number of samples, 
	otherwise dynamic stops (for example) will sound noisey.
    */
    FramesPerWordTime = rint(WordTime * FrameRate);
    
    g_debug("FramesPerWordTime = %f\n",FramesPerWordTime);

    ModifiedWordTime = (FramesPerWordTime / FrameRate);
    g_debug("ModifiedWordTime = %f Changed by %f\n",ModifiedWordTime,
	   ModifiedWordTime/WordTime);

    FramesPerPeriod = (unsigned int)  ceil(FramesPerWordTime * (PeriodTime / ModifiedWordTime));
    g_debug("FramesPerPeriod = %d\n",FramesPerPeriod);

    WordTimesPerPeriod = ceil(PeriodTime / ModifiedWordTime); 
    iWordTimesPerPeriod = (int) WordTimesPerPeriod;
    g_debug("WordTimesPerPeriod = %f\n", WordTimesPerPeriod );

    iFramesPerWordTime = (int) FramesPerWordTime;

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
	g_error("Output failed: %s\n", snd_strerror(err));
	return FALSE;
    }

    g_debug("Playback device is %s\n", soundDevice);
    g_debug("Stream parameters are %iHz, %s, %i channels\n",
	   FrameRate, snd_pcm_format_name(SampleFormat), soundChannelCount);

    if ((err = snd_pcm_open(&AlsaHandle, soundDevice, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	g_error("Playback open error: %s\n", snd_strerror(err));
	return FALSE;
    }

    if ((err = set_hwparamsV3(AlsaHandle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED,
			      PeriodCount, 
			      FramesPerPeriod,
			      FrameRate)) < 0) {
	g_error("Setting of hwparams failed: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }
    if ((err = set_swparamsV3(AlsaHandle, swparams, FramesPerPeriod,3)) < 0) {
	g_error("Setting of swparams failed: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }

    // Needs space to hold any frames left over from previous emulation period.
    PeriodBufferSizeInBytes = ((FramesPerPeriod + (2 * (unsigned int) FramesPerWordTime)) *
			       (unsigned int)BytesPerFrame)  ;

    g_debug("PeriodBufferSizeInBytes=%d\n",PeriodBufferSizeInBytes);

    periodBuffer = (int16_t *) malloc(PeriodBufferSizeInBytes );

    bzero(periodBuffer, PeriodBufferSizeInBytes);

    g_debug("periodBufferSizeInBytes=%d\n",PeriodBufferSizeInBytes);

    return TRUE;
}

static int CPU_SampleCount;
static int16_t *CPU_Samples = NULL;

void addSamplesFromCPU(int16_t first,int16_t remainder)
{
    int16_t *samplePtr;
#if SOUNDDEBUG
    g_debug("%s called 0x%x 0x%x %d ",__FUNCTION__,first,remainder,CPU_SampleCount);
#endif
    if(CPU_Samples == NULL)
    {
	CPU_Samples = (int16_t *) malloc((size_t) PeriodBufferSizeInBytes);
	CPU_SampleCount = 0;
    }
    
    samplePtr = &CPU_Samples[CPU_SampleCount];

    *samplePtr++ = first;   // L
    *samplePtr++ = first;   // R

    for(int n = 1; n < iFramesPerWordTime; n++)
    {
	    *samplePtr++ = remainder;   // L
	    *samplePtr++ = remainder;   // R
    }

    CPU_SampleCount += (iFramesPerWordTime *2);

#if SOUNDDEBUG
    g_debug("%d\n",CPU_SampleCount);
#endif
}

static void addInCPU_Sound(void)
{
    int spareSamples;
    if(CPU_Samples != NULL)
    {
	for(int n = 0; n<(480*2); n++)
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
	    periodBuffer[n] += CPU_Samples[n];
#pragma GCC diagnostic pop
	}
    }
    spareSamples = (CPU_SampleCount - (480 * 2));

    for(int n = 0; n < spareSamples; n++)
    {
	CPU_Samples[n] = CPU_Samples[(480*2) + n];
    }

#if SOUNDDEBUG
    g_debug("%s CPU_SampleCount = %d spareSamples = %d\n",__FUNCTION__,CPU_SampleCount,spareSamples);
#endif    
    CPU_SampleCount = spareSamples;
}

//(*soundFunc)(periodBuffer,480,0.01,iWordTimesPerPeriod - wordTimesAdjustment);
static void keyboardSoundFunc(void *buffer, int sampleCount, 
			      __attribute__((unused)) double time, 
			      __attribute__((unused)) int wordtimes)
{
    GSList *effects,*next;
    struct sndEffect *effect;
    gint16 *dst,*src,sample;
    int n;

    effects = runingSndEffects; 

    while(effects != NULL )
    {
	effect = (struct sndEffect *) effects->data;

	if(effect->frameCount >= sampleCount)
	{
	    //g_debug("len=%d\n",effect->frameCount);
	    dst = buffer;
	    src = effect->frames;
	    n = sampleCount;
	    while(n--)
	    {
		sample = *src++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
		*dst++ +=  sample;
		*dst++ +=  sample;
#pragma GCC diagnostic pop
	    }
			    
	    // Update pointer adn counter
	    effect->frameCount -= sampleCount;
	    effect->frames =  src;
	    effects = g_slist_next(effects);
	}
	else
	{
	    dst = buffer;
	    src = effect->frames;
	    n = effect->frameCount;
	    while(n--)
	    {
		sample = *src++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
		*dst++ +=  sample;
		*dst++ +=  sample;
#pragma GCC diagnostic pop
	    }
	    // Effect finished so remove from list
	    next = g_slist_next(effects);
	    runingSndEffects = g_slist_remove_link(runingSndEffects,effects);
	    g_slist_free_1(effects);
	    free(effect);
	    effects = next;
	}
    }
}

static void DoSoundStuff(void)
{
    ButtonEvent *be;
    static unsigned int F1bits = 0,N1bits = 0,F2bits = 0,N2bits = 0;
    gboolean F1changed = FALSE;
    gboolean N1changed = FALSE;
    gboolean F2changed = FALSE;
    gboolean N2changed = FALSE;

    static int wordTimesAdjustment = 0;
    
    bzero(periodBuffer,PeriodBufferSizeInBytes);
    
    g_mutex_lock(&SoundEffectsQueueMutex);
    keyboardSoundFunc(periodBuffer, 480, 0.0,0);
    g_mutex_unlock(&SoundEffectsQueueMutex);

    // Pull events of the button event queue and set variables/wires accordingly
    if(ButtonEventQueue != NULL)
    {	
	while((be = (ButtonEvent *) g_async_queue_try_pop (ButtonEventQueue)) != NULL)
	{
	    g_debug("Poped %d %d %d\n",be->press,be->rowId,be->value);
	    switch(be->rowId)
	    {
	    case F1:
		if(be->press)
		    F1bits |=  be->value;
		else
		    F1bits &= ~be->value;
		F1changed = TRUE;
		break;

	    case N1:
		if(be->press)
		    N1bits |=  be->value;
		else
		    N1bits &= ~be->value;
		N1changed = TRUE;
		break;

	    case F2:
		if(be->press)
		    F2bits |=  be->value;
		else
		    F2bits &= ~be->value;
		F2changed = TRUE;		    
		break;
		    
	    case N2:
		if(be->press)
		    N2bits |=  be->value;
		else
		    N2bits &= ~be->value;
		N2changed = TRUE;	    
		break;

	    case OPERATE:
		wiring(OPERATEWIRE,be->press ? 1 : 0);
		break;

	    case RON: 
		if(be->press)
		    wiring(RONWIRES,be->value);
		break;

	    case CLEARSTORE:
		wiring(CSWIRE,be->press ? 1 : 0);		    
		break;

	    case MANUALDATA:
		wiring(MDWIRE,be->press ? 1 : 0);		    
		break;

	    case RESET:
		wiring(RESETWIRE,be->press ? 1 : 0);
		break;
		    
	    case BATOFF:
		wiring(BATTERY_OFF_PRESSED,1);
		break;

	    case BATON:
		wiring(BATTERY_ON_PRESSED,1);
		break;
		    
	    case CPUOFF:
		wiring(COMPUTER_OFF_PRESSED,1);
		break;
		    
	    case CPUON:
		wiring(COMPUTER_ON_PRESSED,1);
		break;

	    case SELECTEDSTOP:
		wiring(SSWIRE,be->press ? 1 : 0);
		break;

	    case VOLUME:
		wiring(VOLUME_CONTROL,be->value);
		break;
		    
	    default:
		break;
	    }
	    free(be);
	}
    }

    // Only update wires when needed
    if(F1changed) wiring(F1WIRES,F1bits);
    if(N1changed) wiring(N1WIRES,N1bits);
    if(F2changed) wiring(F2WIRES,F2bits);
    if(N2changed) wiring(N2WIRES,N2bits);

    wiring(TIMER100HZ,0);

    // This does ~14 word times of emulation.
    CPU_sound(periodBuffer,480,0.01,iWordTimesPerPeriod - wordTimesAdjustment);
    
    addInCPU_Sound();
    wordTimesAdjustment = (CPU_SampleCount/2)  / iFramesPerWordTime;
}

// Taken from Alsa demo code 

static int wait_for_poll(snd_pcm_t *handle, struct pollfd *ufds, unsigned int count)
{
    unsigned short revents;
    while (1) {
        poll(ufds, count, -1);
        snd_pcm_poll_descriptors_revents(handle, ufds, count, &revents);
        if (revents & POLLERR)
            return -EIO;
        if (revents & POLLOUT)
            return 0;
    }
}

static int write_and_poll_loop(snd_pcm_t *handle)
{
    struct pollfd *ufds;
    signed short *ptr;
    int err, count, init;
    unsigned int fdCount,frameCount;
    count = snd_pcm_poll_descriptors_count (handle);
    if (count <= 0) {
        g_error("Invalid poll descriptors count\n");
        return count;
    }
    fdCount = (unsigned int) count;
    ufds = malloc( fdCount * sizeof(struct pollfd));

    if (ufds == NULL) {
        g_error("No enough memory\n");
        return -ENOMEM;
    }

    if ((err = snd_pcm_poll_descriptors(handle, ufds, fdCount)) < 0) {
        g_error("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
        return err;
    }

    init = 1;
    while (Running) {
        if (!init) {
            err = (int) wait_for_poll(handle, ufds, fdCount);
            if (err < 0) {
                if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ||
                    snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED)
		{
                    err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ? -EPIPE : -ESTRPIPE;
                    if (xrun_recovery(handle, err) < 0)
		    {
                        g_warning("Write error: %s\n", snd_strerror(err));
                        exit(EXIT_FAILURE);
                    }
                    init = 1;
                }
		else
		{
                    g_warning("Wait for poll failed\n");
                    return err;
                }
            }
        }

	DoSoundStuff();
	callCount += 1;

        ptr = periodBuffer;
        frameCount = FramesPerPeriod;
        while (frameCount > 0)
	{
            err = (int) snd_pcm_writei(handle, ptr, frameCount);
            if (err < 0)
	    {
                if (xrun_recovery(handle, err) < 0)
		{
                    g_warning("Write error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                init = 1;
                break;  /* skip one period */
            }
            if (snd_pcm_state(handle) == SND_PCM_STATE_RUNNING)
                init = 0;
            ptr += (unsigned int) err * soundChannelCount;
            frameCount -= (unsigned int) err;
            if (frameCount == 0)
                break;
            /* it is possible, that the initial buffer cannot store */
            /* all data from the last period, so wait awhile */
	    
            err = wait_for_poll(handle, ufds, fdCount);
            if (err < 0)
	    {
                if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ||
                    snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED) {
                    err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ? -EPIPE : -ESTRPIPE;
                    if (xrun_recovery(handle, err) < 0)
		    {
                        printf("Write error: %s\n", snd_strerror(err));
                        exit(EXIT_FAILURE);
                    }
                    init = 1;
                }
		else
		{
                    g_warning("Wait for poll failed\n");
                    return err;
                }
            }
        }
    }
    return 0;
}


// This is the emulation thread !

gpointer worker(__attribute__((unused)) gpointer data)
{
    int err;
    
    soundInitV3(SND_PCM_FORMAT_S16_LE,48000,100,4);

    // This is where all the emulation happens !
    err =  write_and_poll_loop(AlsaHandle);
    if (err < 0)
        g_warning("Transfer failed: %s\n", snd_strerror(err));
    snd_pcm_close(AlsaHandle);

    g_info("Worker finished!\n");
    return(NULL);
}




