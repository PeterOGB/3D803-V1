/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>		// va_lists for glprint
#include <assert.h>

#include  <GLES3/gl3.h>
#include  <EGL/egl.h>
#include <glib.h>

#include <png.h>
#include "LoadPNG.h"


struct pngData *loadPNG(const char *filename)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    int bitDepth, colourType;
    struct pngData *returnData;
    
    FILE *pngFile = fopen(filename, "rb");
    
    debugOn();
   
    g_debug("%s loading %s %p\n",__FUNCTION__,filename,pngFile);

    assert(pngFile != NULL);
    
    if (!pngFile)
        return 0;

    png_byte sig[8];

    fread(&sig, 8, sizeof(png_byte), pngFile);
    rewind(pngFile);
    if (!png_check_sig(sig, 8)) {
        g_debug("png sig failure\n");
        return 0;
    }

    png_ptr =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr) {
        g_debug("png ptr not created\n");
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        g_debug("set jmp failed\n");
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        g_debug("cant get png info ptr\n");
        return 0;
    }

    png_init_io(png_ptr, pngFile);
    png_read_info(png_ptr, info_ptr);
    bitDepth = png_get_bit_depth(png_ptr, info_ptr);
    colourType = png_get_color_type(png_ptr, info_ptr);

    if (colourType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (colourType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);  // thanks to Jesse Jaara for bug fix for newer libpng...
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (bitDepth == 16)
        png_set_strip_16(png_ptr);
    else if (bitDepth < 8)
        png_set_packing(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    png_uint_32 width, height;
    png_get_IHDR(png_ptr, info_ptr, &width, &height,
                 &bitDepth, &colourType, NULL, NULL, NULL);

    unsigned int components;
    switch (colourType) {
    case PNG_COLOR_TYPE_GRAY:
        components = 1;
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        components = 2;
        break;
    case PNG_COLOR_TYPE_RGB:
        components = 3;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        components = 4;
        break;
    default:
        components = 0;
    }

    if (components == 0) {
        if (png_ptr)
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        g_debug("%s broken?\n", filename);
        return 0;
    }


    g_debug("%s width=%d height=%d components=%d\n",__FUNCTION__,width,height,components);
    
    GLubyte *pixels =
        (GLubyte *) malloc(sizeof(GLubyte) * (width * height * components));
    row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * height);

    unsigned int i;
    for (i = 0; i < height; ++i)
        row_pointers[(height-1) - i] =
            (png_bytep) (pixels + (i * width * components));

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    g_debug("%s has %i colour components\n",filename,components);
    
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    fclose(pngFile);
    free(row_pointers);

    returnData = calloc(sizeof(struct pngData),1);

    returnData->pixels = pixels;
    returnData->width = width;
    returnData->height = height;
    returnData->components = components;

    debugOff();

    return returnData;
}


