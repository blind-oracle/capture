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
#include "jpeglib.h"
#include "jerror.h"

#include "jpeg_utils.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define MAX_LUMA_WIDTH   4096
#define MAX_CHROMA_WIDTH 2048

static unsigned char buf0[16][MAX_LUMA_WIDTH];
static unsigned char buf1[8][MAX_CHROMA_WIDTH];
static unsigned char buf2[8][MAX_CHROMA_WIDTH];
static unsigned char chr1[8][MAX_CHROMA_WIDTH];
static unsigned char chr2[8][MAX_CHROMA_WIDTH];

JSAMPROW row0[16] = { buf0[0],  buf0[1],  buf0[2],  buf0[3],
                      buf0[4],  buf0[5],  buf0[6],  buf0[7],
                      buf0[8],  buf0[9],  buf0[10], buf0[11],
                      buf0[12], buf0[13], buf0[14], buf0[15]};

JSAMPROW row1[8] =  { buf1[0], buf1[1], buf1[2], buf1[3],
                      buf1[4], buf1[5], buf1[6], buf1[7]};

JSAMPROW row2[16] = { buf2[0], buf2[1], buf2[2], buf2[3],
                      buf2[4], buf2[5], buf2[6], buf2[7]};

JSAMPROW row1_444[16], row2_444[16];

JSAMPARRAY scanarray[3] = {row0, row1, row2};

static int i, j, jpeg_image_size, width, height;
static unsigned char *input_image;

u_int8_t *yuv[3];
static JSAMPROW y[16], cb[16], cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)
static JSAMPARRAY data[3]; // t[0][2][5] = color sample 0 of row 2 and column 5

static struct jpeg_compress_struct cinfo;
static struct jpeg_decompress_struct dinfo;
static struct jpeg_error_mgr jerr;

typedef struct {
    struct jpeg_destination_mgr pub;
    JOCTET *buf;
    size_t bufsize;
    size_t jpegsize;
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;

static u_int8_t EOI_data[2] = { 0xFF, 0xD9 };

static void init_source(j_decompress_ptr cinfo) {
    /* no work necessary here */
}

static boolean fill_input_buffer(j_decompress_ptr cinfo) {
    cinfo->src->next_input_byte = EOI_data;
    cinfo->src->bytes_in_buffer = 2;
    
    return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
*/

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
    if (num_bytes > 0) {
	if (num_bytes > (long) cinfo->src->bytes_in_buffer)
	    num_bytes = (long) cinfo->src->bytes_in_buffer;
	
	cinfo->src->next_input_byte += (size_t) num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
    }
}

/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 */

static void term_source(j_decompress_ptr cinfo) {
    /* no work necessary here */
}

METHODDEF(void) init_destination(j_compress_ptr cinfo) {
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    dest->pub.next_output_byte = dest->buf;
    dest->pub.free_in_buffer = dest->bufsize;
    dest->jpegsize = 0;
}

METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo) {
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    dest->pub.next_output_byte = dest->buf;
    dest->pub.free_in_buffer = dest->bufsize;
    
    return FALSE;
}

METHODDEF(void) term_destination(j_compress_ptr cinfo) {
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    dest->jpegsize = dest->bufsize - dest->pub.free_in_buffer;
}

static GLOBAL(void) _jpeg_mem_dest(j_compress_ptr cinfo, JOCTET* buf, size_t bufsize) {
    mem_dest_ptr dest;
    
    if (cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)
                      (*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
                      sizeof(mem_destination_mgr));
    }
    
    dest = (mem_dest_ptr) cinfo->dest;
    
    dest->pub.init_destination    = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination    = term_destination;
    
    dest->buf      = buf;
    dest->bufsize  = bufsize;
    dest->jpegsize = 0;
}

static GLOBAL(int) jpeg_mem_size(j_compress_ptr cinfo) {
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    return dest->jpegsize;
}

static void add_huff_table(j_decompress_ptr dinfo, JHUFF_TBL **htblptr, 
                           const UINT8 *bits, const UINT8 *val)
