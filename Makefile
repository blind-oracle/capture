# GPS HTTPD MKV AUDIO FFMPEG JPEG
FEATURES=HTTPD FFMPEG JPEG MKV GPS

ifneq (,$(findstring MKV,$(FEATURES)))
ifeq (,$(findstring SQLITE,$(FEATURES)))
    FEATURES += SQLITE
endif
endif

ifneq (,$(findstring GPS,$(FEATURES)))
ifeq (,$(findstring SQLITE,$(FEATURES)))
    FEATURES += SQLITE
endif
endif

OPTIMIZE_X86_64=-O2 -march=core2 -mtune=core2 -mfpmath=sse -fomit-frame-pointer -pipe
OPTIMIZE_X86=-O2 -march=core2 -mtune=core2 -mfpmath=sse -fomit-frame-pointer -pipe -m32
OPTIMIZE_ARM=-O2 -march=armv5te -mtune=xscale -fomit-frame-pointer -pipe

ARCH_X86_64=x86_64-pc-linux-gnu
ARCH_X86=x86_64-pc-linux-gnu
ARCH_ARM=armv5tel-softfloat-linux-gnueabi

COMPILER_PREFIX=/usr/bin

CC_X86_64=$(COMPILER_PREFIX)/$(ARCH_X86_64)-gcc
CC_X86=$(COMPILER_PREFIX)/$(ARCH_X86)-gcc
CC_ARM=$(COMPILER_PREFIX)/$(ARCH_ARM)-gcc

LIBPREFIX=/opt/capture-libs
LIBPREFIX_X86_64=$(LIBPREFIX)/x86_64
LIBPREFIX_X86=$(LIBPREFIX)/x86
LIBPREFIX_ARM=$(LIBPREFIX)/arm

LIBJPEG_DIR_X86_64=$(LIBPREFIX_X86_64)/libjpeg-turbo
LIBCONFUSE_DIR_X86_64=$(LIBPREFIX_X86_64)/libconfuse
GPSD_DIR_X86_64=$(LIBPREFIX_X86_64)/gpsd
SQLITE_DIR_X86_64=$(LIBPREFIX_X86_64)/libsqlite
FFMPEG_DIR_X86_64=$(LIBPREFIX_X86_64)/ffmpeg
ALSA_DIR_X86_64=$(LIBPREFIX_X86_64)/alsa-lib
LAME_DIR_X86_64=$(LIBPREFIX_X86_64)/lame
ZLIB_DIR_X86_64=$(LIBPREFIX_X86_64)/zlib

LIBJPEG_DIR_X86=$(LIBPREFIX_X86)/libjpeg-turbo
LIBCONFUSE_DIR_X86=$(LIBPREFIX_X86)/libconfuse
GPSD_DIR_X86=$(LIBPREFIX_X86)/gpsd
SQLITE_DIR_X86=$(LIBPREFIX_X86)/libsqlite
FFMPEG_DIR_X86=$(LIBPREFIX_X86)/ffmpeg
ALSA_DIR_X86=$(LIBPREFIX_X86)/alsa-lib
LAME_DIR_X86=$(LIBPREFIX_X86)/lame
ZLIB_DIR_X86=$(LIBPREFIX_X86)/zlib

LIBJPEG_DIR_ARM=$(LIBPREFIX_ARM)/libjpeg-turbo
LIBCONFUSE_DIR_ARM=$(LIBPREFIX_ARM)/libconfuse
GPSD_DIR_ARM=$(LIBPREFIX_ARM)/gpsd
SQLITE_DIR_ARM=$(LIBPREFIX_ARM)/libsqlite
FFMPEG_DIR_ARM=$(LIBPREFIX_ARM)/ffmpeg
ALSA_DIR_ARM=$(LIBPREFIX_ARM)/alsa-lib
LAME_DIR_ARM=$(LIBPREFIX_ARM)/lame
ZLIB_DIR_ARM=$(LIBPREFIX_ARM)/zlib

