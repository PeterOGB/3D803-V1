/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#version 300 es
precision mediump float;
uniform vec4 u_ColourG;
out vec4 fragColor;
void main()
{
    fragColor = u_ColourG; // vec4 ( 1.0, 0.0, 0.0, 1.0 ); //
}

