/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#version 300 es 
precision mediump float;

in vec4 fragmentNormal;
in vec4 cameraVector;
in vec4 lightVector;

const float MAX_DIST = 50.0;
const float MAX_DIST_SQUARED = MAX_DIST * MAX_DIST;
const vec3 AMBIENT = vec3(0.4, 0.4, 0.4);

uniform vec4 u_HandColour;

layout(location = 0) out vec4 outColor;
//varying vec3 light;
void main()
{
	vec3 lightColor = vec3(1.0,0.9,0.9);

	vec3 diffuse = vec3(0.0, 0.0, 0.0);
	vec3 specular = vec3(0.0, 0.0, 0.0);

	vec4 normal = normalize(fragmentNormal);
	vec4 cameraDir = normalize(cameraVector);

	float dist = min(dot(lightVector, lightVector), 
	      	   MAX_DIST_SQUARED) / MAX_DIST_SQUARED;
	float distFactor = 1.0 - dist;
//	float distFactor = 1.0;
	vec4 lightDir = normalize(lightVector);
	float diffuseDot = dot(normal, lightDir);
	diffuse += lightColor * clamp(diffuseDot, 0.0, 1.0) * distFactor;

	vec4 halfAngle = normalize(cameraDir + lightDir);
	vec3 specularColor = min(lightColor + 0.5, 1.0);
	float specularDot = dot(normal, halfAngle);
	specular += specularColor * pow(clamp(specularDot, 0.0, 1.0), 16.0) * distFactor;

	outColor = vec4(clamp(u_HandColour.rgb * (diffuse + AMBIENT) + specular, 0.0, 1.0), u_HandColour.a);

    
//	outColor = vec4(1.0,0.5,0.5,1.0);
}
