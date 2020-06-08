/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <GLES3/gl3.h>
#include <glib.h>


#include "Shaders.h"


int builder(GString *shaderPath,struct AUV *auvTable);

// Load Vertex or Fragment shader code from a file.
static GLuint loadShader(const char *filename,GLenum VorF)
{
    int shaderFd;
    GLchar *shaderText;
    GLuint shaderId;
    GLint compiledOk;
    struct stat buf;

    //debugOn();
    
    shaderId = glCreateShader(VorF);
    if(shaderId == 0)
    {
	g_debug("glCreateShader failed\n");
	return(0);
    }
    g_debug("opening %s\n",filename);
    
    shaderFd = open(filename,O_RDONLY);
    if(shaderFd == -1)
    {
	g_debug("Failed to open shader file %s : %s\n",filename,strerror(errno));
	return(0);
    }
    fstat(shaderFd,&buf);
    shaderText = (GLchar *) malloc((size_t)buf.st_size+1);
    if(shaderText == NULL)
    {
	g_debug("Malloc failed\n");
	close(shaderFd);
	return(0);
    }
    
    if(read(shaderFd,shaderText,(size_t)buf.st_size) != buf.st_size)
    {
	g_debug("Failed to read expected number of bytes from %s\n",filename);
	close(shaderFd);
	free(shaderText);
	return(0);
    }
    close(shaderFd);
    shaderText[buf.st_size] = '\0';

    glShaderSource(shaderId,1,(const GLchar * const *)&shaderText,NULL);

    free(shaderText);

    glCompileShader(shaderId);

    glGetShaderiv(shaderId,GL_COMPILE_STATUS,&compiledOk );

    if(compiledOk == GL_FALSE)
    {
	GLint logLength;

	glGetShaderiv(shaderId,GL_INFO_LOG_LENGTH,&logLength);
      
	if(logLength > 1)
	{
	    char *logText;
	    logText = malloc((size_t)logLength);

	    glGetShaderInfoLog(shaderId,logLength,NULL,logText);
	    g_debug("Shader from %s didn't compile!\n%s\n",filename,logText);            
         
	    free(logText);
      }

      glDeleteShader(shaderId);
      return 0;
    }
    //debugOff();
    return(shaderId);
}

int builder(GString *shaderPath,struct AUV *auvTable)
{
    struct AUV *auvp;
    enum AUVtypes type;
    GLuint fShaderId = 0;
    GLuint vShaderId = 0;
    GLuint progId = 0;
    GLint linkOk;
    GLenum e;
    
    auvp = auvTable;

    GString *fileName; 
    fileName = g_string_new(NULL);
   
    
    while((type = auvp->type) != AUV_LAST)
    {
	g_string_assign(fileName,shaderPath->str);
	g_string_append(fileName,auvp->name);
	switch(type)
	{
	case AUV_FSHADE:
	    fShaderId = loadShader(fileName->str,GL_FRAGMENT_SHADER);
	    g_debug("Fragment Shader (%s) id=%d\n",fileName->str,fShaderId);
	    progId = 0;
	    break;

	case AUV_VSHADE:
	    vShaderId = loadShader(fileName->str,GL_VERTEX_SHADER);
	    g_debug("Vertex Shader (%s) id=%d\n",fileName->str,vShaderId);
	    progId = 0;
	    break;

	case AUV_PROG:
	    progId  = glCreateProgram();
	    g_debug("Program (%s) id=%d\n",fileName->str,progId);
	    
	    if(progId == 0)
	    {
		glDeleteShader(vShaderId);
		glDeleteShader(fShaderId);
		return(-1);
	    }   
	    glAttachShader(progId,vShaderId);
	    glAttachShader(progId,fShaderId);
	    
	    glLinkProgram(progId);

	    glGetProgramiv(progId,GL_LINK_STATUS,&linkOk);

	    if(linkOk == GL_FALSE)
	    {
		GLint logLength;
		
		glGetProgramiv(progId,GL_INFO_LOG_LENGTH, &logLength);
		
		if(logLength > 1)
		{
		    char *logText;
		    logText = malloc((size_t)logLength);
		    
		    glGetProgramInfoLog(progId,logLength,NULL,logText);
		    g_debug("Linking %s failed: %s\n",
			    fileName->str,logText);            
		    
		    free(logText);     
		}
		glDeleteProgram(progId);
		return(-1);
	    }

	    glDeleteShader(vShaderId);
	    glDeleteShader(fShaderId);

	    vShaderId = fShaderId = 0;

	    *auvp->ULocation = progId;
	    break;

	case AUV_ATTR:
	    *auvp->ULocation = (GLuint) glGetAttribLocation(progId,auvp->name); 
	    g_debug("Attribute (%s,%d) = %d\n",auvp->name,progId,*auvp->Location);
	    break;

	case AUV_UNIF:
	    *auvp->Location =  glGetUniformLocation(progId,auvp->name); 
	    g_debug("Uniform (%s,%d) = %d\n",auvp->name,progId,*auvp->Location);
	    break;

	default:
	    g_debug("Error, unknown AUV type %d\n",type);
	    break;
	}
	e = glGetError();
	g_debug("e=0x%x\n",e);

	auvp++;
    }
    return(0);
}

void SetUpShaders(GString *shaderPath)
{
    for(int n = 0; shaders[n] != NULL; n++)
	builder(shaderPath,shaders[n]);
}
