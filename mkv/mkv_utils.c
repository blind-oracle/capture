#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include "matroska_ebml.h"
#include "../utils.h"
#include "../db.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static mk_writer *mkv_w;
u_int64_t frames_written = 0, open_time = 0, file_size = 0;
static char *_filename;

extern struct s_conf conf;
extern struct _write_buffers *write_buffers;
extern unsigned long frames_dropped;

int mkv_open_file(char *filename) {
    _filename = filename;
    
    frames_written = 0;
    file_size = 0;
    
    unsigned char codec_private[40] = {
	0x28, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0xC0,
	0x03, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x4D, 0x4A,
	0x50, 0x47, 0x00, 0x40, 0x38, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
    };
    
    mkv_w = mk_create_writer(filename);
    
    if(mkv_w == NULL) {
        return -1;
    }
    
    mkv_w->subtitle_enabled = conf.subtitle_enabled;
    
    codec_private[4] = conf.width & 0x00FF;
    codec_private[5] = (conf.width & 0xFF00) >> 8;
    
    codec_private[8] = conf.height & 0x00FF;
    codec_private[9] = (conf.height & 0xFF00) >> 8;
    
    mk_writeHeader(mkv_w, "capture", "V_MS/VFW/FOURCC",
                          codec_private, 40,
                          1000000, // TimeScale
                          conf.width, conf.height);
    
    open_time = get_timestamp_int64();
    sqlite_add_row(_filename);
    
    logger("MKV: File '%s' opened", filename);
    
    return 0;
}

int mkv_write_frame(unsigned int buf_id) {
    if(mk_start_frame(mkv_w, buf_id) < 0) {
	return -1;
    }
    
    if(mk_add_frame_data(mkv_w) < 0) {
	return -1;
    }
    
    if(mk_set_frame_flags(mkv_w, (write_buffers[buf_id].timestamp - open_time) / 1e3) < 0) {
	return -1;
    }
    
    frames_written++;
    file_size += write_buffers[buf_id].ptr_size;
    
    if(conf.sync_interval > 0) {
	if((frames_written % conf.sync_interval) == 0) {
	    fflush(mkv_w->fp);
	}
    }
    
    return 0;
}

int mkv_close_file(u_int64_t last_frame_duration) {
    int ret;
    double record_time = (double)(get_timestamp_int64() - open_time) / 1e6;
    
    ret = mk_close(mkv_w, last_frame_duration);
    
    sqlite_update_row(_filename, file_size);
    
    logger("MKV: File '%s' closed:\n\t"
	    "Size: %.2f MBytes\n\t"
	    "Frames: %ld (%ld dropped)\n\t"
	    "Duration: %.2f s\n\t"
	    "FPS: %.2f\n\t"
	    "~Frame size: %.2f KBytes\n\t"
	    "Bitrate: %.2f Mbits/sec",
	_filename,
	(double)(file_size) / 1048576.0f,
	frames_written, frames_dropped,
	record_time,
	(double)(frames_written) / record_time,
	(double)(file_size) / (double)(frames_written) / 1024.0f,
	(double)(file_size) / record_time / 131072.0f
    );
    
    return ret;
}
