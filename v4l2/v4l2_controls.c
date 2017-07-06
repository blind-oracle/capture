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

#include "v4l2_controls.h"
#include "v4l2.h"
#include "../utils.h"

static struct uvc_xu_control_info xu_ctrls[] = {
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_FOCUS,
    .index    = 2,
    .size     = 6,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
  },
  {
    .entity   = UVC_GUID_LOGITECH_USER_HW_CONTROL,
    .selector = XU_HW_CONTROL_LED1,
    .index    = 0,
    .size     = 3,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
  }
};

static struct uvc_xu_control_mapping xu_mappings[] = {
  {
    .id        = V4L2_CID_FOCUS_LOGITECH,
    .name      = "Focus (absolute)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_FOCUS,
    .size      = 8,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  },
  {
    .id        = V4L2_CID_LED1_MODE_LOGITECH,
    .name      = "LED1 Mode",
    .entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
    .selector  = XU_HW_CONTROL_LED1,
    .size      = 8,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  },
  {
    .id        = V4L2_CID_LED1_FREQUENCY_LOGITECH,
    .name      = "LED1 Frequency",
    .entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
    .selector  = XU_HW_CONTROL_LED1,
    .size      = 8,
    .offset    = 16,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  }
};

static void v4l2_control_read(int device_fd, struct v4l2_queryctrl *ctrl) {
    struct v4l2_control c;
    c.id = ctrl->id;
    
    if(xioctl(device_fd, VIDIOC_G_CTRL, &c) != -1) {
	if (ctrl->type == V4L2_CTRL_TYPE_MENU) {
	    int i;
	    for(i = ctrl->minimum; i <= ctrl->maximum; i++) {
		struct v4l2_querymenu qm;
		
		qm.id = ctrl->id;
		qm.index = i;
		
		if(xioctl(device_fd, VIDIOC_QUERYMENU, &qm) == 0) {
		    logger("V4L2_CONTROLS: Menu item %d: %s", qm.index, qm.name);
		} else {
		    logger("V4L2_CONTROLS: Unable to get menu item for %s, index=%d", ctrl->name, qm.index);
		}
	    }
	} else {
	    
        }
        
        logger("V4L2_CONTROLS: V4L2 parameter found: (0x%X) %s value %d", ctrl->id, ctrl->name, c.value);
    } else {
        logger("V4L2_CONTROLS: Unable to get the value of %s", ctrl->name);
    }
};

void v4l2_init_controls(int device_fd) {
    int i;
    struct v4l2_queryctrl ctrl;
    
    ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    
    if(0 == xioctl(device_fd, VIDIOC_QUERYCTRL, &ctrl)) {
	do {
	    v4l2_control_read(device_fd, &ctrl);
	    ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	} while(0 == xioctl (device_fd, VIDIOC_QUERYCTRL, &ctrl));
    }
    
    errno = 0;
    for(i = 0; i < LENGTH_OF(xu_ctrls); i++) {
	if(xioctl(device_fd, UVCIOC_CTRL_ADD, &xu_ctrls[i]) < 0) {
	    if(errno == EEXIST) {
		logger("V4L2_CONTROLS: Control %d already exists", i);
	    } else {
		logger("V4L2_CONTROLS: Control %d cannot be added", i);
	    }
	}
    }
    
    errno = 0;
    for (i = 0; i < LENGTH_OF(xu_mappings); i++) {
	if(xioctl(device_fd, UVCIOC_CTRL_MAP, &xu_mappings[i]) < 0) {
	    if(errno == EEXIST) {
		logger("V4L2_CONTROLS: Mapping %d already exists", i);
	    } else {
		logger("V4L2_CONTROLS: Mapping %d cannot be added", i);
	    }
	}
    }
}

void v4l2_set_control(int device_fd, int control, int value) {
    struct v4l2_control control_s;
    
    control_s.id = control;
    control_s.value = value;
    
    if(xioctl(device_fd, VIDIOC_S_CTRL, &control_s) < 0) {
	logger("V4L2_CONTROLS: Failed to set control 0x%X to %d", control, value);
    }
}
