/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
#pragma once

#include <GLES3/gl3.h>

GLuint frameBuffer;

GLuint getDepth(GtkAllocation *allocation,GLint x,GLint y);
void GlesInit(GString *shaderPath,int windowWidth,int windowHeight);
void GlesDraw(GLsizei width,GLsizei height);
