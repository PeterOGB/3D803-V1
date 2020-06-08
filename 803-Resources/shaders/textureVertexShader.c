/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
#version 300 es
uniform mat4 u_mvpMatrix;
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;

void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_texCoord = a_texCoord;
}


