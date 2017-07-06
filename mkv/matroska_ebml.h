#include <sys/types.h>

/* Matroska display size units from the spec */
#define	DS_PIXELS        0
#define	DS_CM            1
#define	DS_INCHES        2
#define	DS_ASPECT_RATIO  3

typedef struct mk_writer mk_writer;
typedef struct mk_context mk_context;

struct mk_writer {
    FILE *fp;
    
    unsigned duration_ptr;
    unsigned seekhead_begin_ptr;
    unsigned seekhead_segmentinfo_ptr, seekhead_tracks_ptr, seekhead_cues_ptr;
    unsigned seekhead_segmentinfo_pos, seekhead_tracks_pos, seekhead_cues_pos;
    
    mk_context *root, *cluster, *frame, *blockgroup, *cues, *cuepoint, *cuetrackpositions;
    mk_context *freelist;
    mk_context *actlist;
    
    u_int64_t timescale, cluster_tc, frame_tc, max_frame_tc, last_frame_tc;
    
    int subtitle_enabled;
    int audio_channels;
    int audio_rate;
    
    int track_audio, track_subs;
    
    unsigned int buf_id;
    
    char *subtitle_str;
    char wrote_header, in_frame;
};

struct mk_context {
    struct mk_context *next, **prev, *parent;
    mk_writer *owner;
    unsigned id;
    
    void *data;
    unsigned d_cur, d_max;
};

mk_writer *mk_create_writer(char *filename);

int mk_writeHeader(mk_writer *w,
                   char *writing_app,
                   char *codec_id,
                   void *codec_private, unsigned codec_private_size,
                   unsigned long timescale,
                   unsigned width, unsigned height);

int mk_start_frame(mk_writer *w, unsigned int buf_id);
int mk_close(mk_writer *w, u_int64_t last_frame_duration);

int mk_add_frame_data(mk_writer *w);
int mk_set_frame_flags(mk_writer *w, unsigned long timestamp);

struct clusters_def {
    u_int64_t tc;
    u_int64_t pos;
};