/* Define a Huffman table */
{
    int nsymbols, len;
    
    if (*htblptr == NULL)
	*htblptr = jpeg_alloc_huff_table((j_common_ptr) dinfo);
	
    /* Copy the number-of-symbols-of-each-code-length counts */
    memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));
    
    /* Validate the counts.  We do this here mainly so we can copy the right
     * number of symbols from the val[] array, without risking marching off
     * the end of memory.  jchuff.c will do a more thorough test later.
     */
    nsymbols = 0;
    
    for (len = 1; len <= 16; len++)
        nsymbols += bits[len];
    
    if (nsymbols < 1 || nsymbols > 256)
        return;
    
    memcpy((*htblptr)->huffval, val, nsymbols * sizeof(UINT8));
}

static void std_huff_tables (j_decompress_ptr dinfo)
/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
{
    static const UINT8 bits_dc_luminance[17] =
    { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
    static const UINT8 val_dc_luminance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  
    static const UINT8 bits_dc_chrominance[17] =
    { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    static const UINT8 val_dc_chrominance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  
    static const UINT8 bits_ac_luminance[17] =
    { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
    static const UINT8 val_ac_luminance[] =
    { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
      0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
      0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
      0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
      0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
      0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
      0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
      0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
      0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
      0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
      0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };
  
    static const UINT8 bits_ac_chrominance[17] =
    { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
    static const UINT8 val_ac_chrominance[] =
    { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
      0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
      0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
      0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
      0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
      0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
      0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
      0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
      0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
      0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
      0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
      0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
      0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
      0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
      0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };
  
    add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[0],
                   bits_dc_luminance, val_dc_luminance);
    add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[0],
                   bits_ac_luminance, val_ac_luminance);
    add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[1],
                   bits_dc_chrominance, val_dc_chrominance);
    add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[1],
                   bits_ac_chrominance, val_ac_chrominance);
}

static void guarantee_huff_tables(j_decompress_ptr dinfo)
{
    if ((dinfo->dc_huff_tbl_ptrs[0] == NULL) &&
        (dinfo->dc_huff_tbl_ptrs[1] == NULL) &&
        (dinfo->ac_huff_tbl_ptrs[0] == NULL) &&
        (dinfo->ac_huff_tbl_ptrs[1] == NULL)) {
        std_huff_tables(dinfo);
    }
}

METHODDEF(void) my_jpeg_error_override(j_common_ptr cinfo, int msg_level) {
    
}

void init_jpeg(unsigned char *dest_image, unsigned char *s_input_image, int image_size, int s_width, int s_height, int quality) {
    width = s_width;
    height = s_height;
    input_image = s_input_image;
    
    yuv[0] = malloc(width * height * sizeof(yuv[0][0]));
    yuv[1] = malloc(width * height / 4 * sizeof(yuv[1][0]));
    yuv[2] = malloc(width * height / 4 * sizeof(yuv[2][0]));
    
    data[0] = y;
    data[1] = cb;
    data[2] = cr;
    
    jpeg_std_error(&jerr);
    jerr.emit_message = my_jpeg_error_override;
    
    cinfo.err = &jerr;
    dinfo.err = &jerr;
    
    jpeg_create_decompress(&dinfo);
    jpeg_create_compress(&cinfo);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    
    jpeg_set_defaults (&cinfo);
    cinfo.optimize_coding = TRUE;
    
    jpeg_set_colorspace(&cinfo, JCS_YCbCr);
    
    cinfo.raw_data_in = TRUE; // supply downsampled data
    
    //#if JPEG_LIB_VERSION > 62
    cinfo.do_fancy_downsampling = TRUE;  // fix segfault with v7
    //#endif
    
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;
    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 1;
    
    jpeg_set_quality(&cinfo, quality, TRUE);
    cinfo.dct_method = JDCT_FASTEST;
    
    _jpeg_mem_dest(&cinfo, dest_image, image_size);    // data written to mem
}

