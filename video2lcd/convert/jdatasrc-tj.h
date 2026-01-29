#ifndef JDATASRC_TJ_H
#define JDATASRC_TJ_H

#include <jpeglib.h>

/* 使用与定义一致的声明 */
GLOBAL(void) jpeg_mem_src_tj(j_decompress_ptr cinfo, unsigned char * inbuffer, unsigned long insize);

#endif