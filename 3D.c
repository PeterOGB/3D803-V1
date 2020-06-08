/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/



#define G_LOG_USE_STRUCTURED
#define _GNU_SOURCE

#include <glib.h>
#include <cglm/cglm.h>
#include <GLES3/gl3.h>

#include "Parse.h"
#include "Common.h"

#include "3D.h"

vec4 WGLightPosition = {0.0f,16.0f,8.0f,0.0};
vec3 UserHTP = {0.0f,0.0f,0.0f};   // Heading,Tilt,Pan
vec3 UserXYZ = {0.0f,0.0f,-2.5f};


// Update MVP matrix
void UpdateMVP(int windowWidth, int  windowHeight)
{
    mat4 modelview, perspective;
    float   xv,yv,zv;
    float aspect;

    // Compute the window aspect ratio
    aspect =     (GLfloat) windowWidth / (GLfloat) windowHeight;
    // Generate a perspective matrix with a 15 degree FOV
    glm_perspective(30.0f*M_PIf32/180.0f, aspect, 1.0f, 25.0f, perspective);

    // Generate a model view matrix to rotate/translate the cube
    glm_mat4_identity(modelview);

    // Rotate to the direction you are looking
    glm_rotate(modelview,UserHTP[HEADING], (vec3) {0.0,1.0,0.0});
    
    zv = -sinf(UserHTP[HEADING]);
    xv =  cosf(UserHTP[HEADING]);
    
    glm_rotate(modelview,UserHTP[TILT], (vec3) {xv,0.0,zv});
    
    // Calculate heading vector in xz plane
    zv = cosf(UserHTP[HEADING]);
    xv = sinf(UserHTP[HEADING]);
    
    zv *=  sinf(UserHTP[TILT]);
    xv *=  sinf(UserHTP[TILT]);

    yv = cosf(UserHTP[TILT]);
    
    glm_rotate(modelview,UserHTP[PAN], (vec3) {xv,yv,zv});
   
    // Compute the final MVP by multiplying the 
    // modevleiw and perspective matrices together

    // Translate away from the viewer
    glm_translate(modelview,UserXYZ);

    glm_mat4_mul( perspective,  modelview, mvpMatrix);

    glm_mat4_inv(mvpMatrix,mvpMatrixInv);
}

// Convert mouse movements into changes in UserXYZ and UserHTP
void UpdateUser(GLfloat deltaX,GLfloat deltaY, gboolean shiftPressed,gboolean controlPressed)
{

    if(shiftPressed && controlPressed)
    {
		    UserHTP[HEADING] += deltaX * 0.001f;
	    UserXYZ[1] += deltaY * 0.01f;

    }
    else
    {
	if(shiftPressed)
	{
	    UserHTP[PAN] += deltaX * 0.001f;
	    UserHTP[TILT] += deltaY * 0.001f;
	}
			
	if(controlPressed)
	{
	UserXYZ[2] += 0.01f * sinf(2.0f * M_PIf32 *
				      UserHTP[HEADING]/360.0f) * deltaX;
	UserXYZ[0] -= 0.01f * cosf(2.0f * M_PIf32 *
				      UserHTP[HEADING]/360.0f) * deltaX;
		    
	UserXYZ[2] -= 0.01f * cosf(2.0f * M_PIf32 *
				      UserHTP[HEADING]/360.0f) * deltaY;
	UserXYZ[0] -= 0.01f * sinf(2.0f * M_PIf32 *
				      UserHTP[HEADING]/360.0f) * deltaY;
	}
    }
}



// Write UserHTP and UserXYZ to config file
void SaveScene(__attribute__((unused)) GString *sharedPath,
	       GString *userPath)
{
    GString *configText;

    configText = g_string_new("# 3D configuration\n");

    g_string_append_printf(configText,"Direction %f %f %f\n",
			   (double)UserHTP[HEADING],
			   (double)UserHTP[TILT],
			   (double)UserHTP[PAN]);
    g_string_append_printf(configText,"Position %f %f %f\n",
			   (double)UserXYZ[0],
			   (double)UserXYZ[1],
			   (double)UserXYZ[2]);
    
    updateConfigFile("3dState",userPath,configText);

    g_string_free(configText,TRUE);    
}


// Config file parsing

static int savedDirectionHandler(int nn)
{
    UserHTP[HEADING] = strtof(getField(nn+1),NULL);
    UserHTP[TILT] = strtof(getField(nn+2),NULL);
    UserHTP[PAN] = strtof(getField(nn+3),NULL);
    return TRUE;
}

static int savedPositionHandler(int nn)
{
    UserXYZ[0] = strtof(getField(nn+1),NULL);
    UserXYZ[1] = strtof(getField(nn+2),NULL);
    UserXYZ[2] = strtof(getField(nn+3),NULL);
    return TRUE;
}

static Token savedStateTokens[] = {
    {"Direction",0,savedDirectionHandler},
    {"Position",0,savedPositionHandler},
    {NULL,0,NULL}
};

gboolean LoadScene(__attribute__((unused)) GString *sharedPath,
		   GString *userPath)
{
    if(!readConfigFile("3dState",userPath,savedStateTokens))
	ResetScene();   // Set defaults

    return TRUE;
}

void ResetScene(void)
{
    UserHTP[HEADING] = 0.0f;
    UserHTP[TILT] = 0.60f;
    UserHTP[PAN] = 0.0f;
    UserXYZ[0] = 0.0f;
    UserXYZ[1] = -8.2f;
    UserXYZ[2] = -10.0f;
}