static void jpeg_skip_ff(j_decompress_ptr cinfo) {
    while (cinfo->src->bytes_in_buffer > 1 &&
	   cinfo->src->next_input_byte[0] == 0xff &&
	   cinfo->src->next_input_byte[1] == 0xff)
    {
	cinfo->src->bytes_in_buffer--;
	cinfo->src->next_input_byte++;
    }
}

static void jpeg_buffer_src(j_decompress_ptr cinfo, unsigned char *buffer, long num)
{
/* The source object and input buffer are made permanent so that a series
* of JPEG images can be read from the same buffer by calling jpeg_buffer_src
* only before the first one.  (If we discarded the buffer at the end of
* one image, we'd likely lose the start of the next one.)
* This makes it unsafe to use this manager and a different source
* manager serially with the same JPEG object.  Caveat programmer.
*/
    if (cinfo->src == NULL) {    /* first time for this JPEG object? */
	cinfo->src = (struct jpeg_source_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
			 sizeof (struct jpeg_source_mgr));
    }
    
    cinfo->src->init_source = init_source;
    cinfo->src->fill_input_buffer = fill_input_buffer;
    cinfo->src->skip_input_data = skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = term_source;
    cinfo->src->bytes_in_buffer = num;
    cinfo->src->next_input_byte = (JOCTET *) buffer;
}

void uninit_jpeg(void) {
    jpeg_destroy_compress(&cinfo);
    jpeg_destroy_decompress(&dinfo);
}

int encode_jpeg(void) {
    jpeg_start_compress (&cinfo, TRUE);
    
    for (j = 0; j < height; j += 16) {
	for (i = 0; i < 16; i++) {
	    y[i] = input_image + width * (i + j);
	    
	    if (i % 2 == 0) {
		cb[i/2] = input_image + width * height + width / 2 * ((i + j) / 2);
		cr[i/2] = input_image + width * height + width * height / 4 + width / 2 * ((i + j) / 2);
	    }
	}
        
	jpeg_write_raw_data(&cinfo, data, 16);
    }
    
    jpeg_finish_compress(&cinfo);
    jpeg_image_size = jpeg_mem_size(&cinfo);
    
    return jpeg_image_size;
}

