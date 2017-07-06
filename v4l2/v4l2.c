#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#include "../utils.h"
#include "huffman.h"
#include "v4l2.h"
#include "v4l2_controls.h"

struct buffer {
    void *ptr;
    size_t len;
};

extern struct s_conf conf;

static struct buffer	*buffers	= NULL;
static unsigned int	n_buffers	= 0;

static int		device_fd	= -1;
static char		*device_name	= NULL;
static int		input		= 0;
static int		width		= 0;
static int		height		= 0;
static int		fps		= 0;

static void errno_exit (const char *s) {
    logger("V4L2: %s error %d, %s", s, errno, strerror (errno));
    exit (EXIT_FAILURE);
}

/*
int xioctl (int fd, int request, void *arg) {
    int r;
    
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    
    return r;
}
*/

int xioctl (int fd, int request, void *arg) {
    return ioctl(fd, request, arg);
}

static int is_huffman(unsigned char *buf) {
    unsigned char *ptbuf;
    int i = 0;
    ptbuf = buf;
    
    while (((ptbuf[0] << 8) | ptbuf[1]) != 0xffda) {
	if (i++ > 2048)
	    return 0;
	
	if (((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
            return 1;
        
        ptbuf++;
    }
    
    return 0;
}

static int memcpy_picture(unsigned char *out, unsigned char *buf, int size) {
    unsigned char *ptdeb, *ptlimit, *ptcur = buf;
    int sizein, pos = 0;
    
    if (!is_huffman(buf)) {
	ptdeb = ptcur = buf;
	ptlimit = buf + size;
	
	while ((((ptcur[0] << 8) | ptcur[1]) != 0xffc0) && (ptcur < ptlimit)) {
	    ptcur++;
	}
	
	if (ptcur >= ptlimit) {
	    return pos;
	}
	
	sizein = ptcur - ptdeb;
	memcpy(out+pos, buf, sizein); pos += sizein;
	memcpy(out+pos, dht_data, sizeof(dht_data)); pos += sizeof(dht_data);
	memcpy(out+pos, ptcur, size - sizein); pos += size-sizein;
    } else {
	memcpy(out+pos, ptcur, size); pos += size;
    }
    
    return pos;
}

int v4l2_read_frame (unsigned char *output_buffer) {
    struct v4l2_buffer buf;
    fd_set fds;
    struct timeval tv;
    int r;
    int frame_size = 0;
    
    FD_ZERO (&fds);
    FD_SET (device_fd, &fds);
    
    /* Timeout. */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    
    r = select(device_fd + 1, &fds, NULL, NULL, &tv);
    
    if (-1 == r) {
        if (EINTR == errno)
	    return -1;
	
        errno_exit ("select");
    }
    
    if (0 == r) {
        logger("V4L2: Select timeout on device");
        exit (EXIT_FAILURE);
    }
    
    CLEAR (buf);
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl (device_fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
                return -1;
            case EIO:
        	/* Could ignore EIO, see spec. */
                /* fall through */
            default:
                errno_exit ("VIDIOC_DQBUF");
        }
    }
    
    #ifdef JPEG
    if(conf.format >= 1) {
    #endif
	frame_size = memcpy_picture(output_buffer, buffers[buf.index].ptr, buf.bytesused);
	
	#ifdef DEBUG
	logger("bytesused: %d -> frame_size: %d", buf.bytesused, frame_size);
	#endif
    #ifdef JPEG
    } else {
	memcpy(output_buffer, buffers[buf.index].ptr, buf.bytesused);
	frame_size = buf.bytesused;
    }
    #endif
    
    if (-1 == xioctl (device_fd, VIDIOC_QBUF, &buf)) {
        errno_exit ("VIDIOC_QBUF");
    }
    
    return frame_size;
}

static void v4l2_stop_capturing (void) {
    enum v4l2_buf_type type;
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (-1 == xioctl (device_fd, VIDIOC_STREAMOFF, &type))
        errno_exit ("VIDIOC_STREAMOFF");
}

static void v4l2_start_capturing (void) {
    unsigned int i;
    enum v4l2_buf_type type;
    
    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;
	
        CLEAR (buf);
	
        buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory	= V4L2_MEMORY_MMAP;
        buf.index	= i;
	
        if (-1 == xioctl (device_fd, VIDIOC_QBUF, &buf))
	    errno_exit ("VIDIOC_QBUF");
    }
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (-1 == xioctl (device_fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");
}

static void v4l2_uninit_device (void) {
    unsigned int i;
    
    for (i = 0; i < n_buffers; ++i) {
	if (-1 == munmap (buffers[i].ptr, buffers[i].len))
	    errno_exit ("munmap");
    }
    
    free (buffers);
}

static void v4l2_init_mmap (void) {
    struct v4l2_requestbuffers req;
    
    CLEAR (req);
    
    req.count	= 4;
    req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory	= V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl (device_fd, VIDIOC_REQBUFS, &req)) {
	if (EINVAL == errno) {
	    logger("V4L2: %s does not support memory mapping", device_name);
	    exit (EXIT_FAILURE);
	} else {
	    errno_exit ("VIDIOC_REQBUFS");
	}
    }
    
    if (req.count < 2) {
	logger("V4L2: Insufficient buffer memory on %s", device_name);
	exit (EXIT_FAILURE);
    }
    
    buffers = calloc (req.count, sizeof (*buffers));
    
    if (!buffers) {
	logger("V4L2: Out of memory");
	exit (EXIT_FAILURE);
    }
    
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
	
        CLEAR (buf);
	
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;
	
        if (-1 == xioctl (device_fd, VIDIOC_QUERYBUF, &buf))
	    errno_exit ("VIDIOC_QUERYBUF");
	
        buffers[n_buffers].len = buf.length;
        buffers[n_buffers].ptr =
        mmap (
	    NULL /* start anywhere */,
	    buf.length,
	    PROT_READ | PROT_WRITE /* required */,
	    MAP_SHARED /* recommended */,
	    device_fd,
	    buf.m.offset
	);
	
        if (MAP_FAILED == buffers[n_buffers].ptr)
            errno_exit ("mmap");
    }
}

static void v4l2_init_device () {
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;
    
    CLEAR(cap);
    
    if (-1 == xioctl (device_fd, VIDIOC_QUERYCAP, &cap)) {
	if (EINVAL == errno) {
	    logger("V4L2: %s is not a V4L2 device", device_name);
	    exit (EXIT_FAILURE);
	} else {
	    errno_exit ("VIDIOC_QUERYCAP");
	}
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
	logger("V4L2: %s is not a video capture device", device_name);
	exit (EXIT_FAILURE);
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
	logger("V4L2: %s does not support streaming i/o", device_name);
	exit (EXIT_FAILURE);
    }
    
    /* Select video input, video standard and tune here. */
    if (xioctl(device_fd, VIDIOC_S_INPUT, &input) == -1) {
	logger("V4L2: Error selecting input %d VIDIOC_S_INPUT", input);
	exit (EXIT_FAILURE);
    }
    
    CLEAR (cropcap);
    
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (0 == xioctl (device_fd, VIDIOC_CROPCAP, &cropcap)) {
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = cropcap.defrect; // reset to default
	
	if (-1 == xioctl (device_fd, VIDIOC_S_CROP, &crop)) {
	    switch (errno) {
		case EINVAL:
		    // Cropping not supported
		    break;
		default:
		    // Errors ignored
		    break;
	    }
	}
    } else {        
	// Errors ignored
    }
    
    CLEAR (fmt);
    
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    
    #ifdef JPEG
    if(conf.format >= 1) {
    #endif
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    #ifdef JPEG
    } else {
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    }
    #endif
    
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    
    if (-1 == xioctl (device_fd, VIDIOC_S_FMT, &fmt)) {
        errno_exit ("VIDIOC_S_FMT");
    }
    
    /* Note VIDIOC_S_FMT may change width and height. */
    
    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
	fmt.fmt.pix.bytesperline = min;
    
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    
    if (fmt.fmt.pix.sizeimage < min)
	fmt.fmt.pix.sizeimage = min;
    
    // Set FPS
    struct v4l2_streamparm* setfps;
    setfps = (struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
    memset(setfps, 0, sizeof(struct v4l2_streamparm));
    setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps->parm.capture.timeperframe.numerator = 1;
    setfps->parm.capture.timeperframe.denominator = fps;
    
    if (xioctl(device_fd, VIDIOC_S_PARM, setfps) == -1) {
	logger("V4L2: Failed to set FPS to %d", fps);
    }
}

static void v4l2_close_device (void) {
    if (-1 == close (device_fd)) {
        errno_exit ("close");
    }
    
    device_fd = -1;
}

static void v4l2_open_device (void) {
    struct stat st; 
    
    if(-1 == stat (device_name, &st)) {
        logger("V4L2: Cannot identify device '%s': %d, %s", device_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
    
    if(!S_ISCHR(st.st_mode)) {
        logger("V4L2: %s is not a device", device_name);
        exit (EXIT_FAILURE);
    }
    
    device_fd = open (device_name, O_RDWR);
    
    if(-1 == device_fd) {
        logger("V4L2: Cannot open '%s': %d, %s", device_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}

void v4l2_init (char *arg_device_name, int arg_input, int arg_width, int arg_height, int arg_fps) {
    device_name = arg_device_name;
    input = arg_input;
    width = arg_width;
    height = arg_height;
    fps = arg_fps;
    
    v4l2_open_device();
    v4l2_init_device();
    v4l2_init_mmap();
    v4l2_start_capturing();
    
    sleep(1);
    
    if(0) {
	v4l2_init_controls(device_fd);
        // Focus-Auto (0-1)
	v4l2_set_control(device_fd, 0x9a090c, 0);
	// Focus-Absolute (0-255)
	v4l2_set_control(device_fd, 0x9a090a, 1);
	// Power line frequency (0 = disable, 1 = 50Hz, 2 = 60Hz)
	v4l2_set_control(device_fd, 0x980918, 1);
    }
    
    logger("V4L2: initialized");
}

void v4l2_uninit(void) {
    v4l2_stop_capturing();
    v4l2_uninit_device();
    v4l2_close_device();
    
    logger("V4L2: uninitialized");
}
