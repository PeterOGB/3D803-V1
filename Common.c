/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include "Common.h"
#include "Sound.h"


struct sndEffect *readWavData(const char *wavFileName)
{
    int fd;
    ssize_t n;
    char header[40];
    size_t length;
    guint32 len;
    char *buffer = NULL;
    struct sndEffect *se;
   
    fd = open(wavFileName,O_RDONLY);

    if(fd == -1) 
    {
	// Not all sound effect files exist, so just ignore missing ones.
	//g_debug("Failed to open file (%s)\n",wavFileName);
        return NULL;
    }

    n = read(fd,header,40);
    
    length = 0;
    n = read(fd,&len,4);
    length = (size_t) len;
    
    buffer = (char *) malloc(length);

    n = read(fd,buffer,length);

    if ((n<0) || ((size_t)n != length))
    {
        g_error("read failed for bulk of %s\n",wavFileName);
        free(buffer);
        buffer = NULL;
    }

    close(fd);

    se = (struct sndEffect *) malloc(sizeof(struct sndEffect));
    se->frames = (gint16 *) buffer;
    se->frameCount = (int) (length/2);
    return(se);
}



/*********************** FSM support *********************/

void doFSM(struct fsm *fsm,int event,void *data)
{

    int (*handler)(int,int,void *,guint);
    int state;
    struct fsmtable *fsmEntry;
 
nextEvent:
    fsmEntry = fsm->table;
    state = fsm->state;

    if(fsm->debugFlag)
    {
	g_debug("\tFSM %s ",fsm->name);
	if(fsm->stateNames != NULL) 
	{
	    g_debug("state %s ",(fsm->stateNames)[state]);
	}
	else
	{
	    g_debug("state %d ",state);
	}

	if(fsm->eventNames != NULL)
	{
	    if(event < 0)
		g_debug("event = FSMNoEvent ");
	    else
		g_debug("event %s \n",(fsm->eventNames)[event]);
	}
	else
	{
	    g_debug("event %d \n",event);
	}
    }

    while(fsmEntry->state != -1)
    {
	if( (fsmEntry->state == state) && (fsmEntry->event == event))
	{
    	    handler = fsmEntry->handler;

	    if(handler == NULL)
	    {
		state = fsmEntry->nextState; 
	    }
	    else
	    {
		state = (handler)(state,event,data,0);
		if(state == -1)
		    state = fsmEntry->nextState; 

	    }
	    fsm->state = state;
	    if(fsm->nextEvent != -1)
	    {
		event = fsm->nextEvent;
		fsm->nextEvent = -1;
		goto nextEvent;
	    }
	    return;
	}
	fsmEntry++;
    }

    if(fsm->debugFlag)
    {
    
	if(fsm->stateNames == NULL)
	{
	    g_debug("No entry in FSM table %s for state %d ",fsm->name,state);
	}
	else
	{
	    g_debug("No entry in FSM table %s for state %s",fsm->name,(fsm->stateNames)[state]);

	}
   
	if(fsm->eventNames == NULL)
	{
	    g_debug("event = %d",event);
	}
	else
	{
	    if(event < 0)
		g_debug("event = FSMNoEvent ");
	    else
		g_debug("event = %s",(fsm->eventNames)[event]);
	}
    }
}

