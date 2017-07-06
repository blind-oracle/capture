#include <linux/kernel.h>
#include <linux/videodev2.h>

void v4l2_init_controls(int device_fd);
void v4l2_set_control(int device_fd, int control, int value);

#define __user

#ifndef V4L2_CID_BACKLIGHT_COMPENSATION
#define V4L2_CID_BACKLIGHT_COMPENSATION		(V4L2_CID_PRIVATE_BASE+0)
#endif

#ifndef V4L2_CID_POWER_LINE_FREQUENCY
#define V4L2_CID_POWER_LINE_FREQUENCY		(V4L2_CID_PRIVATE_BASE+1)
#endif

#ifndef V4L2_CID_SHARPNESS
#define V4L2_CID_SHARPNESS			(V4L2_CID_PRIVATE_BASE+2)
#endif

#ifndef V4L2_CID_HUE_AUTO
#define V4L2_CID_HUE_AUTO			(V4L2_CID_PRIVATE_BASE+3)
#endif

#ifndef V4L2_CID_FOCUS_AUTO
#define V4L2_CID_FOCUS_AUTO			(V4L2_CID_PRIVATE_BASE+4)
#endif

#ifndef V4L2_CID_FOCUS_ABSOLUTE
#define V4L2_CID_FOCUS_ABSOLUTE			(V4L2_CID_PRIVATE_BASE+5)
#endif

#ifndef V4L2_CID_FOCUS_RELATIVE
#define V4L2_CID_FOCUS_RELATIVE			(V4L2_CID_PRIVATE_BASE+6)
#endif

#ifndef V4L2_CID_PAN_RELATIVE
#define V4L2_CID_PAN_RELATIVE			(V4L2_CID_PRIVATE_BASE+7)
#endif

#ifndef V4L2_CID_TILT_RELATIVE
#define V4L2_CID_TILT_RELATIVE			(V4L2_CID_PRIVATE_BASE+8)
#endif

#ifndef V4L2_CID_PANTILT_RESET
#define V4L2_CID_PANTILT_RESET			(V4L2_CID_PRIVATE_BASE+9)
#endif

#ifndef V4L2_CID_EXPOSURE_AUTO
#define V4L2_CID_EXPOSURE_AUTO			(V4L2_CID_PRIVATE_BASE+10)
#endif

#ifndef V4L2_CID_EXPOSURE_ABSOLUTE
#define V4L2_CID_EXPOSURE_ABSOLUTE		(V4L2_CID_PRIVATE_BASE+11)
#endif

#ifndef V4L2_CID_EXPOSURE_AUTO_PRIORITY
#define V4L2_CID_EXPOSURE_AUTO_PRIORITY		(V4L2_CID_PRIVATE_BASE+14)
#endif

#ifndef V4L2_CID_WHITE_BALANCE_TEMPERATURE_AUTO
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_AUTO	(V4L2_CID_PRIVATE_BASE+12)
#endif

#ifndef V4L2_CID_WHITE_BALANCE_TEMPERATURE
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE	(V4L2_CID_PRIVATE_BASE+13)
#endif

#ifndef V4L2_CID_PRIVATE_LAST
#define V4L2_CID_PRIVATE_LAST			V4L2_CID_EXPOSURE_AUTO_PRIORITY
#endif

#define UVC_CONTROL_SET_CUR	(1 << 0)
#define UVC_CONTROL_GET_CUR	(1 << 1)
#define UVC_CONTROL_GET_MIN	(1 << 2)
#define UVC_CONTROL_GET_MAX	(1 << 3)
#define UVC_CONTROL_GET_RES	(1 << 4)
#define UVC_CONTROL_GET_DEF	(1 << 5)
#define UVC_CONTROL_RESTORE	(1 << 6)


enum uvc_control_data_type {
    UVC_CTRL_DATA_TYPE_RAW = 0,
    UVC_CTRL_DATA_TYPE_SIGNED,
    UVC_CTRL_DATA_TYPE_UNSIGNED,
    UVC_CTRL_DATA_TYPE_BOOLEAN,
    UVC_CTRL_DATA_TYPE_ENUM,
    UVC_CTRL_DATA_TYPE_BITMASK,
};

#define UVC_CONTROL_GET_RANGE	(UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
				 UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | \
				 UVC_CONTROL_GET_DEF)

#define V4L2_CID_PAN_RELATIVE_LOGITECH   0x0A046D01
#define V4L2_CID_TILT_RELATIVE_LOGITECH  0x0A046D02
#define V4L2_CID_PANTILT_RESET_LOGITECH  0x0A046D03
#define V4L2_CID_FOCUS_LOGITECH          0x0A046D04
#define V4L2_CID_LED1_MODE_LOGITECH      0x0A046D05
#define V4L2_CID_LED1_FREQUENCY_LOGITECH 0x0A046D06

#define UVC_GUID_LOGITECH_MOTOR_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1               1
#define XU_MOTORCONTROL_PANTILT_RELATIVE 1
#define XU_MOTORCONTROL_PANTILT_RESET    2
#define XU_MOTORCONTROL_FOCUS            3

struct uvc_xu_control_info {
	__u8 entity[16];
	__u8 index;
	__u8 selector;
	__u16 size;
	__u32 flags;
};

struct uvc_xu_control_mapping {
	__u32 id;
	__u8 name[32];
	__u8 entity[16];
	__u8 selector;

	__u8 size;
	__u8 offset;
	
	enum v4l2_ctrl_type v4l2_type;
	enum uvc_control_data_type data_type;
};

struct uvc_xu_control {
	__u8 unit;
	__u8 selector;
	__u16 size;
	__u8 __user *data;
};

#define UVCIOC_CTRL_ADD		_IOW  ('U', 1, struct uvc_xu_control_info)
#define UVCIOC_CTRL_MAP		_IOWR ('U', 2, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_GET		_IOWR ('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET		_IOW  ('U', 4, struct uvc_xu_control)
