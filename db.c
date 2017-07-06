#define _GNU_SOURCE
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

#include <sqlite3.h>

#include "db.h"
#include "utils.h"

#ifdef MKV
#define CREATE_TABLE_CAPTURE \
    "CREATE TABLE IF NOT EXISTS `capture` (" \
    "`capture_id` INTEGER PRIMARY KEY ASC AUTOINCREMENT," \
    "`datetime` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ," \
    "`filename` TEXT," \
    "`size` INTEGER);"
#endif

#ifdef GPS
#define CREATE_TABLE_GPS \
    "CREATE TABLE IF NOT EXISTS `gps` (" \
    "`gps_id` INTEGER  PRIMARY KEY ASC AUTOINCREMENT," \
    "`datetime` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ," \
    "`lat` REAL," \
    "`lon` REAL," \
    "`alt` REAL," \
    "`speed` REAL," \
    "`head` REAL," \
    "`sat` INTEGER," \
    "`fix` INTEGER);"
#endif

extern int _shutdown;
#ifdef MKV
static u_int64_t maximum_size;
#endif

sqlite3 *dbh;

#ifdef MKV
#define TOTAL_QUERY "SELECT SUM(`size`) FROM `capture`"
#define OLDEST_QUERY "SELECT `filename`, `size` FROM `capture` ORDER BY `datetime` ASC LIMIT 1"

pthread_t t_cleanup_thread;

u_int64_t sqlite_get_total_size(void) {
    sqlite3_stmt *sum_stmt;
    sqlite3_prepare_v2(dbh, TOTAL_QUERY, 128, &sum_stmt, NULL); // Total sum
    sqlite3_step(sum_stmt);
    return sqlite3_column_int64(sum_stmt, 0);
}

static void *sqlite_cleanup_thread(void *arg) {
    char filename_del[128];
    char query[256];
    
    sqlite3_stmt *sum_stmt, *select_stmt;
    u_int64_t total_size = 0;
    
    sqlite3_prepare_v2(dbh, TOTAL_QUERY, 128, &sum_stmt, NULL); // Total sum
    sqlite3_prepare_v2(dbh, OLDEST_QUERY, 128, &select_stmt, NULL); // Oldest file
    
    while(!_shutdown) {
	sqlite3_step(sum_stmt);
	total_size = sqlite3_column_int64(sum_stmt, 0);
	
	if(total_size >= maximum_size) {
	    logger("SQLITE: Total file size (%.2f MB) exceeded threshold (%.2f MB)", (double)(total_size) / 1048576.0f, (double)(maximum_size) / 1048576.0f);
	    sqlite3_step(select_stmt);
	    
	    CLEAR(filename_del);
	    sprintf(filename_del, "%s", sqlite3_column_text(select_stmt, 0));
	    unlink(filename_del);
	    
	    CLEAR(query);
	    sprintf(query, "DELETE FROM `capture` WHERE `filename` = '%s'", filename_del);
	    sqlite3_exec(dbh, query, NULL, NULL, NULL);
	    
	    logger("SQLITE: Deleting file '%s'", filename_del);
	    
	    sqlite3_reset(select_stmt);
	}
	
	empty_dir_cleanup();
	sqlite3_reset(sum_stmt);
	
	sleep(120);
    }
    
    pthread_exit(NULL);
}
#endif

#ifdef MKV
void sqlite_init(char *db_path, u_int64_t max_size)
#else
void sqlite_init(char *db_path)
#endif
{
    #ifdef MKV
    maximum_size = max_size;
    #endif
    sqlite3_stmt *select_stmt;
    char filename_del[128];
    u_int64_t file_size;
    char query[256];
    
    sqlite3_open(db_path, &dbh);
    #ifdef MKV
    sqlite3_exec(dbh, CREATE_TABLE_CAPTURE, NULL, NULL, NULL);
    #endif
    #ifdef GPS
    sqlite3_exec(dbh, CREATE_TABLE_GPS, NULL, NULL, NULL);
    #endif
    
    #ifdef MKV
    sqlite3_prepare_v2(dbh, "SELECT `filename`, `size` FROM `capture`", 128, &select_stmt, NULL);
    while(sqlite3_step(select_stmt) == SQLITE_ROW) {
	CLEAR(filename_del);
	sprintf(filename_del, "%s", sqlite3_column_text(select_stmt, 0));
	
	if(!file_exists(filename_del)) {
	    logger("SQLITE: Filename '%s' not found, removing from DB", filename_del);
	    CLEAR(query);
	    sprintf(query, "DELETE FROM `capture` WHERE `filename` = '%s'", filename_del);
	    sqlite3_exec(dbh, query, NULL, NULL, NULL);
	} else {
	    file_size = sqlite3_column_int64(select_stmt, 1);
	    
	    if(file_size == 0) {
		file_size = file_get_size(filename_del);
		
		if(file_size > 0) {
		    logger("SQLITE: File '%s' wasn't properly closed, updating it's size to %llu", filename_del, file_size);
		    
		    CLEAR(query);
		    sprintf(query, "UPDATE `capture` SET `size` = %llu WHERE `filename` = '%s'", file_size, filename_del);
		    sqlite3_exec(dbh, query, NULL, NULL, NULL);
		} else {
		    logger("SQLITE: Size of file '%s' is zero, deleting", filename_del);
		    unlink(filename_del);
		}
	    }
	}
    }
    sqlite3_finalize(select_stmt);
    #endif
    
    sqlite3_exec(dbh, "VACUUM", NULL, NULL, NULL);
    logger("SQLITE: VACUUM performed");
    
    #ifdef MKV
    pthread_create(&t_cleanup_thread, NULL, sqlite_cleanup_thread, NULL);
    #endif
    logger("SQLITE: Initialized");
}

void sqlite_uninit(void) {
    pthread_cancel(t_cleanup_thread);
    pthread_join(t_cleanup_thread, NULL);
    
    sqlite3_close(dbh);
    logger("SQLITE: Database closed");
}

#ifdef MKV
void sqlite_add_row(char *filename) {
    char query[256];
    CLEAR(query);
    sprintf(query, "INSERT INTO `capture` (`filename`, `size`) VALUES('%s', 0)", filename);
    sqlite3_exec(dbh, query, NULL, NULL, NULL);
}

void sqlite_update_row(char *filename, u_int64_t size) {
    char query[256];
    CLEAR(query);
    sprintf(query, "UPDATE `capture` SET `size` = %llu WHERE `filename` = '%s'", size, filename);
    sqlite3_exec(dbh, query, NULL, NULL, NULL);
}
#endif

#ifdef GPS
void sqlite_add_gps(double lat, double lon, double alt, double speed, double head, int sat, int fix) {
    char query[256];
    CLEAR(query);
    sprintf(query, "INSERT INTO `gps` (`lat`, `lon`, `alt`, `speed`, `head`, `sat`, `fix`) VALUES(%.9f, %.9f, %.2f, %.2f, %.2f, %d, %d)", lat, lon, alt, speed, head, sat, fix);
    sqlite3_exec(dbh, query, NULL, NULL, NULL);
}
#endif