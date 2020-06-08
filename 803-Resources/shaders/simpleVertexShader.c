/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
#version 300 es
uniform mat4 u_mvpMatrix;
layout(location = 0) in vec4 a_Position;
void main()
{
    gl_Position = u_mvpMatrix * a_Position;
}

