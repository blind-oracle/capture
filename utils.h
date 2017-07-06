#define LENGTH_OF(x) (sizeof(x)/sizeof(x[0]))

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define CHECK(x) \
do { \
    if((x) < 0) \
        return -1; \
} while(0)

int file_exists (char *filename);
void convert_yuv422_to_420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void convert_yuv420_to_bgr24(unsigned char *buffer_rgb, unsigned char *buffer_420, unsigned int width, unsigned int height);
void convert_bgr24_to_yuv420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void empty_dir_cleanup(void);
u_int64_t get_timestamp_int64 (void);
double get_timestamp_double (void);
void logger(const char *msg, ...);
int deinterlace_420p (unsigned char *img, int width, int height);
u_int64_t file_get_size(char *filename);
void timestamp_subtitle (char *buffer, unsigned long ts);

#ifdef MKV
struct _write_buffers {
    int free;
    int ptr_size;
    char subtitle[256];
    u_int64_t frame_duration;
    u_int64_t timestamp;
    
    char *ptr;
};
#endif

struct s_conf {
    #ifdef HTTPD
    unsigned int httpd_mjpeg_port, httpd_jpeg_port;
    #endif
    
    #ifdef GPS
    unsigned int gpsd_use;
    char *gpsd_host, *gpsd_port;
    #endif
    
    #ifdef FFMPEG
    unsigned int deinterlace;
    #endif
    
    #ifdef SQLITE
    char *db_path;
    #endif
    
    #ifdef MKV
    unsigned int file_use, timelapse, keyframe_interval, sync_interval, subtitle_enabled, write_buffer_count;
    u_int64_t maximum_size;
    char *video_dir;
    #endif
    
    #ifdef AUDIO
    unsigned int audio_enabled, audio_channels, audio_rate;
    char *audio_device;
    #endif
    
    #ifdef JPEG
    unsigned int jpeg_quality, format, text_double, osd;
    #endif
    
    unsigned int input;
    char *video_device;
    
    unsigned int width, height, fps, jpeg_buffer_size;
};