#SYSLIBPREFIX_X86_64=/usr/$(ARCH_X86_64)/usr/lib
#SYSLIBPREFIX_X86=/usr/$(ARCH_X86)/usr/lib
SYSLIBPREFIX_X86_64=/usr/lib64
SYSLIBPREFIX_X86=/usr/lib32
SYSLIBPREFIX_ARM=/usr/$(ARCH_ARM)/usr/lib

CFLAGS_X86_64=\
-Wall \
-DX86_64 \
$(DEBUG) \
-I$(LIBCONFUSE_DIR_X86_64)/include \
-I/usr/$(ARCH_X86_64)/usr/include \
${OPTIMIZE_X86_64} \
-fPIC

CFLAGS_X86=\
-Wall \
-DX86 \
$(DEBUG) \
-I$(LIBCONFUSE_DIR_X86)/include \
-I/usr/$(ARCH_X86)/usr/include \
${OPTIMIZE_X86} \
-fPIC

CFLAGS_ARM=\
-Wall \
-DARM \
$(DEBUG) \
-I$(LIBCONFUSE_DIR_ARM)/include \
-I/usr/$(ARCH_ARM)/usr/include \
${OPTIMIZE_ARM} \
-fPIC

LDFLAGS_X86_64=$(LIBCONFUSE_DIR_X86_64)/lib/libconfuse.a
LDFLAGS_X86=$(LIBCONFUSE_DIR_X86)/lib/libconfuse.a
LDFLAGS_ARM=$(LIBCONFUSE_DIR_ARM)/lib/libconfuse.a

ifneq (,$(findstring JPEG,$(FEATURES)))
    CFLAGS_ARM += -DJPEG -I$(LIBJPEG_DIR_ARM)/include
    CFLAGS_X86_64 += -DJPEG -I$(LIBJPEG_DIR_X86_64)/include
    CFLAGS_X86 += -DJPEG -I$(LIBJPEG_DIR_X86)/include
    
    LDFLAGS_X86_64 += $(LIBJPEG_DIR_X86_64)/lib/libturbojpeg.a
    LDFLAGS_X86 += $(LIBJPEG_DIR_X86)/lib/libturbojpeg.a
    LDFLAGS_ARM += $(LIBJPEG_DIR_ARM)/lib/libjpeg.a
endif

ifneq (,$(findstring GPS,$(FEATURES)))
    CFLAGS_ARM += -DGPS -I$(GPSD_DIR_ARM)/include
    CFLAGS_X86_64 += -DGPS -I$(GPSD_DIR_X86_64)/include
    CFLAGS_X86 += -DGPS -I$(GPSD_DIR_X86)/include
    
    LDFLAGS_X86_64 += $(GPSD_DIR_X86_64)/lib/libgps.a
    LDFLAGS_X86 += $(GPSD_DIR_X86)/lib/libgps.a
    LDFLAGS_ARM += $(GPSD_DIR_ARM)/lib/libgps.a
endif

ifneq (,$(findstring HTTPD,$(FEATURES)))
    CFLAGS_ARM += -DHTTPD
    CFLAGS_X86_64 += -DHTTPD
    CFLAGS_X86 += -DHTTPD
endif

ifneq (,$(findstring MKV,$(FEATURES)))
    CFLAGS_ARM += -DMKV
    CFLAGS_X86_64 += -DMKV
    CFLAGS_X86 += -DMKV
endif

ifneq (,$(findstring SQLITE,$(FEATURES)))
    CFLAGS_ARM += -DSQLITE -I$(SQLITE_DIR_ARM)/include
    CFLAGS_X86_64 += -DSQLITE -I$(SQLITE_DIR_X86_64)/include
    CFLAGS_X86 += -DSQLITE -I$(SQLITE_DIR_X86)/include
    
    LDFLAGS_X86_64 += $(SQLITE_DIR_X86_64)/lib/libsqlite3.a
    LDFLAGS_X86 += $(SQLITE_DIR_X86)/lib/libsqlite3.a
    LDFLAGS_ARM += $(SQLITE_DIR_ARM)/lib/libsqlite3.a
