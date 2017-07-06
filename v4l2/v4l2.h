int xioctl (int fd, int request, void *arg);
int v4l2_read_frame (unsigned char *output_buffer);
void v4l2_init (char *arg_device_name, int arg_input, int arg_width, int arg_height, int arg_fps);
void v4l2_uninit(void);
