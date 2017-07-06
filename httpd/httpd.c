#define _GNU_SOURCE
#include <sys/types.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "../utils.h"

#define max(a, b) ( ((a) > (b)) ? (a) : (b) )

#define TYPE_MJPG 0
#define TYPE_JPEG 1

#define THREADS 4

#define HEADERS "HTTP/1.0 200 OK\r\n" \
		"Connection: close\r\n" \
		"Max-Age: 0\r\n" \
		"Expires: 0\r\n" \
		"Cache-Control: no-cache, private\r\n" \
		"Pragma: no-cache\r\n"

#define CONTENT_TYPE_MJPG "Content-Type: multipart/x-mixed-replace; boundary=--BoundaryString\r\n\r\n"

#define HEADERS_FRAME_JPEG "Content-type: image/jpeg\r\n" \
			   "Content-Length: "

#define HEADERS_FRAME_MJPG "--BoundaryString\r\n" \
			"Content-type: image/jpeg\r\n" \
			"Content-Length: "

extern int _shutdown;
extern struct s_conf conf;

int listen_socket_mjpg = 0;
int listen_socket_jpeg = 0;
int client_socket = 0;

int httpd_clients = 0;

typedef struct {
    pthread_t thread_id;
    
    pthread_mutex_t mutex_connect;
    pthread_cond_t cond_connect;
    pthread_mutex_t mutex_frame;
    pthread_cond_t cond_frame;
    
    unsigned char *jpeg_buffer;
    int jpeg_buffer_size;
    
    int socket;
    int busy;
    int connected;
    int type;
} struct_thread;

struct_thread t_pool[THREADS];

pthread_t t_listen_thread;

// JPEG distribution thread
unsigned char *jpeg_thread_buffer = NULL;
int jpeg_thread_buffer_size = 0;
pthread_t t_jpeg_thread;
int jpeg_thread_busy = 0;
pthread_mutex_t mutex_jpeg_frame;
pthread_cond_t cond_jpeg_frame;

struct _thread_arg {
    int idx;
};

struct _thread_arg *thread_arg = NULL;

static void socket_linger(int socket, int onoff) {
    struct linger linger;
    linger.l_onoff = onoff;
    linger.l_linger = 120;
    
    setsockopt(socket, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
}

static void tune_socket(int socket) {
    int on = 1;
    int off = 0;
    int buffer_size = 16384;
    
    // Set buffers size
    setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
    
    // Set SO_REUSEADDR
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    // Set TCP_NODELAY
    setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    
    // Set KEEPALIVE
    setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &off, sizeof(off));
    
    // Set SO_LINGER
    socket_linger(socket, 1);
}

void close_socket(int socket) {
    shutdown(socket, SHUT_RDWR);
}

static int get_free_thread(void) {
    int i = 0;
    
    for(i = 0; i < THREADS; i++) {
	if(t_pool[i].connected == 0) {
	    return i;
	}
    }
    
    return -1;
}

static void listen_thread_cleanup(void *arg) {
    logger("HTTP: Listen thread: got cancellation request, exiting...");
    close_socket(listen_socket_jpeg);
    close_socket(listen_socket_mjpg);
}

static void jpeg_thread_cleanup(void *arg) {
    logger("HTTP: JPEG thread: got cancellation request, exiting...");
    free(jpeg_thread_buffer);
}

static void serve_thread_cleanup(void *arg) {
    int *idx = (int *) arg;
    logger("HTTP: Serve thread %d: got cancellation request, exiting...", *idx);
    
    if(t_pool[*idx].connected) {
	close(t_pool[*idx].socket);
    }
    
    free(t_pool[*idx].jpeg_buffer);
}

static void serve_thread_free(int idx) {
    shutdown(t_pool[idx].socket, SHUT_RDWR);
    
    t_pool[idx].connected = 0;
    t_pool[idx].busy = 0;
    
    httpd_clients--;
    logger("HTTP: Thread %d: free, HTTP clients now: %d", idx, httpd_clients);
}