endif

ifneq (,$(findstring FFMPEG,$(FEATURES)))
    CFLAGS_ARM += -DFFMPEG -I$(FFMPEG_DIR_ARM)/include
    CFLAGS_X86_64 += -DFFMPEG -I$(FFMPEG_DIR_X86_64)/include
    CFLAGS_X86 += -DFFMPEG -I$(FFMPEG_DIR_X86)/include
    
    LDFLAGS_X86_64 += \
	$(FFMPEG_DIR_X86_64)/lib/libavcodec.a \
	$(FFMPEG_DIR_X86_64)/lib/libavformat.a \
	$(FFMPEG_DIR_X86_64)/lib/libavfilter.a \
	$(FFMPEG_DIR_X86_64)/lib/libavutil.a
    
    LDFLAGS_X86 += \
	$(FFMPEG_DIR_X86)/lib/libavcodec.a \
	$(FFMPEG_DIR_X86)/lib/libavformat.a \
	$(FFMPEG_DIR_X86)/lib/libavfilter.a \
	$(FFMPEG_DIR_X86)/lib/libavutil.a
    
    LDFLAGS_ARM += \
	$(FFMPEG_DIR_ARM)/lib/libavcodec.a \
	$(FFMPEG_DIR_ARM)/lib/libavformat.a \
	$(FFMPEG_DIR_ARM)/lib/libavfilter.a \
	$(FFMPEG_DIR_ARM)/lib/libavutil.a

endif

ifneq (,$(findstring AUDIO,$(FEATURES)))
    CFLAGS_ARM += -DAUDIO -I$(ALSA_DIR_ARM)/include -I$(LAME_DIR_ARM)/include
    CFLAGS_X86_64 += -DAUDIO -I$(ALSA_DIR_X86_64)/include -I$(LAME_DIR_X86_64)/include
    CFLAGS_X86 += -DAUDIO -I$(ALSA_DIR_X86)/include -I$(LAME_DIR_X86)/include
    
    LDFLAGS_X86_64 += \
	$(ALSA_DIR_X86_64)/lib/libsalsa.a \
	$(LAME_DIR_X86_64)/lib/libmp3lame.a
    
    LDFLAGS_X86 += \
	$(ALSA_DIR_X86)/lib/libsalsa.a \
	$(LAME_DIR_X86)/lib/libmp3lame.a
    
    LDFLAGS_ARM += \
	$(ALSA_DIR_ARM)/lib/libsalsa.a \
	$(LAME_DIR_ARM)/lib/libmp3lame.a
endif

LDFLAGS_X86_64 += \
$(SYSLIBPREFIX_X86_64)/libpthread.a \
$(SYSLIBPREFIX_X86_64)/libm.a \
$(ZLIB_DIR_X86_64)/lib/libz.a \
-static \
-fPIC

LDFLAGS_X86 += \
$(SYSLIBPREFIX_X86)/libpthread.a \
$(SYSLIBPREFIX_X86)/libm.a \
$(ZLIB_DIR_X86)/lib/libz.a \
-static \
-fPIC

LDFLAGS_ARM += \
$(SYSLIBPREFIX_ARM)/libpthread.a \
$(SYSLIBPREFIX_ARM)/libm.a \
$(ZLIB_DIR_ARM)/lib/libz.a \
-static \
-fPIC

OBJECTS_X86_64=.OBJ/x86_64/capture.o .OBJ/x86_64/v4l2.o .OBJ/x86_64/v4l2_controls.o .OBJ/x86_64/utils.o
OBJECTS_X86=.OBJ/x86/capture.o .OBJ/x86/v4l2.o .OBJ/x86/v4l2_controls.o .OBJ/x86/utils.o
OBJECTS_ARM=.OBJ/arm/capture.o .OBJ/arm/v4l2.o .OBJ/arm/v4l2_controls.o .OBJ/arm/utils.o

