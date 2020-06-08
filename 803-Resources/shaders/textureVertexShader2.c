#version 300 es
uniform mat4 u_mvpMatrix;
uniform vec4 u_handTranslate;
uniform mat4 u_rotate;

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;

void main()
{
    vec4 posn;

    posn = (u_rotate * a_position) + u_handTranslate;
    
    gl_Position = u_mvpMatrix * posn;
    v_texCoord = a_texCoord;
}


