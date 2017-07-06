#define _GNU_SOURCE
#include <gps.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <sched.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "gpsd.h"
#include "utils.h"
#include "db.h"

extern int _shutdown;
extern struct s_conf conf;

struct gps_data_t *gpsd_data = NULL, *gpsd_data_new = NULL;

pthread_t t_gps_update;
pthread_mutex_t mutex_gps;

double db_interval = 5.0f;
double db_last = 0.0f;

static void *gpsd_update_thread(void *arg) {
    logger("GPS: Update thread initialized");
    int gps_fix = 0;
    
    while(!_shutdown) {
	if(gps_waiting(gpsd_data)) {
	    pthread_mutex_lock(&mutex_gps);
	    gps_poll(gpsd_data);
	    pthread_mutex_unlock(&mutex_gps);
	    
	    if(get_timestamp_double() >= (db_last + db_interval)) {
		db_last = get_timestamp_double();
		
	        switch(gpsd_data->fix.mode) {
		    case MODE_NOT_SEEN:
		    case MODE_NO_FIX:
			gps_fix = 0;
		    break;
		    case MODE_2D:
			gps_fix = 1;
		    break;
		    case MODE_3D:
			gps_fix = 2;
		    break;
		}
		
		sqlite_add_gps(
		    gpsd_data->fix.latitude,
		    gpsd_data->fix.longitude,
		    gpsd_data->fix.altitude,
		    gpsd_data->fix.speed * 3.6f,
		    gpsd_data->fix.track,
		    gpsd_data->satellites_used,
		    gps_fix);
	    }
	}
	
	usleep(100000);
    }
    
    pthread_exit(NULL);
}

void gpsd_init(void) {
    gpsd_data = gps_open(conf.gpsd_host, conf.gpsd_port);
    gpsd_data_new = malloc(sizeof(struct gps_data_t));
    
    if(gpsd_data == NULL) {
	logger("GPS: Unable to connect to GPS daemon, location data will be unavailable");
	return;
    } else {
	logger("GPS: Connection to GPS daemon established");
    }
    
    gps_stream(gpsd_data, WATCH_NEWSTYLE, NULL);
    gpsd_data_new = malloc(sizeof(struct gps_data_t));
    
    pthread_mutex_init(&mutex_gps, NULL);
    pthread_create(&t_gps_update, NULL, gpsd_update_thread, NULL);
}

struct gps_data_t *gpsd_update(void) {
    if(gpsd_data == NULL) {
	return NULL;
    }
    
    pthread_mutex_lock(&mutex_gps);
    memcpy(gpsd_data_new, gpsd_data, sizeof(struct gps_data_t));
    pthread_mutex_unlock(&mutex_gps);
    
    return gpsd_data_new;
}

void gpsd_uninit(void) {
    if(gpsd_data != NULL) {
	pthread_cancel(t_gps_update);
	pthread_join(t_gps_update, NULL);
	
	gps_close(gpsd_data);
	logger("GPS: Disconnected");
    }
}
