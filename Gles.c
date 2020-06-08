/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

// OpenGLES code
#define G_LOG_USE_STRUCTURED

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <cglm/cglm.h>

#include "Gles.h"
#include "Shaders.h"
#include "ShaderDefinitions.h"
#include "LoadPNG.h"

#include "ObjLoader.h"
#include "Keyboard.h"
#include "Hands.h"
#include "Logging.h"

static GLenum e;
#define CHECK(n) if((e=glGetError())!=0){g_warning("Error %s %x\n",n,e);} 
struct pngData *paperTexture;
static GLuint depthBuffer;



void GlesInit(GString *shaderPath,int windowWidth,int windowHeight)
{
    GLenum status;
    GLint no_of_extensions;
    const char *ext_name;
    gboolean extensionFound = FALSE;
	
    glGetIntegerv(GL_NUM_EXTENSIONS, &no_of_extensions);
    for ( int i = 0; i < no_of_extensions; ++i )
    {
	ext_name = (const char *) glGetStringi(GL_EXTENSIONS, (unsigned) i);
	if(strcmp("GL_NV_read_depth",ext_name) == 0)
	{
	    extensionFound = TRUE;
	}
    }
    if(!extensionFound)
    {
	g_info("GL_RENDERER   = %s\n", (const char *) glGetString (GL_RENDERER));
	g_info("GL_VERSION    = %s\n", (const char *) glGetString (GL_VERSION));
	g_info("GL_VENDOR     = %s\n", (const char *) glGetString (GL_VENDOR));
	g_error("Required openGL ES extension GL_NV_read_depth not available on this platform.\n");
    }
    
    // Set background color and clear buffers
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK("glClearColor");
  
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Frame Buffer Object stuff
    glGenFramebuffers(1,&frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER,frameBuffer);

    glGenRenderbuffers(1,&depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER,depthBuffer);
       
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,windowWidth,windowHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthBuffer);
    
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
	g_error("FBO FAILED %d\n",status);
	exit(0);
    }

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

    SetUpShaders(shaderPath);
}

extern mat4  mvpMatrix;

// Top level "draw"
void GlesDraw(GLsizei width,GLsizei height)
{
    //CHECK("DUMMY1");
    glViewport (0, 0,width,height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //CHECK("DUMMY2");
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glEnable(GL_CULL_FACE);

    DrawKeyboard();
    DrawHands();
    //CHECK("DUMMY3");

}


// Read depth (z) value for pixel at (x,y) 
GLuint getDepth(GtkAllocation *allocation,GLint x,GLint y)
{
    GLuint depth;
    
    e = glGetError();
    if(e != 0)
    {
	g_debug("glReadPixels Error 1 %x\n",e);
    }

    glBindFramebuffer(GL_FRAMEBUFFER,frameBuffer );
    e = glGetError();
    if(e != 0)
    {
	g_debug("glReadPixels Error 2 %x\n",e);
    }
    glViewport (0, 0,allocation->width , allocation->height);
    e = glGetError();
    if(e != 0)
    {
	g_debug("glReadPixels Error 3  %x\n",e);
    }
    glEnable(GL_DEPTH_TEST);
    e = glGetError();
    if(e != 0)
    {
	g_debug("glReadPixels Error 4  %x\n",e);
    }
    glDepthMask(GL_TRUE);
    e = glGetError();
    if(e != 0)
    {
	g_debug("glReadPixels Error 5  %x\n",e);
    }
    glReadPixels(x,y,1,1,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,&depth);
    e = glGetError();
    if(e != 0)
    {
	g_debug("glReadPixels Error 6  %x\n",e);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return(depth);
}
