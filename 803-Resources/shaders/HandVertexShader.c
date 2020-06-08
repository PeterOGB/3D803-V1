/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#version 300 es
uniform vec4 u_cameraPosition;
uniform vec4 u_lightPosition;
uniform mat4 u_mvpMatrix;
uniform mat4 u_armRotate;
uniform mat4 u_handRotate;
uniform vec4 u_handTranslate;
uniform vec4 u_armTranslate;
uniform float u_mirrorX;
//uniform vec4 u_PreTranslate;

layout(location = 1) in vec4 a_position;
layout(location = 0) in vec4 a_normal;

out vec4 cameraVector;
out vec4 lightVector;
out vec4 fragmentNormal;

void main()
{
    vec4 posn,norm;
    //float factor;
    

    if(a_position.z > 2.0)
    {
	// ARM
	posn = a_position;
	posn.z -= 2.0;
	posn.x *= u_mirrorX;
	//posn.x += 0.25;
	posn =  (u_armRotate * posn) + u_armTranslate;
	posn.z += 2.0;
	//posn.x -= 0.25;
	norm = a_normal;
	norm.x *= u_mirrorX;
	fragmentNormal = u_armRotate * norm;
    }
    else
    {
	// HAND
	posn = a_position;
	posn.x *= u_mirrorX;
	//posn.x = -posn.x;
	posn =  (u_handRotate * posn) + u_handTranslate;
	
	norm = a_normal;
	norm.x *= u_mirrorX;
	

	fragmentNormal = u_handRotate * norm;
    }
	
 
	
    cameraVector = u_cameraPosition -  posn;
    
    lightVector = u_lightPosition - posn;

    gl_Position = u_mvpMatrix * posn ;
}
