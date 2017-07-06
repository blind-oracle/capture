#include <stdio.h>
#include <gps.h>
#include <unistd.h>

int main(int argc, char **argv) {
    struct gps_data_t *gpsd_data;
    char *gps_fix = NULL;
    char *gps_fix_mode = NULL;
    int c = 0, i = 0;
    
    gpsd_data = gps_open("127.0.0.1", "2947");
    gps_stream(gpsd_data, WATCH_ENABLE, NULL);
    
    while(c < 100) {
        if(gps_waiting(gpsd_data)) {
	    for(i = 0; i < 5; i++) {
		gps_poll(gpsd_data);
	    }
	    break;
	} else {
	    usleep(100000);
	}
	
	c++;
    }
    
    switch(gpsd_data->status) {
        case STATUS_NO_FIX:
            gps_fix = "NO";
        break;
        case STATUS_FIX:
            gps_fix = "GPS";
        break;
        case STATUS_DGPS_FIX:
            gps_fix = "DGPS";
        break;
    }
    
    switch(gpsd_data->fix.mode) {
        case MODE_NOT_SEEN:
        case MODE_NO_FIX:
            gps_fix_mode = "";
        break;
        case MODE_2D:
            gps_fix_mode = "2D ";
        break;
        case MODE_3D:
            gps_fix_mode = "3D ";
        break;
    }
    
    if(gpsd_data->fix.mode == MODE_NOT_SEEN || gpsd_data->fix.mode == MODE_NO_FIX) {
	printf("NO FIX\n");
	exit(1);
    }
    
    if(argc == 1) {
	printf("SAT: %d/%d (%s%s)\n",
	    gpsd_data->satellites_used,
	    gpsd_data->satellites_visible,
	    gps_fix_mode,
	    gps_fix
        );
	
        printf("LAT: %.9f\n", gpsd_data->fix.latitude);
        printf("LON: %.9f\n", gpsd_data->fix.longitude);
        printf("ALT: %.2f m\n", gpsd_data->fix.altitude);
        printf("SPD: %.2f km/h\n", gpsd_data->fix.speed * 3.6f);
	printf("HEAD: %.2f\n", gpsd_data->fix.track);
    } else {
	printf("%.9f %.9f %.3f\n", gpsd_data->fix.latitude, gpsd_data->fix.longitude, gpsd_data->fix.altitude);
    }
    
    gps_close(gpsd_data);
    
    return 0;
}