ifneq (,$(findstring JPEG,$(FEATURES)))
    OBJECTS_X86_64 += .OBJ/x86_64/jpeg_utils.o .OBJ/x86_64/draw_text.o
    OBJECTS_X86 += .OBJ/x86/jpeg_utils.o .OBJ/x86/draw_text.o
    OBJECTS_ARM += .OBJ/arm/jpeg_utils.o .OBJ/arm/draw_text.o
endif

ifneq (,$(findstring GPS,$(FEATURES)))
    OBJECTS_X86_64 += .OBJ/x86_64/gpsd.o
    OBJECTS_X86 += .OBJ/x86/gpsd.o
    OBJECTS_ARM += .OBJ/arm/gpsd.o
endif

ifneq (,$(findstring HTTPD,$(FEATURES)))
    OBJECTS_X86_64 += .OBJ/x86_64/httpd.o
    OBJECTS_X86 += .OBJ/x86/httpd.o
    OBJECTS_ARM += .OBJ/arm/httpd.o
endif

ifneq (,$(findstring MKV,$(FEATURES)))
    OBJECTS_X86_64 += .OBJ/x86_64/mkv_utils.o .OBJ/x86_64/matroska_ebml.o
    OBJECTS_X86 += .OBJ/x86/mkv_utils.o .OBJ/x86/matroska_ebml.o
    OBJECTS_ARM += .OBJ/arm/mkv_utils.o .OBJ/arm/matroska_ebml.o
endif

ifneq (,$(findstring SQLITE,$(FEATURES)))
    OBJECTS_X86_64 += .OBJ/x86_64/db.o
    OBJECTS_X86 += .OBJ/x86/db.o
    OBJECTS_ARM += .OBJ/arm/db.o
endif

ifneq (,$(findstring AUDIO,$(FEATURES)))
    OBJECTS_X86_64 += .OBJ/x86_64/audio.o
    OBJECTS_X86 += .OBJ/x86/audio.o
    OBJECTS_ARM += .OBJ/arm/audio.o
endif

all: capture.x86_64 capture.x86 capture.arm
	echo "$(OPTIMIZE_X86_64)" > /opt/optimize.x86_64
	echo "$(OPTIMIZE_X86)" > /opt/optimize.x86
	echo "$(OPTIMIZE_ARM)" > /opt/optimize.arm

