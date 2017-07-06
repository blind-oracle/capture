#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <sched.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "confuse.h"
#include "utils.h"

#ifdef HTTPD
#include "httpd/httpd.h"
#endif

#ifdef JPEG
#include "draw_text.h"
#include "jpeg/jpeg_utils.h"
#endif

#ifdef MKV
#include "mkv/mkv_utils.h"
#endif

#include "v4l2/v4l2.h"

#ifdef GPS
#include "gpsd.h"
#include "gps.h"
#endif

#ifdef SQLITE
#include "db.h"
#endif

#ifdef AUDIO
#include "audio/audio.h"
#endif

char compiled_options[64];

time_t rawtime;
struct tm *tinfo;

unsigned int prev_sec = 0, frame_count = 0, frame_count_sec = 0, text_dist = 0;
struct s_conf conf;

#ifdef HTTPD
extern int httpd_clients;

// JPEG thread stuff
extern int jpeg_thread_busy;
extern unsigned char *jpeg_thread_buffer;
extern int jpeg_thread_buffer_size;
extern pthread_mutex_t mutex_jpeg_frame;
extern pthread_cond_t cond_jpeg_frame;
#endif

#ifdef GPS
struct gps_data_t *_gpsd_data = NULL;
char *gps_fix;
char *gps_fix_mode;
int gps_fix_acq = 0;
#endif

float measure_fps = 0;
u_int64_t measure_start = 0, frame_duration = 0;
unsigned int measure_frames = 0;

#ifdef MKV
struct _write_buffers *write_buffers;

pthread_t t_write_thread;
unsigned int newfile = 0;
char filename[256], subtitle_str[256];
unsigned long file_start_timestamp = 0;
int closefile = 0;
unsigned long frames_dropped = 0;
#endif

#ifdef AUDIO
unsigned char mp3_buffer[2 * 2048 + 7200];
int mp3_size = 0;
#endif

char *config_file = "capture.conf";

int jpeg_size = 0;
unsigned char *jpeg_buffer = NULL;

int _shutdown = 0;

#ifdef JPEG
unsigned char *yuv420_buffer = NULL;
unsigned char *yuv422_buffer = NULL;
int yuv420_buffer_size = 0;
int yuv422_buffer_size = 0;
#endif

static void gettime(void) {
    time(&rawtime);
    tinfo = localtime(&rawtime);
    
    tinfo->tm_year += 1900;
    tinfo->tm_mon += 1;
}

#ifdef MKV
static void mkfilename(void) {
    char date_dir[64];
    char time_dir[64];
    
    gettime();
    
    CLEAR(filename);
    CLEAR(date_dir);
    CLEAR(time_dir);
    
    sprintf(date_dir, "%s/%d-%.2d-%.2d", conf.video_dir, tinfo->tm_year, tinfo->tm_mon, tinfo->tm_mday);
    
    if(!file_exists(date_dir)) {
	mkdir(date_dir, 0666);
    }
    
    sprintf(time_dir, "%s/%.2d", date_dir, tinfo->tm_hour);
    
    if(!file_exists(time_dir)) {
	mkdir(time_dir, 0666);
    }
    
    sprintf(filename, "%s/%.2d-%.2d-%.2d.mkv", time_dir, tinfo->tm_hour, tinfo->tm_min, tinfo->tm_sec);
    
    if(file_exists(filename)) {
	logger("CORE: File '%s' already exists, aborting...", filename);
	exit(EXIT_FAILURE);
    }
}

static int get_free_buffer(void) {
    int i = 0;
    
    for(i = 0; i < conf.write_buffer_count; i++) {
	if(write_buffers[i].free) {
	    return i;
	}
    }
    
    return -1;
}

static int get_filled_buffer(void) {
    int i = 0;
    
    for(i = 0; i < conf.write_buffer_count; i++) {
	if(!write_buffers[i].free) {
	    return i;
	}
    }
    
    return -1;
}

