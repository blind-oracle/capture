#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include "audio.h"
#include "../utils.h"
#include <lame/lame.h>

// (S)ALSA Stuff
static snd_pcm_t *handle;
static snd_pcm_hw_params_t *hw_params;
static int channels = 2;
static int frame_size;

#define BUFFER_SIZE 8192
#define PERIOD_SIZE 1024

int first = 1;
const snd_pcm_channel_area_t *mmap_areas; // mapped memory area info
snd_pcm_uframes_t mmap_offset, mmap_frames, mmap_size; // aux for frames count

short audio_buffer[PERIOD_SIZE * 2];
short buf_left[PERIOD_SIZE], buf_right[PERIOD_SIZE];

// Lame
static lame_global_flags *gfp;

int audio_init(char *device_name, unsigned int rate, u_int8_t n_channels) {
    channels = n_channels;
    frame_size = channels * sizeof(short);
    
    logger("AUDIO: Opening device...");
    CHECK(snd_pcm_open(&handle, device_name, SND_PCM_STREAM_CAPTURE, 0));
    logger("AUDIO: Allocating parameters 1...");
    CHECK(snd_pcm_hw_params_malloc(&hw_params));
    logger("AUDIO: Allocating parameters 2...");
    CHECK(snd_pcm_hw_params_any(handle, hw_params));
    logger("AUDIO: Setting access...");
    CHECK(snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
    logger("AUDIO: Setting format...");
    CHECK(snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE));
    logger("AUDIO: Setting rate to %u...", rate);
    CHECK(snd_pcm_hw_params_set_rate(handle, hw_params, rate, 0));
    logger("AUDIO: Setting channels to %u...", n_channels);
    CHECK(snd_pcm_hw_params_set_channels(handle, hw_params, n_channels));
    logger("AUDIO: Setting ringbuffer size to %d...", BUFFER_SIZE);
    CHECK(snd_pcm_hw_params_set_buffer_size(handle, hw_params, BUFFER_SIZE));
    logger("AUDIO: Setting period size to %d...", PERIOD_SIZE);
    CHECK(snd_pcm_hw_params_set_period_size(handle, hw_params, PERIOD_SIZE, 0));
    logger("AUDIO: Applying parameters...");
    CHECK(snd_pcm_hw_params(handle, hw_params));
    snd_pcm_hw_params_free(hw_params);
    CHECK(snd_pcm_prepare(handle));
    CHECK(snd_pcm_start(handle));
    
    logger("AUDIO: Initializing LAME...");
    gfp = lame_init();
    lame_set_num_channels(gfp, channels);
    lame_set_in_samplerate(gfp, rate);
    lame_set_brate(gfp, 256);
    lame_set_quality(gfp, 0); // 0..9
    CHECK(lame_init_params(gfp));
    
    logger("AUDIO: Successfully initialized");
    
    return 0;
}

static int audio_recovery(snd_pcm_t *handle, int error) {
    switch(error) {
	case -EPIPE:
	    logger("EPIPE");
	    
	    if(snd_pcm_prepare(handle) < 0) {
		logger("Buffer underrun cannot be recovered");
	    }
	    
	    return 0;
	break;
	
	case -ESTRPIPE:
	    logger("ESTRPIPE");
	    
	    while ((error = snd_pcm_resume(handle)) == -EAGAIN) {
		sleep(1); // wait until the suspend flag is clear
	    }
	    
	    if(error < 0) {
		if((error = snd_pcm_prepare(handle)) < 0) {
		    logger("Suspend cannot be recovered");
		}
	    }
	    
	    return 0;
	break;
    }
    
    return error;
}

int audio_read(unsigned char *mp3_buffer, int mp3_size) {
    int error;
    int state = snd_pcm_state(handle);
    
    switch(state) {
	case SND_PCM_STATE_XRUN:
	    logger("SND_PCM_STATE_XRUN");
	    audio_recovery(handle, -EPIPE);
	    first = 1;
	break;
	
	case SND_PCM_STATE_SUSPENDED:
	    logger("SND_PCM_STATE_SUSPENDED");
	    audio_recovery(handle, -ESTRPIPE);
	break;
    }
    
    error = snd_pcm_readi(handle, audio_buffer, PERIOD_SIZE);
    
    if(error < 0) {
	logger("AUDIO: Got error %d", error);
	
	if(audio_recovery(handle, error) < 0) {
	    logger("Unable to recover from error!");
	}
    } else {
	logger("Read %d frames %d", error, sizeof(mp3_buffer));
	error = lame_encode_buffer_interleaved(gfp, audio_buffer, error, mp3_buffer, mp3_size);
	logger("MP3 encoded %d", error);
    }
    
    return error;
}

int audio_uninit(void) {
    lame_close(gfp);
    snd_pcm_close(handle);
    
    logger("AUDIO: Uninitialized");
    return 0;
}
