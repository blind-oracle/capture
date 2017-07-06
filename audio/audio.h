int audio_init(char *device_name, unsigned int rate, u_int8_t channels);
int audio_read(unsigned char *mp3_buffer, int mp3_size);
int audio_uninit();
