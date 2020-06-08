#version 300 es 
precision mediump float;
in vec2 v_texCoord;
uniform sampler2D s_texture;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture( s_texture, v_texCoord );
    
}




