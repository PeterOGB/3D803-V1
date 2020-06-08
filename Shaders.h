/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#pragma once

#include <gmodule.h>

enum AUVtypes { AUV_PROG = 0,AUV_FSHADE,AUV_VSHADE,AUV_ATTR,AUV_UNIF,AUV_VARY,AUV_LAST };

struct AUV {
    int type;
    const char *name;
    union {
        GLint *Location;
        GLuint *ULocation;
    };
};

void SetUpShaders(GString *shaderPath);
extern struct AUV *shaders[];

