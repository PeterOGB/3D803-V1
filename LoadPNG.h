/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#pragma once

struct pngData {
    GLuint textureId;
    GLubyte *pixels;
    unsigned int width;
    unsigned int height;
    unsigned int components;
};

struct pngData *loadPNG(const char *filename);