static void *write_thread(void *arg) {
    int i = 0;
    
    write_buffers = malloc(sizeof(struct _write_buffers) * conf.write_buffer_count);
    
    if(write_buffers == NULL) {
        logger("WRITE_THREAD: Unable to allocate memory for write buffers");
        exit(1);
    }
    
    for(i = 0; i < conf.write_buffer_count; i++) {
	write_buffers[i].free = 1;
	write_buffers[i].ptr = malloc(conf.jpeg_buffer_size);
	
	if(write_buffers[i].ptr == NULL) {
	    logger("WRITE_THREAD: Cannot allocate JPEG buffer %d", i);
	    exit(1);
	}
    }
    
    logger("WRITE_THREAD: %d buffers initialized", conf.write_buffer_count);
    
    mkfilename();
    mkv_open_file(filename);
    file_start_timestamp = get_timestamp_int64();
    
    while(!_shutdown) {
	while(1) {
	    i = get_filled_buffer();
	    
	    if(i >= 0) {
		if((((tinfo->tm_min % 10 == 0) && tinfo->tm_sec == 0) || closefile) && !newfile) {
		    newfile = 1;
		    
		    mkv_close_file(frame_duration);
		    
		    logger("Total size of video files: %.2f MB", (double)(sqlite_get_total_size()) / 1048576.0f);
		    
		    mkfilename();
		    mkv_open_file(filename);
		    
		    file_start_timestamp = get_timestamp_int64();
		    
		    closefile = 0;
		} else if(tinfo->tm_sec > 0 && newfile) {
		    newfile = 0;
		}
		
		#ifdef DEBUG
		logger("WRITE_THREAD: Got filled buffer %d", i);
		#endif
		
		mkv_write_frame(i);
		
		write_buffers[i].free = 1;
	    } else {
		break;
	    }
	}
	
	usleep(1000);
    }
    
    logger("WRITE_THREAD: Exiting...");
    
    mkv_close_file(frame_duration);
    
    for(i = 0; i < conf.write_buffer_count; i++) {
        free(write_buffers[i].ptr);
    }
    
    free(write_buffers);
    
    logger("WRITE_THREAD: %d buffers freed, exiting...", conf.write_buffer_count);
    
    pthread_exit(NULL);
}
#endif

#ifdef HTTPD
static void update_jpeg_thread (void) {
    if(jpeg_thread_busy == 0) {
	memcpy(jpeg_thread_buffer, jpeg_buffer, jpeg_size);
	jpeg_thread_buffer_size = jpeg_size;
	
	pthread_mutex_lock(&mutex_jpeg_frame);
	pthread_cond_signal(&cond_jpeg_frame);
	pthread_mutex_unlock(&mutex_jpeg_frame);
    } else {
	logger("CORE: JPEG thread busy, skipping update");
    }
}
#endif

