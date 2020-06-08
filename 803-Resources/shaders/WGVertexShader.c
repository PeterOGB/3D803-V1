/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#version 300 es
uniform vec4 u_cameraPosition;
uniform vec4 u_lightPosition;
uniform mat4 u_mvpMatrix;
uniform mat4 u_rotate;
uniform bool u_rotflag;
uniform vec4 u_Translate;
//uniform vec4 u_PreTranslate;

layout(location = 1) in vec4 a_position;
layout(location = 0) in vec4 a_normal;

out vec4 cameraVector;
out vec4 lightVector;
out vec4 fragmentNormal;

void main()
{
    vec4 posn;
    float factor;
    mat4 rot;
    if(u_rotflag)
    {
	posn =  (u_rotate * a_position) + u_Translate;
	fragmentNormal = u_rotate * a_normal;
    }
    else
    {
	posn =  a_position + u_Translate;
	fragmentNormal = a_normal;
    }
	
    cameraVector = posn -  u_cameraPosition;
    
    lightVector = u_lightPosition - posn;

    gl_Position = u_mvpMatrix * posn ;
}