clean:
	rm -f .OBJ/x86_64/*.o
	rm -f .OBJ/x86/*.o
	rm -f .OBJ/arm/*.o
	rm -f capture.arm
	rm -f capture.x86_64
	rm -f capture.x86

## X86_64
capture.x86_64: $(OBJECTS_X86_64)
	${CC_X86_64} -o capture.x86_64 $(OBJECTS_X86_64) $(LDFLAGS_X86_64)
	strip capture.x86_64

.OBJ/x86_64/capture.o: capture.c
	${CC_X86_64} $(CFLAGS_X86_64) -c capture.c -o .OBJ/x86_64/capture.o

.OBJ/x86_64/draw_text.o: draw_text.c draw_text.h
	${CC_X86_64} $(CFLAGS_X86_64) -c draw_text.c -o .OBJ/x86_64/draw_text.o

.OBJ/x86_64/jpeg_utils.o: jpeg/jpeg_utils.c jpeg/jpeg_utils.h
	${CC_X86_64} $(CFLAGS_X86_64) -c jpeg/jpeg_utils.c -o .OBJ/x86_64/jpeg_utils.o

.OBJ/x86_64/mkv_utils.o: mkv/mkv_utils.c mkv/mkv_utils.h
	${CC_X86_64} $(CFLAGS_X86_64) -c mkv/mkv_utils.c -o .OBJ/x86_64/mkv_utils.o

.OBJ/x86_64/matroska_ebml.o: mkv/matroska_ebml.c mkv/matroska_ebml.h
	${CC_X86_64} $(CFLAGS_X86_64) -c mkv/matroska_ebml.c -o .OBJ/x86_64/matroska_ebml.o

.OBJ/x86_64/v4l2.o: v4l2/v4l2.c v4l2/v4l2.h
	${CC_X86_64} $(CFLAGS_X86_64) -c v4l2/v4l2.c -o .OBJ/x86_64/v4l2.o

.OBJ/x86_64/v4l2_controls.o: v4l2/v4l2_controls.c v4l2/v4l2_controls.h
	${CC_X86_64} $(CFLAGS_X86_64) -c v4l2/v4l2_controls.c -o .OBJ/x86_64/v4l2_controls.o

.OBJ/x86_64/utils.o: utils.c utils.h
	${CC_X86_64} $(CFLAGS_X86_64) -c utils.c -o .OBJ/x86_64/utils.o

.OBJ/x86_64/httpd.o: httpd/httpd.c httpd/httpd.h
	${CC_X86_64} $(CFLAGS_X86_64) -c httpd/httpd.c -o .OBJ/x86_64/httpd.o

.OBJ/x86_64/gpsd.o: gpsd.c gpsd.h
	${CC_X86_64} $(CFLAGS_X86_64) -c gpsd.c -o .OBJ/x86_64/gpsd.o

.OBJ/x86_64/db.o: db.c db.h
	${CC_X86_64} $(CFLAGS_X86_64) -c db.c -o .OBJ/x86_64/db.o

.OBJ/x86_64/audio.o: audio/audio.c audio/audio.h
	${CC_X86_64} $(CFLAGS_X86_64) -c audio/audio.c -o .OBJ/x86_64/audio.o

## X86
capture.x86: $(OBJECTS_X86)
	${CC_X86} -o capture.x86 $(OBJECTS_X86) $(LDFLAGS_X86)
	strip capture.x86

.OBJ/x86/capture.o: capture.c
	${CC_X86} $(CFLAGS_X86) -c capture.c -o .OBJ/x86/capture.o

.OBJ/x86/draw_text.o: draw_text.c draw_text.h
	${CC_X86} $(CFLAGS_X86) -c draw_text.c -o .OBJ/x86/draw_text.o

.OBJ/x86/jpeg_utils.o: jpeg/jpeg_utils.c jpeg/jpeg_utils.h
	${CC_X86} $(CFLAGS_X86) -c jpeg/jpeg_utils.c -o .OBJ/x86/jpeg_utils.o

.OBJ/x86/mkv_utils.o: mkv/mkv_utils.c mkv/mkv_utils.h
	${CC_X86} $(CFLAGS_X86) -c mkv/mkv_utils.c -o .OBJ/x86/mkv_utils.o

.OBJ/x86/matroska_ebml.o: mkv/matroska_ebml.c mkv/matroska_ebml.h
	${CC_X86} $(CFLAGS_X86) -c mkv/matroska_ebml.c -o .OBJ/x86/matroska_ebml.o

.OBJ/x86/v4l2.o: v4l2/v4l2.c v4l2/v4l2.h
	${CC_X86} $(CFLAGS_X86) -c v4l2/v4l2.c -o .OBJ/x86/v4l2.o

.OBJ/x86/v4l2_controls.o: v4l2/v4l2_controls.c v4l2/v4l2_controls.h
	${CC_X86} $(CFLAGS_X86) -c v4l2/v4l2_controls.c -o .OBJ/x86/v4l2_controls.o

.OBJ/x86/utils.o: utils.c utils.h
	${CC_X86} $(CFLAGS_X86) -c utils.c -o .OBJ/x86/utils.o

.OBJ/x86/httpd.o: httpd/httpd.c httpd/httpd.h
	${CC_X86} $(CFLAGS_X86) -c httpd/httpd.c -o .OBJ/x86/httpd.o

.OBJ/x86/gpsd.o: gpsd.c gpsd.h
	${CC_X86} $(CFLAGS_X86) -c gpsd.c -o .OBJ/x86/gpsd.o

.OBJ/x86/db.o: db.c db.h
	${CC_X86} $(CFLAGS_X86) -c db.c -o .OBJ/x86/db.o

.OBJ/x86/audio.o: audio/audio.c audio/audio.h
	${CC_X86} $(CFLAGS_X86) -c audio/audio.c -o .OBJ/x86/audio.o

# ARM
capture.arm: $(OBJECTS_ARM)
	${CC_ARM} -o capture.arm $(OBJECTS_ARM) $(LDFLAGS_ARM)
	
#	${CC_ARM} -o gpsd_read.arm .OBJ/arm/gpsd_read.o \
#	    $(GPSD_DIR_ARM)/lib/libgps.a \
#	    $(SYSLIBPREFIX_ARM)/libpthread.a \
#	    $(SYSLIBPREFIX_ARM)/libm.a \
#	    $(ZLIB_DIR_ARM)/lib/libz.a \
#	    -static
	
	armv5tel-softfloat-linux-gnueabi-strip capture.arm

.OBJ/arm/gpsd_read.o: gpsd_read.c
	${CC_ARM} $(CFLAGS_ARM) -c gpsd_read.c -o .OBJ/arm/gpsd_read.o

.OBJ/arm/capture.o: capture.c
	${CC_ARM} $(CFLAGS_ARM) -c capture.c -o .OBJ/arm/capture.o

.OBJ/arm/draw_text.o: draw_text.c draw_text.h
	${CC_ARM} $(CFLAGS_ARM) -c draw_text.c -o .OBJ/arm/draw_text.o

.OBJ/arm/jpeg_utils.o: jpeg/jpeg_utils.c jpeg/jpeg_utils.h
	${CC_ARM} $(CFLAGS_ARM) -c jpeg/jpeg_utils.c -o .OBJ/arm/jpeg_utils.o

.OBJ/arm/mkv_utils.o: mkv/mkv_utils.c mkv/mkv_utils.h
	${CC_ARM} $(CFLAGS_ARM) -c mkv/mkv_utils.c -o .OBJ/arm/mkv_utils.o

.OBJ/arm/matroska_ebml.o: mkv/matroska_ebml.c mkv/matroska_ebml.h
	${CC_ARM} $(CFLAGS_ARM) -c mkv/matroska_ebml.c -o .OBJ/arm/matroska_ebml.o

.OBJ/arm/v4l2.o: v4l2/v4l2.c v4l2/v4l2.h
	${CC_ARM} $(CFLAGS_ARM) -c v4l2/v4l2.c -o .OBJ/arm/v4l2.o

.OBJ/arm/v4l2_controls.o: v4l2/v4l2_controls.c v4l2/v4l2_controls.h
	${CC_ARM} $(CFLAGS_ARM) -c v4l2/v4l2_controls.c -o .OBJ/arm/v4l2_controls.o

.OBJ/arm/utils.o: utils.c utils.h
	${CC_ARM} $(CFLAGS_ARM) -c utils.c -o .OBJ/arm/utils.o

.OBJ/arm/httpd.o: httpd/httpd.c httpd/httpd.h
	${CC_ARM} $(CFLAGS_ARM) -c httpd/httpd.c -o .OBJ/arm/httpd.o

.OBJ/arm/gpsd.o: gpsd.c gpsd.h
	${CC_ARM} $(CFLAGS_ARM) -c gpsd.c -o .OBJ/arm/gpsd.o

.OBJ/arm/db.o: db.c db.h
	${CC_ARM} $(CFLAGS_ARM) -c db.c -o .OBJ/arm/db.o

.OBJ/arm/audio.o: audio/audio.c audio/audio.h
	${CC_ARM} $(CFLAGS_ARM) -c audio/audio.c -o .OBJ/arm/audio.o