static void main_loop (void) {
    char text_str[256];
    
    #ifdef GPS
    char text_gps_1[256];
    char text_gps_2[256];
    char text_gps_3[256];
    char text_gps_4[256];
    char text_gps_5[256];
    char text_gps_6[256];
    #endif
    
    double duration = 0;
    
    #ifdef JPEG
    if(conf.text_double) {
	text_dist = 20;
    } else {
	text_dist = 10;
    }
    #endif
    
    measure_start = get_timestamp_int64();
    
    while(!_shutdown) {
	#ifdef JPEG
	if(conf.format >= 1) {
	#endif
	    frame_duration = get_timestamp_int64();
	    jpeg_size = v4l2_read_frame(jpeg_buffer);
	    frame_duration = (get_timestamp_int64() - frame_duration) / 1e3;
	    
	    #ifdef DEBUG
	    logger("CORE: Got JPEG size %d", jpeg_size);
	    #endif
	    
	    #ifdef JPEG
	    if(conf.format == 1) {
		mjpegtoyuv420p(yuv420_buffer, jpeg_buffer, jpeg_size);
	    }
	    #endif
	#ifdef JPEG
	}
	else if(conf.format == 0) {
	    v4l2_read_frame(yuv422_buffer);
	    convert_yuv422_to_420p(yuv420_buffer, yuv422_buffer, conf.width, conf.height);
	    
	    #ifdef FFMPEG
	    if(conf.deinterlace) {
		deinterlace_420p(yuv420_buffer, conf.width, conf.height);
	    }
	    #endif
	}
	#endif
	
	#ifdef MKV
	if(conf.timelapse) {
	    usleep(conf.timelapse * 100000);
	}
	#endif
	
	#ifdef AUDIO
	if(conf.audio_enabled) {
	    mp3_size = audio_read(mp3_buffer, mp3_size);
	    
	    if(mp3_size < 0) {
		logger("CORE: Unable to encode audio frame to mp3");
		exit(1);
	    }
	}
	#endif
	
	frame_count_sec++;
	frame_count++;
	measure_frames++;
	
	// Get & format time
	gettime();
	CLEAR(text_str);
	
	if(tinfo->tm_sec != prev_sec) {
	    frame_count_sec = 0;
	    prev_sec = tinfo->tm_sec;
	}
	
	if(measure_frames >= (conf.fps * 2)) {
	    duration = (get_timestamp_int64() - measure_start) / 1e3;
	    
	    if(duration > 0) {
		measure_fps = (float)(measure_frames) / (float)(duration) * 1e3;
	    } else {
		logger("CORE: Error! Duration <= 0, division by zero, skipping!");
	    }
	    
	    measure_frames = 0;
	    measure_start = get_timestamp_int64();
	}
	
	#ifdef GPS
	if(conf.gpsd_use) {
	    _gpsd_data = gpsd_update();
	}
	
	CLEAR(text_gps_1);
	
	if(_gpsd_data != NULL) {
	    switch(_gpsd_data->status) {
		case STATUS_NO_FIX:
		    gps_fix = "NO";
		break;
		case STATUS_FIX:
		    gps_fix = "GPS";
		break;
		case STATUS_DGPS_FIX:
		    gps_fix = "DGPS";
		break;
	    }
	    
	    switch(_gpsd_data->fix.mode) {
		case MODE_NOT_SEEN:
		case MODE_NO_FIX:
		    gps_fix_mode = "";
		break;
		case MODE_2D:
		    gps_fix_mode = "2D ";
		break;
		case MODE_3D:
		    gps_fix_mode = "3D ";
		break;
	    }
	    
	    sprintf(text_gps_1, "SAT: %d/%d (FIX: %s%s)",
		_gpsd_data->satellites_used,
		_gpsd_data->satellites_visible,
		gps_fix_mode,
		gps_fix
	    );
	    
	    if(_gpsd_data->fix.mode != MODE_NOT_SEEN && _gpsd_data->fix.mode != MODE_NO_FIX) {
		CLEAR(text_gps_2);
		CLEAR(text_gps_3);
		CLEAR(text_gps_4);
		CLEAR(text_gps_5);
		CLEAR(text_gps_6);
		
		sprintf(text_gps_2, "LAT: %.9f", _gpsd_data->fix.latitude);
		sprintf(text_gps_3, "LON: %.9f", _gpsd_data->fix.longitude);
		sprintf(text_gps_4, "ALT: %.2f m", _gpsd_data->fix.altitude);
		sprintf(text_gps_5, "SPD: %.2f km/h", _gpsd_data->fix.speed * 3.6f);
		sprintf(text_gps_6, "HEAD: %.2f", _gpsd_data->fix.track);
		
		gps_fix_acq = 1;
	    } else {
		gps_fix_acq = 0;
	    }
	    
	    #ifdef JPEG
	    if(conf.format <= 1) {
		if(conf.osd) {
		    draw_text(yuv420_buffer, 10, 10 + text_dist,   conf.width, text_gps_1, conf.text_double);
		    draw_text(yuv420_buffer, 10, 10 + text_dist*2, conf.width, text_gps_2, conf.text_double);
		    draw_text(yuv420_buffer, 10, 10 + text_dist*3, conf.width, text_gps_3, conf.text_double);
		    draw_text(yuv420_buffer, 10, 10 + text_dist*4, conf.width, text_gps_4, conf.text_double);
		    draw_text(yuv420_buffer, 10, 10 + text_dist*5, conf.width, text_gps_5, conf.text_double);
		    draw_text(yuv420_buffer, 10, 10 + text_dist*6, conf.width, text_gps_6, conf.text_double);
		}
	    }
	    #endif
	} else {
	    sprintf(text_gps_1, "NO GPS CONNECTION");
	    
	    #ifdef JPEG
	    if(conf.osd) {
		draw_text(yuv420_buffer, 10, 10 + text_dist, conf.width, text_gps_1, conf.text_double);
	    }
	    #endif
	}
	#endif
	
	sprintf(text_str,
	    "%d-%.2d-%.2d %.2d:%.2d:%.2d-%.2d FPS: %.2f",
	    tinfo->tm_year,
	    tinfo->tm_mon,
	    tinfo->tm_mday,
	    tinfo->tm_hour,
	    tinfo->tm_min,
	    tinfo->tm_sec,
	    frame_count_sec,
	    measure_fps
	);
	
	#ifdef JPEG
	if(conf.format <= 1) {
	    if(conf.osd) {
		draw_text(yuv420_buffer, 10, 10, conf.width, text_str, conf.text_double);
	    }
	    
	    #ifdef DEBUG
	    logger("CORE: About to encode to JPEG");
	    #endif
	    
	    jpeg_size = encode_jpeg();
	}
	#endif
	
	#ifdef MKV
	if(conf.subtitle_enabled) {
	    CLEAR(subtitle_str);
	    sprintf(subtitle_str, "%s", text_str);
	    
	    #ifdef GPS
	    if(conf.gpsd_use) {
		if(gps_fix_acq) {
		    sprintf(subtitle_str + strlen(subtitle_str), "\n%s %s\n%s %s %s", text_gps_2, text_gps_3, text_gps_4, text_gps_5, text_gps_6);
		} else {
		    sprintf(subtitle_str + strlen(subtitle_str), "\nNO GPS FIX");
		}
	    }
	    #endif
	}
	#endif
	
	if(jpeg_size > 0) {
	    #ifdef HTTPD
	    if(httpd_clients > 0) {
		update_jpeg_thread();
	    }
	    #endif
	    
	    #ifdef MKV
	    if(conf.file_use) {
		int i = get_free_buffer();
		if(i >= 0) {
		    #ifdef DEBUG
		    logger("CORE: Got free buffer %d", i);
		    #endif
		    
		    memcpy(write_buffers[i].ptr, jpeg_buffer, jpeg_size);
		    CLEAR(write_buffers[i].subtitle);
		    strncpy(write_buffers[i].subtitle, subtitle_str, strlen(subtitle_str));
		    
		    write_buffers[i].frame_duration = frame_duration;
		    write_buffers[i].ptr_size = jpeg_size;
		    write_buffers[i].timestamp = get_timestamp_int64();
		    
		    write_buffers[i].free = 0;
		} else {
		    logger("CORE: No free buffers, dropping frame %d", frame_count);
		    frames_dropped++;
		}
	    }
	    #endif
	} else {
	    logger("CORE: JPEG size for frame %d is zero", frame_count);
	}
    }
}