static void *jpeg_thread(void *arg) {
    pthread_cleanup_push(jpeg_thread_cleanup, NULL);
    
    int i;
    
    while(!_shutdown) {
	pthread_mutex_lock(&mutex_jpeg_frame);
	pthread_cond_wait(&cond_jpeg_frame, &mutex_jpeg_frame);
	pthread_mutex_unlock(&mutex_jpeg_frame);
	
	jpeg_thread_busy = 1;
	
	for(i = 0; i < THREADS; i++) {
	    if(t_pool[i].connected == 1) {
		if(t_pool[i].busy == 0) {
		    memcpy(t_pool[i].jpeg_buffer, jpeg_thread_buffer, jpeg_thread_buffer_size);
		    t_pool[i].jpeg_buffer_size = jpeg_thread_buffer_size;
		    
		    pthread_mutex_lock(&t_pool[i].mutex_frame);
		    pthread_cond_signal(&t_pool[i].cond_frame);
		    pthread_mutex_unlock(&t_pool[i].mutex_frame);
		}
	    }
	}
	
	jpeg_thread_busy = 0;
    }
    
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

static void *serve_thread(void *arg) {
    struct timeval tv;
    fd_set rfds;
    
    struct _thread_arg *thread_arg = (struct _thread_arg *) arg;
    int idx = thread_arg->idx;
    free(thread_arg);
    
    pthread_cleanup_push(serve_thread_cleanup, (void *) &idx);
    
    char headers[128];
    
    while(!_shutdown) {
	pthread_mutex_lock(&t_pool[idx].mutex_connect);
	pthread_cond_wait(&t_pool[idx].cond_connect, &t_pool[idx].mutex_connect);
	pthread_mutex_unlock(&t_pool[idx].mutex_connect);
	
	FD_ZERO(&rfds);
	FD_SET(t_pool[idx].socket, &rfds);
	
	logger("HTTP: Thread %d: Awoken, serving socket %d", idx, t_pool[idx].socket);
	
	t_pool[idx].connected = 1;
	
	int bytes_sent = send(t_pool[idx].socket, HEADERS, strlen(HEADERS), MSG_NOSIGNAL);
	if(bytes_sent == -1) {
	    logger("HTTP: Thread %d: Cannot send HEADERS to socket %d", idx, t_pool[idx].socket);
	    serve_thread_free(idx);
	    continue;
	}
	
	if(t_pool[idx].type == TYPE_MJPG) {
	    bytes_sent = send(t_pool[idx].socket, CONTENT_TYPE_MJPG, strlen(CONTENT_TYPE_MJPG), MSG_NOSIGNAL);
	    
	    if(bytes_sent == -1) {
		logger("HTTP: Thread %d: Cannot send CONTENT_TYPE to socket %d", idx, t_pool[idx].socket);
		serve_thread_free(idx);
		continue;
	    }
	}
	
	while(!_shutdown) {
	    pthread_mutex_lock(&t_pool[idx].mutex_frame);
	    pthread_cond_wait(&t_pool[idx].cond_frame, &t_pool[idx].mutex_frame);
	    pthread_mutex_unlock(&t_pool[idx].mutex_frame);
	    
	    t_pool[idx].busy = 1;
	    
	    if(t_pool[idx].type == TYPE_MJPG) {
		bytes_sent = send(t_pool[idx].socket, HEADERS_FRAME_MJPG, strlen(HEADERS_FRAME_MJPG), MSG_NOSIGNAL);
	    } else if (t_pool[idx].type == TYPE_JPEG) {
		bytes_sent = send(t_pool[idx].socket, HEADERS_FRAME_JPEG, strlen(HEADERS_FRAME_JPEG), MSG_NOSIGNAL);
	    }
	    
	    if(bytes_sent <= 0) {
		logger("HTTP: Thread %d: Cannot send HEADERS_FRAME to socket %d, closing connection", idx, t_pool[idx].socket);
		serve_thread_free(idx);
		break;
	    } else {
		CLEAR(headers);
		sprintf(headers, "%d\r\n\r\n", t_pool[idx].jpeg_buffer_size);
		
		bytes_sent = send(t_pool[idx].socket, headers, strlen(headers), MSG_NOSIGNAL);
		if(bytes_sent > 0) {
		    bytes_sent = send(t_pool[idx].socket, t_pool[idx].jpeg_buffer, t_pool[idx].jpeg_buffer_size, MSG_NOSIGNAL);
		    if(bytes_sent <= 0) {
			serve_thread_free(idx);
			break;
		    }
		} else {
		    serve_thread_free(idx);
		    break;
		}
	    }
	    
	    tv.tv_sec = 120;
	    tv.tv_usec = 0;
	    select(t_pool[idx].socket + 1, &rfds, NULL, NULL, &tv);
	    
	    t_pool[idx].busy = 0;
	    
	    if(t_pool[idx].type == TYPE_JPEG) {
		logger("HTTP: Thread %d: JPEG image sent (%d bytes)", idx, bytes_sent);
		serve_thread_free(idx);
		break;
	    }
	}
	
	if(_shutdown) {
	    serve_thread_free(idx);
	}
    }
    
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

static void *listen_thread (void *arg) {
    int thread_idx = -1;
    int i = 0;
    int off = 0;
    struct timeval tv;
    fd_set rfds;
    
    int max_sd = max(listen_socket_jpeg, listen_socket_mjpg);
    
    pthread_cleanup_push(listen_thread_cleanup, NULL);
    while(!_shutdown) {
	tv.tv_sec = 180;
	tv.tv_usec = 0;
	
	FD_ZERO(&rfds);
	FD_SET(listen_socket_jpeg, &rfds);
	FD_SET(listen_socket_mjpg, &rfds);
	
	int ndesc = select(max_sd + 1, &rfds, NULL, NULL, &tv);
	
	if(ndesc > 0) {
	    for(i = 0; i <= max_sd && ndesc > 0; i++) {
		if(FD_ISSET(i, &rfds)) {
		    ndesc--;
		    
		    client_socket = accept(i, NULL, NULL);
		    ioctl(client_socket, FIONBIO, (char *) &off);
		    
		    tune_socket(client_socket);
		    
		    thread_idx = get_free_thread();
		    
		    if(thread_idx >= 0) {
			logger("HTTP: Listen thread: Got free thread %d", thread_idx);
			t_pool[thread_idx].socket = client_socket;
			
			if(i == listen_socket_mjpg)
			    t_pool[thread_idx].type = TYPE_MJPG;
			else if(i == listen_socket_jpeg)
			    t_pool[thread_idx].type = TYPE_JPEG;
			else {
			    logger("HTTP: Listen thread: Unknown listening socket: %d", i);
			    exit(EXIT_FAILURE);
			}
			
			pthread_mutex_lock(&t_pool[thread_idx].mutex_connect);
			pthread_cond_signal(&t_pool[thread_idx].cond_connect);
			pthread_mutex_unlock(&t_pool[thread_idx].mutex_connect);
			
			httpd_clients++;
		    } else {
			logger("HTTP: Listen thread: No free serve threads available, closing connection");
			close_socket(client_socket);
		    }
		}
	    }
	}
    }
    
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

static void httpd_init_thread_pool(void) {
    int i = 0;
    struct _thread_arg *thread_arg = NULL;
    
    for(i = 0; i < THREADS; i++) {
	thread_arg = (struct _thread_arg *) malloc(sizeof(struct _thread_arg));
	thread_arg->idx = i;
	
	t_pool[i].socket = 0;
	t_pool[i].connected = 0;
	t_pool[i].busy = 0;
	t_pool[i].type = TYPE_MJPG;
	
	pthread_mutex_init(&t_pool[i].mutex_connect, NULL);
	pthread_cond_init(&t_pool[i].cond_connect, NULL);
	
	pthread_mutex_init(&t_pool[i].mutex_frame, NULL);
	pthread_cond_init(&t_pool[i].cond_frame, NULL);
	
	t_pool[i].jpeg_buffer = (unsigned char *) malloc(conf.jpeg_buffer_size);
	t_pool[i].jpeg_buffer_size = 0;
	
	pthread_create(&t_pool[i].thread_id, NULL, serve_thread, (void *) thread_arg);
    }
    
    logger("HTTP: Thread pool initialized with %d threads", THREADS);
}

void httpd_init(unsigned int httpd_mjpeg_port, unsigned int httpd_jpeg_port) {
    struct sockaddr_in srv_mjpg, srv_jpeg;
    
    if((listen_socket_jpeg = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
	(listen_socket_mjpg = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	logger("HTTP: Cannot create listening socket: %s", strerror(errno));
	exit(1);
    }
    
    // Add non-blocking flag
    int on = 1;
    ioctl(listen_socket_mjpg, FIONBIO, (char *) &on);
    ioctl(listen_socket_jpeg, FIONBIO, (char *) &on);
    
    tune_socket(listen_socket_mjpg);
    tune_socket(listen_socket_jpeg);
    
    httpd_init_thread_pool();
    
    // Init JPEG thread stuff
    jpeg_thread_buffer = (unsigned char *) malloc(conf.jpeg_buffer_size);
    pthread_mutex_init(&mutex_jpeg_frame, NULL);
    pthread_cond_init(&cond_jpeg_frame, NULL);
    
    CLEAR(srv_mjpg);
    srv_mjpg.sin_family = AF_INET;
    srv_mjpg.sin_addr.s_addr = INADDR_ANY;
    srv_mjpg.sin_port = htons(httpd_mjpeg_port);
    
    CLEAR(srv_jpeg);
    srv_jpeg.sin_family = AF_INET;
    srv_jpeg.sin_addr.s_addr = INADDR_ANY;
    srv_jpeg.sin_port = htons(httpd_jpeg_port);
    
    if(bind(listen_socket_mjpg, (struct sockaddr *) &srv_mjpg, sizeof(srv_mjpg)) < 0 ||
       bind(listen_socket_jpeg, (struct sockaddr *) &srv_jpeg, sizeof(srv_jpeg)) < 0)
    {
	logger("HTTP: Cannot bind listening socket: %s", strerror(errno));
	exit(1);
    }
    
    if(listen(listen_socket_mjpg, 16) < 0 ||
       listen(listen_socket_jpeg, 16) < 0)
    {
	logger("HTTP: Cannot put socket into listen state: %s", strerror(errno));
	exit(1);
    }
    
    pthread_create(&t_jpeg_thread, NULL, jpeg_thread, NULL);
    pthread_create(&t_listen_thread, NULL, listen_thread, NULL);
}

void httpd_uninit(void) {
    int i = 0;
    
    pthread_cancel(t_listen_thread);
    pthread_join(t_listen_thread, NULL);
    pthread_cancel(t_jpeg_thread);
    pthread_join(t_jpeg_thread, NULL);
    
    for(i = 0; i < THREADS; i++) {
	pthread_cancel(t_pool[i].thread_id);
	pthread_join(t_pool[i].thread_id, NULL);
    }
    
    logger("HTTP: Shut down");
}