static int decode_jpeg_raw (unsigned char *jpeg_data, int len,
                     int itype, int ctype, unsigned int width,
                     unsigned int height, unsigned char *raw0,
                     unsigned char *raw1, unsigned char *raw2)
{
    int numfields, hsf[3], field, yl, yc;
    int i, xsl, xsc, xs, hdown;
    unsigned int x, y = 0, vsf[3], xd;
    
    jpeg_buffer_src (&dinfo, jpeg_data, len);
    jpeg_read_header (&dinfo, TRUE);
    
    dinfo.raw_data_out = TRUE;
    dinfo.out_color_space = JCS_YCbCr;
    dinfo.dct_method = JDCT_IFAST;
    
    guarantee_huff_tables(&dinfo);
    jpeg_start_decompress (&dinfo);
    
    for (i = 0; i < 3; i++) {
	hsf[i] = dinfo.comp_info[i].h_samp_factor;
	vsf[i] = dinfo.comp_info[i].v_samp_factor;
    }
    
    if ((hsf[0] != 2 && hsf[0] != 1) || hsf[1] != 1 || hsf[2] != 1 ||
	(vsf[0] != 1 && vsf[0] != 2) || vsf[1] != 1 || vsf[2] != 1) {
	
	return -1;
    }
    
    if (hsf[0] == 1) {
        if (height % 8 != 0) {
	    return -1;
        }
	
	for (y = 0; y < 16; y++) { // allocate a special buffer for the extra sampling depth
	    row1_444[y] = (unsigned char *) malloc(dinfo.output_width * sizeof(char));
	    row2_444[y] = (unsigned char *) malloc(dinfo.output_width * sizeof(char));
	}
	
	scanarray[1] = row1_444; 
	scanarray[2] = row2_444; 
    }
    
    /* Height match image height or be exact twice the image height */
    if (dinfo.output_height == height) {
	numfields = 1;
    } else if (2 * dinfo.output_height == height) {
	numfields = 2;
    } else {
	return -1;
    }
    
    /* Width is more flexible */
    if (dinfo.output_width > MAX_LUMA_WIDTH) {
	return -1;
    }
    
    if (width < 2 * dinfo.output_width / 3) {
	/* Downsample 2:1 */
	hdown = 1;
        
	if(2 * width < dinfo.output_width)
	    xsl = (dinfo.output_width - 2 * width) / 2;
	else
	    xsl = 0;
    } else if (width == 2 * dinfo.output_width / 3) {
	/* special case of 3:2 downsampling */
	hdown = 2;
	xsl = 0;
    } else {
        /* No downsampling */
	hdown = 0;
	
	if(width < dinfo.output_width)
	    xsl = (dinfo.output_width - width) / 2;
	else
	    xsl = 0;
    }
    
    /* Make xsl even, calculate xsc */
    xsl = xsl & ~1;
    xsc = xsl / 2;
    
    yl = yc = 0;
    
    for (field = 0; field < numfields; field++) {
	if (field > 0) {
	    jpeg_read_header (&dinfo, TRUE);
	    dinfo.raw_data_out = TRUE;
	    dinfo.out_color_space = JCS_YCbCr;
	    dinfo.dct_method = JDCT_IFAST;
	    jpeg_start_decompress (&dinfo);
	}
	
	if (numfields == 2) {
	    switch (itype) {
		case Y4M_ILACE_TOP_FIRST:
		    yl = yc = field;
		break;
		case Y4M_ILACE_BOTTOM_FIRST:
		    yl = yc = (1 - field);
		break;
		default:
		    return -1;
	    }
	} else {
	    yl = yc = 0;
	}
	
	while (dinfo.output_scanline < dinfo.output_height) {
	    /* read raw data */
	    jpeg_read_raw_data (&dinfo, scanarray, 8 * vsf[0]);
	    
	    for (y = 0; y < 8 * vsf[0]; yl += numfields, y++) {
		xd = yl * width;
		xs = xsl;
		
		if (hdown == 0) {
		    for (x = 0; x < width; x++)
			raw0[xd++] = row0[y][xs++];
		} else if (hdown == 1) {
		    for (x = 0; x < width; x++, xs += 2)
			raw0[xd++] = (row0[y][xs] + row0[y][xs + 1]) >> 1;
		} else {
		    for (x = 0; x < width / 2; x++, xd += 2, xs += 3) {
			raw0[xd] = (2 * row0[y][xs] + row0[y][xs + 1]) / 3;
			raw0[xd + 1] = (2 * row0[y][xs + 2] + row0[y][xs + 1]) / 3;
		    }
		}
	    }
	    
	    /* Horizontal downsampling of chroma */
	    for (y = 0; y < 8; y++) {
		xs = xsc;
		if (hsf[0] == 1)
		    for (x = 0; x < width / 2; x++, xs++) { 		  
			row1[y][xs] = (row1_444[y][2*x] + row1_444[y][2*x + 1]) >> 1;
			row2[y][xs] = (row2_444[y][2*x] + row2_444[y][2*x + 1]) >> 1;
		    }
		
		xs = xsc;
		if (hdown == 0) {
		    for (x = 0; x < width / 2; x++, xs++) {
			chr1[y][x] = row1[y][xs];
			chr2[y][x] = row2[y][xs];
		    }
		} else if (hdown == 1) {
		    for (x = 0; x < width / 2; x++, xs += 2) {
			chr1[y][x] = (row1[y][xs] + row1[y][xs + 1]) >> 1;
			chr2[y][x] = (row2[y][xs] + row2[y][xs + 1]) >> 1;
		    }
		} else {
		    for (x = 0; x < width / 2; x += 2, xs += 3) {
			chr1[y][x] = (2 * row1[y][xs] + row1[y][xs + 1]) / 3;
			chr1[y][x + 1] = (2 * row1[y][xs + 2] + row1[y][xs + 1]) / 3;
			chr2[y][x] = (2 * row2[y][xs] + row2[y][xs + 1]) / 3;
			chr2[y][x + 1] = (2 * row2[y][xs + 2] + row2[y][xs + 1]) / 3;
		    }
		}
	    }

	    /* Vertical resampling of chroma */
	    switch (ctype) {
		case Y4M_CHROMA_422:
		    if (vsf[0] == 1) {
			/* Just copy */
			for (y = 0; y < 8 /*&& yc < height */; y++, yc += numfields) {
			    xd = yc * width / 2;
			    
			    for (x = 0; x < width / 2; x++, xd++) {
				raw1[xd] = chr1[y][x];
				raw2[xd] = chr2[y][x];
			    }
			}
		    } else {
			/* upsample */
			for (y = 0; y < 8 /*&& yc < height */; y++) {
			    xd = yc * width / 2;
			    
			    for (x = 0; x < width / 2; x++, xd++) {
				raw1[xd] = chr1[y][x];
				raw2[xd] = chr2[y][x];
			    }
			    
			    yc += numfields;
			    xd = yc * width / 2;
			    
			    for (x = 0; x < width / 2; x++, xd++) {
				raw1[xd] = chr1[y][x];
				raw2[xd] = chr2[y][x];
			    }
			    
			    yc += numfields;
			}
		    }
		break;
		
		default:
		    /*
		    * should be case Y4M_CHROMA_420JPEG: but use default: for compatibility. Some
		    * pass things like '420' in with the expectation that anything other than
		    * Y4M_CHROMA_422 will default to 420JPEG.
		    */
		    
		    if (vsf[0] == 1) {
			/* Really downsample */
			for (y = 0; y < 8 /*&& yc < height/2*/; y += 2, yc += numfields) {
			    xd = yc * width / 2;
			    
			    for (x = 0; x < width / 2; x++, xd++) {
				assert(xd < (width * height / 4));
				raw1[xd] = (chr1[y][x] + chr1[y + 1][x]) >> 1;
				raw2[xd] = (chr2[y][x] + chr2[y + 1][x]) >> 1;
			    }
			}
		    } else {
			/* Just copy */
			for (y = 0; y < 8 /*&& yc < height/2 */; y++, yc += numfields) {
			    xd = yc * width / 2;
			
			    for (x = 0; x < width / 2; x++, xd++) {
				raw1[xd] = chr1[y][x];
				raw2[xd] = chr2[y][x];
			    }
			}
		    }
		break;
	    }
	}
	
	(void) jpeg_finish_decompress (&dinfo);
	
	if (field == 0 && numfields > 1) {
	    jpeg_skip_ff (&dinfo);
	}
    }
    
    if (hsf[0] == 1) {
	for (y = 0; y < 16; y++) { // allocate a special buffer for the extra sampling depth
	    free(row1_444[y]);
	    free(row2_444[y]);
	}
    }
    
    return 0;
}

int mjpegtoyuv420p(unsigned char *dest, unsigned char *src, unsigned int size) {
    unsigned char *y, *u, *v;
    int loop;
    
    if(decode_jpeg_raw(src, size, 0, 420, width, height, yuv[0], yuv[1], yuv[2]) < 0) {
	return -1;
    }
    
    y = dest;
    u = y + width * height;
    v = u + (width * height) / 4;
    
    memset(y, 0, width * height);
    memset(u, 0, width * height / 4);
    memset(v, 0, width * height / 4);
    
    for(loop = 0; loop < width * height; loop++)
	*dest++=yuv[0][loop];
    
    for(loop = 0; loop < width * height / 4; loop++)
	*dest++=yuv[1][loop];
    
    for(loop = 0; loop < width * height / 4; loop++)
	*dest++=yuv[2][loop];
    
    return 0;
}
