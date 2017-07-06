#define _XOPEN_SOURCE 500
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <strings.h>
#include <time.h>
#include <string.h>
#include <ftw.h>

#ifdef FFMPEG
#include <libavformat/avformat.h>
#endif

#include "utils.h"

#define SATURATE8(x) ((unsigned int) x <= 255 ? x : (x < 0 ? 0: 255))

extern struct s_conf conf;

u_int64_t get_timestamp_int64 (void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    return (tv.tv_sec * 1e6 + tv.tv_usec);
}

double get_timestamp_double (void) {
    return (double)(get_timestamp_int64()) / 1e6;
}

#ifdef MKV
void timestamp_subtitle (char *buffer, unsigned long ts) {
    int hours = ts / 3600 / 1000;
    int minutes = (ts - hours * 3600 * 1000) / 60 / 1000;
    int seconds = (ts - hours * 3600 * 1000 - minutes * 60 * 1000) / 1000;
    int msecs = (ts - hours * 3600 * 1000 - minutes * 60 * 1000 - seconds * 1000);
    
    sprintf(buffer, "%02d:%02d:%02d,%03d", hours, minutes, seconds, msecs);
}
#endif

#ifdef MKV
u_int64_t file_get_size(char *filename) {
    struct stat fstat;
    stat(filename, &fstat);
    return fstat.st_size;
}
#endif

int file_exists (char *filename) {
    struct stat st;
    
    if(stat(filename, &st) == 0) {
	return 1;
    }
    
    return 0;
}

void logger(const char *msg, ...) {
    va_list ap;
    char buf[512];
    char ftime[32];
    time_t rawtime;
    struct tm * timeinfo;
    
    bzero(ftime, sizeof(ftime));
    bzero(buf, sizeof(ftime));
    
    va_start(ap, msg);
    vsprintf(buf, (void *)msg, ap);
    va_end(ap);
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(ftime, sizeof(ftime), "%b %d %X", timeinfo);
    fprintf(stderr, "[%s] %s\n", ftime, buf);
}

#ifdef MKV
int dir_cleanup_callback(const char *name, const struct stat *status, int type, struct FTW *ftwbuf) {
    // Avoid removing top-level video dir
    if(type == FTW_DP && (strcmp(name, conf.video_dir) != 0)) {
	if(rmdir(name) == 0) {
	    logger("CORE: Empty dir '%s' removed", name);
	}
    }
    
    return 0;
}

void empty_dir_cleanup(void) {
    nftw(conf.video_dir, dir_cleanup_callback, 1, FTW_DEPTH);
}
#endif

void convert_yuv422_to_420p(unsigned char *map, unsigned char *cap_map, int width, int height) {
    unsigned char *src, *dest, *src2, *dest2;
    int i, j;
    
    src = cap_map;
    dest = map;
    
    /* Create the Y plane */
    for (i = width * height; i > 0; i--) {
        *dest++ = *src;
        src += 2;
    }
    
    /* Create U and V planes */
    src = cap_map + 1;
    src2 = cap_map + width * 2 + 1;
    dest = map + width * height;
    dest2 = dest + (width * height) / 4;
    
    for (i = height / 2; i > 0; i--) {
        for (j = width / 2; j > 0; j--) {
            *dest = ((int) *src + (int) *src2) / 2;
            src += 2;
            src2 += 2;
            dest++;
            *dest2 = ((int) *src + (int) *src2) / 2;
            src += 2;
            src2 += 2;
            dest2++;
        }
	
        src += width * 2;
        src2 += width * 2;
    }
}

#ifdef FFMPEG
int deinterlace_420p (unsigned char *img, int width, int height) {
    AVFrame *picture;
    int width2 = width / 2;
    
    picture = avcodec_alloc_frame();
    
    if(!picture) {
        return 1;
    }
    
    picture->data[0] = img;
    picture->data[1] = img+width*height;
    picture->data[2] = picture->data[1]+(width*height)/4;
    
    picture->linesize[0] = width;
    picture->linesize[1] = width2;
    picture->linesize[2] = width2;
    
    avpicture_deinterlace((AVPicture *)picture, (AVPicture *)picture, PIX_FMT_YUV420P, width, height);
    
    av_free(picture);
    
    return 0;
}
#endif
