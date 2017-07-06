void init_jpeg(unsigned char *dest_image, unsigned char *input_image, int image_size, int width, int height, int quality);
void uninit_jpeg(void);
int encode_jpeg(void);
int mjpegtoyuv420p(unsigned char *dest, unsigned char *src, unsigned int size);

#define Y4M_ILACE_NONE          0  /* non-interlaced, progressive frame    */
#define Y4M_ILACE_TOP_FIRST     1  /* interlaced, top-field first          */
#define Y4M_ILACE_BOTTOM_FIRST  2  /* interlaced, bottom-field first       */
#define Y4M_ILACE_MIXED         3  /* mixed, "refer to frame header"       */

#define Y4M_CHROMA_420JPEG      0  /* 4:2:0, H/V centered, for JPEG/MPEG-1 */
#define Y4M_CHROMA_420MPEG2     1  /* 4:2:0, H cosited, for MPEG-2         */
#define Y4M_CHROMA_420PALDV     2  /* 4:2:0, alternating Cb/Cr, for PAL-DV */
#define Y4M_CHROMA_444          3  /* 4:4:4, no subsampling, phew.         */
#define Y4M_CHROMA_422          4  /* 4:2:2, H cosited                     */
#define Y4M_CHROMA_411          5  /* 4:1:1, H cosited                     */
#define Y4M_CHROMA_MONO         6  /* luma plane only                      */
#define Y4M_CHROMA_444ALPHA     7  /* 4:4:4 with an alpha channel          */