static void sig_handler(int signum) {
    #ifdef MKV
    if(signum == SIGHUP) {
	logger("CORE: Got SIGHUP, closing current file and opening new one...");
	closefile = 1;
    } else {
    #endif
	logger("CORE: Got signal %d, raising up shutdown", signum);
	_shutdown = 1;
    #ifdef MKV
    }
    #endif
}

static void parse_config(char *config_file) {
    CLEAR(compiled_options);
    
    cfg_opt_t opts[] = {
	CFG_STR("video_device", "/dev/video0", CFGF_NONE),
	CFG_INT("video_input", 0, CFGF_NONE),
	CFG_INT("width", 640, CFGF_NONE),
	CFG_INT("height", 480, CFGF_NONE),
	CFG_INT("jpeg_buffer_size", 1048576, CFGF_NONE),
	#ifdef MKV
	CFG_INT("sync_interval", 10, CFGF_NONE),
	CFG_INT("timelapse", 0, CFGF_NONE),
	CFG_STR("file_dir", ".", CFGF_NONE),
	CFG_INT("file_use", 0, CFGF_NONE),
	CFG_INT("maximum_size", 10240, CFGF_NONE), // 10GB
	CFG_INT("subtitle_enabled", 0, CFGF_NONE),
	CFG_INT("write_buffer_count", 10, CFGF_NONE),
	#endif
	CFG_INT("fps", 30, CFGF_NONE),
	#ifdef JPEG
	CFG_INT("text_double", 1, CFGF_NONE),
	CFG_INT("jpeg_quality", 75, CFGF_NONE),
	CFG_INT("format", 1, CFGF_NONE),
	CFG_INT("osd", 1, CFGF_NONE),
	#endif
	#ifdef FFMPEG
	CFG_INT("deinterlace", 0, CFGF_NONE),
	#endif
	#ifdef HTTPD
	CFG_INT("httpd_mjpeg_port", 6666, CFGF_NONE),
	CFG_INT("httpd_jpeg_port", 6667, CFGF_NONE),
	#endif
	#ifdef GPS
	CFG_STR("gpsd_host", "127.0.0.1", CFGF_NONE),
	CFG_STR("gpsd_port", "2947", CFGF_NONE),
	CFG_INT("gpsd_use", 0, CFGF_NONE),
	#endif
	#ifdef AUDIO
	CFG_INT("audio_enabled", 1, CFGF_NONE),
	CFG_STR("audio_device", "default", CFGF_NONE),
	CFG_INT("audio_channels", 1, CFGF_NONE),
	CFG_INT("audio_rate", 44100, CFGF_NONE),
	#endif
	#ifdef SQLITE
	CFG_STR("db_path", "capture.sqlite", CFGF_NONE),
	#endif
	CFG_END()
    };
    
    if(!file_exists(config_file)) {
	logger("Config file '%s' not found, defaults will be used");
    }
    
    cfg_t *cfg;
    cfg = cfg_init(opts, CFGF_NONE);
    if(cfg_parse(cfg, config_file) == CFG_PARSE_ERROR) {
	logger("CORE: Config file '%s' parsing error!", config_file);
	exit(EXIT_FAILURE);
    } else {
	logger("CORE: Configuration loaded");
    }
    
    conf.video_device = cfg_getstr(cfg, "video_device");
    
    #ifdef SQLITE
    sprintf(compiled_options, "SQLITE ");
    
    conf.db_path = cfg_getstr(cfg, "db_path");
    #endif
    
    conf.input = cfg_getint(cfg, "video_input");
    conf.width = cfg_getint(cfg, "width");
    conf.height = cfg_getint(cfg, "height");
    conf.fps = cfg_getint(cfg, "fps");
    conf.jpeg_buffer_size = cfg_getint(cfg, "jpeg_buffer_size");
    
    #ifdef MKV
    sprintf(compiled_options + strlen(compiled_options), "MKV ");
    
    conf.sync_interval = cfg_getint(cfg, "sync_interval");
    conf.timelapse = cfg_getint(cfg, "timelapse");
    conf.subtitle_enabled = cfg_getint(cfg, "subtitle_enabled");
    conf.maximum_size = (u_int64_t)(cfg_getint(cfg, "maximum_size")) * 1048576;
    conf.video_dir = cfg_getstr(cfg, "file_dir");
    conf.file_use = cfg_getint(cfg, "file_use");
    conf.subtitle_enabled = cfg_getint(cfg, "subtitle_enabled");
    conf.write_buffer_count = cfg_getint(cfg, "write_buffer_count");
    #endif
    
    #ifdef FFMPEG
    sprintf(compiled_options + strlen(compiled_options), "FFMPEG ");
    
    conf.deinterlace = cfg_getint(cfg, "deinterlace");
    #endif
    
    #ifdef JPEG
    sprintf(compiled_options + strlen(compiled_options), "JPEG ");
    
    conf.jpeg_quality = cfg_getint(cfg, "jpeg_quality");
    conf.format = cfg_getint(cfg, "format");
    conf.text_double = cfg_getint(cfg, "text_double");
    conf.osd = cfg_getint(cfg, "osd");
    #endif
    
    #ifdef HTTPD
    sprintf(compiled_options + strlen(compiled_options), "HTTPD ");
    
    conf.httpd_mjpeg_port = cfg_getint(cfg, "httpd_mjpeg_port");
    conf.httpd_jpeg_port = cfg_getint(cfg, "httpd_jpeg_port");
    #endif
    
    #ifdef GPS
    sprintf(compiled_options + strlen(compiled_options), "GPS ");
    
    conf.gpsd_host = cfg_getstr(cfg, "gpsd_host");
    conf.gpsd_port = cfg_getstr(cfg, "gpsd_port");
    conf.gpsd_use = cfg_getint(cfg, "gpsd_use");
    #endif
    
    #ifdef AUDIO
    sprintf(compiled_options + strlen(compiled_options), "AUDIO ");
    
    conf.audio_enabled = cfg_getint(cfg, "audio_enabled");
    conf.audio_device = cfg_getstr(cfg, "audio_device");
    conf.audio_rate = cfg_getint(cfg, "audio_rate");
    conf.audio_channels = cfg_getint(cfg, "audio_channels");
    #endif
    
    logger("CORE: Capture built on '" __DATE__ " " __TIME__ "' with '" __VERSION__ "'");
    logger("CORE: Compiled-in Options: %s", compiled_options);
    
    logger(
	"CORE: Configuration summary:\n\t"
	"Device: %s (input %d)\n\t"
	"Resolution: %dx%d @ %dfps\n\t"
	"JPEG buffer: %d\n\t"
	
	#ifdef FFMPEG
	"Deinterlace: %d\n\t"
	#endif
	
	#ifdef JPEG
	"Format: %d\n\t"
	"JPEG Quality: %d\n\t"
	"Text Double: %d\n\t"
	"OSD: %d\n\t"
	#endif
	
	#ifdef MKV
	"Video dir: %s\n\t"
	"Timelapse: %d\n\t"
	"Sync Interval: %d\n\t"
	"Subtitle Enabled: %d\n\t"
	"Maximum size of video files: %lld Mbytes\n\t"
	"Write buffer count: %d\n\t"
	#endif
	
	#ifdef SQLITE
	"DB path: %s\n\t"
	#endif
	
	#ifdef GPS
	"GPSD: %s:%s\n\t"
	#endif
	
	#ifdef AUDIO
	"Audio Enabled: %d\n\t"
	"Audio Device: %s\n\t"
	"Audio Rate: %d\n\t"
	"Audio Channels: %d\n\t"
	#endif
	
	#ifdef HTTPD
	"HTTP ports: MJPEG: %d, JPEG: %d\n\t%s"
	#endif
	,
	conf.video_device, conf.input,
	conf.width, conf.height, conf.fps,
	conf.jpeg_buffer_size,
	#ifdef FFMPEG
	conf.deinterlace,
	#endif
	
	#ifdef JPEG
	conf.format,
	conf.jpeg_quality,
	conf.text_double,
	conf.osd,
	#endif
	
	#ifdef MKV
	conf.video_dir,
	conf.timelapse,
	conf.sync_interval,
	conf.subtitle_enabled,
	(conf.maximum_size / 1048576),
	conf.write_buffer_count,
	#endif
	
	#ifdef SQLITE
	conf.db_path,
	#endif
	
	#ifdef GPS
	conf.gpsd_host, conf.gpsd_port,
	#endif
	
	#ifdef AUDIO
	conf.audio_enabled, conf.audio_device, conf.audio_rate, conf.audio_channels,
	#endif
	
	#ifdef HTTPD
	conf.httpd_mjpeg_port, conf.httpd_jpeg_port,
	#endif
	""
    );
}

