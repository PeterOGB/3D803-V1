/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#version 300 es
uniform mat4 u_mvpMatrix;
uniform vec4 u_Translate;

layout(location = 0) in vec4 a_position;
void main()
{
    gl_Position = u_mvpMatrix * (a_position + u_Translate);
}

