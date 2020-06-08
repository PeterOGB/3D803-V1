/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#include <stdio.h>
#include <GLES3/gl3.h>

#include "Shaders.h"
#include "ShaderDefinitions.h"

struct AUV SimpleProgram[] = {

    {AUV_VSHADE,"simpleVertexShader.c",{.Location = NULL}},
    {AUV_FSHADE,"simpleFragmentShader.c",{.Location = NULL}},
    {AUV_PROG,"simpleProg",{.ULocation = &simpleProg}},
    {AUV_ATTR,"a_Position",{.ULocation = &simplePositionLoc}},
    {AUV_UNIF,"u_ColourG",{.Location = &simpleColourLoc}},
    {AUV_UNIF,"u_mvpMatrix",{.Location = &simpleMvpLoc}},
    {AUV_LAST,NULL,{.Location = NULL}},
};

struct AUV TextureProgram[] = {
    {AUV_VSHADE,"textureVertexShader.c",{.Location = NULL}},
    {AUV_FSHADE,"textureFragmentShader.c",{.Location = NULL}},
    {AUV_PROG,"objLoaderProg",{.ULocation = &textureProg}},
    {AUV_ATTR,"a_position",{.ULocation = &texturePositionLoc}},
    {AUV_ATTR,"a_texCoord",{.ULocation = &textureTexelCoordLoc}},
    {AUV_UNIF,"u_mvpMatrix",{.Location = &textureMvpLoc}},
    {AUV_UNIF,"s_texture",{.Location = &textureTextureLoc}},
    {AUV_LAST,NULL,{.Location = NULL}},
};

struct AUV Texture2Program[] = {
    {AUV_VSHADE,"textureVertexShader2.c",{.Location = NULL}},
    {AUV_FSHADE,"textureFragmentShader2.c",{.Location = NULL}},
    {AUV_PROG,"objLoaderProg",{.ULocation = &texture2Prog}},
    {AUV_ATTR,"a_position",{.ULocation = &texture2PositionLoc}},
    {AUV_ATTR,"a_texCoord",{.ULocation = &texture2TexelCoordLoc}},
    {AUV_UNIF,"u_mvpMatrix",{.Location = &texture2MvpLoc}},
    {AUV_UNIF,"s_texture",{.Location = &texture2TextureLoc}},
    {AUV_UNIF,"u_handTranslate",{.Location = &Hand2TranslateLoc}},
    {AUV_UNIF,"u_rotate",{.Location = &texture2RotateLoc }},
    {AUV_LAST,NULL,{.Location = NULL}},
};




struct AUV SimpleTextureProgram[] = {
    {AUV_VSHADE,"simpleTextureVertexShader.c",{.Location = NULL}},
    {AUV_FSHADE,"simpleTextureFragmentShader.c",{.Location = NULL}},
    {AUV_PROG,"objLoaderProg",{.ULocation = &simpleTextureProg}},
    {AUV_ATTR,"a_position",{.ULocation = &simpleTexturePositionLoc}},
    {AUV_ATTR,"a_texCoord",{.ULocation = &simpleTextureTexelCoordLoc}},
    {AUV_UNIF,"s_texture",{.Location = &simpleTextureTextureLoc}},
    {AUV_LAST,NULL,{.Location = NULL}},
};

struct AUV WGProgram[] = {
    {AUV_VSHADE,"WGVertexShader.c",{.Location = NULL}},
    {AUV_FSHADE,"WGFragmentShader.c",{.Location = NULL}},
    {AUV_PROG,"WGProg",{.ULocation = &WGProg}},
    {AUV_ATTR,"a_position",{.ULocation = &WGPositionLoc }},
    {AUV_ATTR, "a_normal",{.ULocation = &WGNormalsLoc  } },
    {AUV_UNIF,"u_WGColour",{.Location = &WGColourLoc}},
    {AUV_UNIF,"u_mvpMatrix",{.Location = &WGMvpLoc }},
    {AUV_UNIF,"u_Translate",{.Location = &WGTranslateLoc}},
    {AUV_UNIF,"u_lightPosition",{.Location = &WGLightPositionLoc}},
    {AUV_UNIF,"u_cameraPosition",{.Location = &WGCameraPositionLoc}},
    {AUV_UNIF,"u_rotflag",{.Location = &WGRotFlagLoc }},
    {AUV_UNIF,"u_rotate",{.Location = &WGRotateLoc }},
    {AUV_LAST,NULL,{.Location = NULL}}
};

struct AUV XwiresProgram[] = {
    {AUV_VSHADE,"XwiresVertexShader.c",{.Location = NULL}},
    {AUV_FSHADE,"XwiresFragmentShader.c",{.Location = NULL}},
    {AUV_PROG,"XwiresprogramObject",{.ULocation = &XwiresProg}},
    {AUV_ATTR,"a_position",{.ULocation = &XwiresPositionLoc}},
    {AUV_UNIF,"u_mvpMatrix",{.Location = &XwiresMvpLoc }},
    {AUV_UNIF,"u_Translate",{.Location = &XwiresTranslateLoc}},
    {AUV_LAST,NULL,{.ULocation = NULL}}

};

struct AUV HandProgram[] = {
    {AUV_VSHADE,"HandVertexShader.c",{.Location = NULL}},
    {AUV_FSHADE,"HandFragmentShader.c",{.Location = NULL}},
    {AUV_PROG,"HandProg",{.ULocation = &HandProg}},
    {AUV_ATTR,"a_position",{.ULocation = &HandPositionLoc }},
    {AUV_ATTR, "a_normal",{.ULocation = &HandNormalsLoc  } },
    {AUV_UNIF,"u_HandColour",{.Location = &HandColourLoc}},
    {AUV_UNIF,"u_mvpMatrix",{.Location = &HandMvpLoc }},
    {AUV_UNIF,"u_handTranslate",{.Location = &HandTranslateLoc}},
    {AUV_UNIF,"u_armTranslate",{.Location = &ArmTranslateLoc}},
    {AUV_UNIF,"u_lightPosition",{.Location = &HandLightPositionLoc}},
    {AUV_UNIF,"u_cameraPosition",{.Location = &HandCameraPositionLoc}},
    {AUV_UNIF,"u_handRotate",{.Location = &HandRotateLoc }},
    {AUV_UNIF,"u_armRotate",{.Location = &ArmRotateLoc }},
    {AUV_UNIF,"u_mirrorX",{.Location = &HandMirrorXLoc }},
    {AUV_LAST,NULL,{.Location = NULL}}
};

struct AUV *shaders[] = {SimpleProgram,TextureProgram,Texture2Program,SimpleTextureProgram,WGProgram,
			 XwiresProgram,HandProgram,NULL};

