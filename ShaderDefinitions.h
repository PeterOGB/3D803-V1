/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

// Declarations of all the variables used in shaders

GLuint simpleProg;
GLuint simplePositionLoc;
GLint simpleColourLoc;
GLint simpleMvpLoc;

GLuint textureProg;
GLuint texturePositionLoc;
GLuint textureTexelCoordLoc;
GLint textureTextureLoc;
GLint textureMvpLoc;

GLuint texture2Prog;
GLuint texture2PositionLoc;
GLuint texture2TexelCoordLoc;
GLint texture2TextureLoc;
GLint texture2MvpLoc;
GLint texture2RotateLoc;
GLint Hand2TranslateLoc;


GLuint simpleTextureProg;
GLuint simpleTexturePositionLoc;
GLuint simpleTextureTexelCoordLoc;
GLint simpleTextureTextureLoc;

GLuint WGProg;
GLuint WGPositionLoc; 
GLuint WGNormalsLoc;
GLint WGColourLoc;
GLint WGMvpLoc;
GLint WGTranslateLoc;
GLint WGPreTranslateLoc;
GLint WGLightPositionLoc;
GLint WGCameraPositionLoc;
GLint WGRotFlagLoc;
GLint WGRotateLoc;

GLuint XwiresProg;
GLuint XwiresPositionLoc;
GLint XwiresTranslateLoc;
GLint  XwiresMvpLoc;

GLuint HandProg;
GLuint HandPositionLoc;  
GLuint HandNormalsLoc;
GLint HandColourLoc;
GLint HandMvpLoc;
GLint HandTranslateLoc;
GLint  ArmTranslateLoc;
GLint HandPreTranslateLoc;
GLint HandLightPositionLoc;
GLint HandCameraPositionLoc;
GLint HandRotFlagLoc;
GLint ArmRotateLoc;
GLint HandRotateLoc;
GLint HandMirrorXLoc;
