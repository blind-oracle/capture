#ifdef MKV
u_int64_t sqlite_get_total_size(void);
void sqlite_init(char *db_path, u_int64_t max_size);
void sqlite_add_row(char *filename);
void sqlite_update_row(char *filename, u_int64_t size);
#else
void sqlite_init(char *db_path);
#endif

void sqlite_uninit(void);

#ifdef GPS
void sqlite_add_gps(double lat, double lon, double alt, double speed, double head, int sat, int fix);
#endif
