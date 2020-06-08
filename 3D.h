/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/


void UpdateMVP(int windowWidth, int  windowHeight);
#ifdef __gles2_gl3_h_
void UpdateUser(GLfloat deltaX,GLfloat deltaY, gboolean shift,gboolean control);
#endif

void SaveScene(GString *sharedPath,
	       GString *userPath);
gboolean LoadScene(GString *sharedPath,
		   GString *userPath);

void ResetScene(void);

enum HTP { HEADING=0,TILT,PAN};


#ifdef cglm_h
vec3 UserHTP;
vec3 UserXYZ;
vec4 WGLightPosition;
mat4  mvpMatrix;
mat4 mvpMatrixInv;
#endif