int main (int argc, char **argv) {
    // Argument parsing
    if(argc > 2) {
	logger("CORE: Usage: %s [configuration_file]", argv[0]);
	exit(EXIT_FAILURE);
    } else if(argc == 2) {
	if(!file_exists(argv[1])) {
	    logger("CORE: Configuration file '%s' not found!", argv[1]);
	    exit(EXIT_FAILURE);
	} else {
	    logger("CORE: Using configuration file '%s'", argv[1]);
	    config_file = argv[1];
	}
    } else {
	logger("Using default configuration file '%s' in current directory", config_file);
    }
    
    parse_config(config_file);
    
    // Modify process name
    //int argv_size = strlen(argv[0]);
    //snprintf(argv[0], argv_size, "");
    //snprintf(argv[0], argv_size, "capture [%s]", config_file);
    
    // Apply scheduling
    struct sched_param _sched;
    _sched.sched_priority = 9;
    if(sched_setscheduler(getpid(), SCHED_FIFO, &_sched) == 0) {
	logger("CORE: Realtime scheduling applied");
    } else {
	logger("CORE: Cannot apply realtime scheduling");
	exit(1);
    }
    
    // Signal handling
    struct sigaction new_action;
    new_action.sa_handler = sig_handler;
    new_action.sa_flags = 0;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGHUP,  &new_action, NULL);
    sigaction(SIGINT,  &new_action, NULL);
    
    #ifdef JPEG
    // Allocate buffers
    if(conf.format <= 1) {
	yuv420_buffer_size = conf.width * conf.height * 3 / 2;
	yuv422_buffer_size = conf.width * conf.height * 2;
	
	yuv420_buffer = (unsigned char *) malloc(yuv420_buffer_size);
	yuv422_buffer = (unsigned char *) malloc(yuv422_buffer_size);
    }
    #endif
    
    jpeg_buffer = (unsigned char *) malloc(conf.jpeg_buffer_size);
    
    if(
    #ifdef JPEG
    ((yuv420_buffer == NULL || yuv422_buffer == NULL) && conf.format <= 1) ||
    #endif
    jpeg_buffer == NULL) {
	logger("CORE: Cannot allocate frame buffer memory");
	exit(1);
    } else {
	logger("CORE: Frame buffer memory allocated (%.2f MB)", (double)(
	#ifdef JPEG
	yuv420_buffer_size + yuv422_buffer_size +
	#endif
	conf.jpeg_buffer_size) / 1048576.0f);
    }
    
    // Main shit
    v4l2_init(conf.video_device, conf.input, conf.width, conf.height, conf.fps);
    
    #ifdef HTTPD
    httpd_init(conf.httpd_mjpeg_port, conf.httpd_jpeg_port);
    #endif
    
    #ifdef GPS
    if(conf.gpsd_use) {
	gpsd_init();
    }
    #endif
    
    #ifdef JPEG
    if(conf.osd) {
	draw_text_initialize_chars();
    }
    init_jpeg(jpeg_buffer, yuv420_buffer, yuv420_buffer_size, conf.width, conf.height, conf.jpeg_quality);
    #endif
    
    #ifdef SQLITE
    if(conf.file_use) {
	#ifdef MKV
	sqlite_init(conf.db_path, conf.maximum_size);
	#else
	sqlite_init(conf.db_path);
	#endif
    }
    #endif
    
    // Lockout process in memory
    if(mlockall(MCL_FUTURE) == 0) {
	logger("CORE: mlockall() succeeded");
    } else {
	logger("CORE: mlockall() failed");
	exit(1);
    }
    
    #ifdef AUDIO
    if(conf.audio_enabled) {
	if(audio_init(conf.audio_device, conf.audio_rate, conf.audio_channels) < 0) {
	    logger("Unable to initialize audio");
	    exit(1);
	}
    }
    #endif
    
    #ifdef MKV
    if(conf.file_use) {
	pthread_create(&t_write_thread, NULL, write_thread, NULL);
	
	logger("CORE: Cleaning up empty directories");
	// Remove empty subdirectories in video dir
	empty_dir_cleanup();
    }
    #endif
    
    main_loop();
    
    #ifdef AUDIO
    if(conf.audio_enabled) {
	if(audio_uninit() < 0) {
	    logger("Unable to uninitialize audio");
	}
    }
    #endif
    
    #ifdef MKV
    if(conf.file_use) {
	logger("CORE: Waiting for write_thread() to finish...");
	pthread_join(t_write_thread, NULL);
    }
    #endif
    
    #ifdef SQLITE
    if(conf.file_use) {
	sqlite_uninit();
    }
    #endif
    
    #ifdef JPEG
    uninit_jpeg();
    #endif
    
    #ifdef GPS
    if(conf.gpsd_use) {
	gpsd_uninit();
    }
    #endif
    
    #ifdef HTTPD
    httpd_uninit();
    #endif
    
    v4l2_uninit();
    
    exit(EXIT_SUCCESS);
    
    return 0;
}
