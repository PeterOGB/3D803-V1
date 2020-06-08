/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#define _GNU_SOURCE

#include <gtk/gtk.h>
#include <GLES2/gl2.h>
#include <cglm/cglm.h>

#include "ObjLoader.h"
#include "WGbuttons.h"
#include "Common.h"
#include "Keyboard.h"
#include "wg-definitions.h"

#define SOUNDS 1

static gboolean startSndEffect(struct sndEffect *effect);
GSList *runingSndEffects = NULL;

enum {WG_PRESS=1,WG_RELEASE,RR_PRESS,RR_RELEASE};
enum {BUTTON_UP=0,BUTTON_DOWN,BUTTON_RELEASED};

unsigned int rowValues[ROWCOUNT];

static const char *rowNames[ROWCOUNT] = 
{
    "F1","N1","F2","N2","Operate","Read-Normal-Obey","Clear_Store",
    "Manual_Data","Reset","Battery_Off","Battery_On","Computer_Off",
    "Computer_On","Selected_Stop","Power","NoRow"
}; 
int buttonsReleased;

int doFREE_PRESS(int state,int event, void *data,guint time);
int doFREE_RELEASE(int state,int event, void *data,guint time);
int doWG_RELEASE(int state,int event, void *data,guint time);
int doWG_PRESS(int state,int event, void *data,guint time);
int doRADIO_PRESS(int state,int event, void *data,guint time);
int doRADIO_RELEASE(int state,int event, void *data,guint time);
int doRR_PRESS(int state,int event, void *data,guint time);
int doRR_RELEASE(int state,int event, void *data,guint time);

int doSnd_2(int state,int event, void *data,guint time);
int doSnd_3(int state,int event, void *data,guint time);
void doButtonFSM(WGButton *button, int event,guint time);

struct fsmtable WGbuttonFSM[] = {
    { 0, WG_PRESS,      1, doWG_PRESS },
    { 0, WG_RELEASE,    0, NULL},
    
    { 0, RR_PRESS,      3, NULL },

    { 1, WG_RELEASE,    2, doSnd_2 },
    { 1, RR_PRESS,      3, doWG_RELEASE },

    { 2, WG_PRESS,      1, doSnd_3 },
    { 2, WG_RELEASE,    2, NULL},
    { 2, RR_PRESS,      3, doWG_RELEASE },

    { 3, WG_PRESS,      4, doWG_PRESS },
    { 3, RR_RELEASE,    0, NULL },

    { 4, WG_RELEASE,    3, doWG_RELEASE },
    { 4, RR_RELEASE,    1, NULL },

    {-1,-1,             -1,NULL }
};

struct fsmtable FREEbuttonFSM[] = {

    { 0, WG_PRESS,      1, doFREE_PRESS },
    { 1, WG_RELEASE,    0, doFREE_RELEASE },    
    {-1,-1,             -1,NULL }
};

struct fsmtable TOGGLEbuttonFSM[] = {

    { 0, WG_PRESS,      1, doFREE_PRESS },
    
    { 1, WG_RELEASE,    2, doSnd_2 },
    { 2, WG_PRESS,      3, doSnd_3 },
    { 3, WG_RELEASE,    0, doFREE_RELEASE },
    {-1,-1,             -1,NULL }
};

struct fsmtable RADIObuttonFSM[] = {

    { 0, WG_PRESS,      1, doRADIO_PRESS },
    { 1, WG_RELEASE,    0, doRADIO_RELEASE },
    {-1,-1,             -1,NULL }
};

struct fsmtable RRbuttonFSM[] = {

    { 0, WG_PRESS,      1, doRR_PRESS },
    { 0, WG_RELEASE,    0, NULL},
    
    { 1, WG_RELEASE,    0, doRR_RELEASE },    
    {-1,-1,             -1,NULL }
};

void WGButtonPressed(WGButton *bp,gboolean presed,guint time);
void WGOperatePressed(WGButton *bp,gboolean pressed,guint time);

#define BUTTONX(c) (-3.1f+(c*(2.2f)/12.0f))
#define BUTTONY(r) ( 2.36f-(r*(2.36f-2.77f)/3.0f))
#define BUTTONZ(r) ( 2.54f+(r*(0.21f-2.54f)/3.0f))

/*
Objects on the console are ordered so as to minimise the number of times the object changes 
which minimises the number of object data loads in the openGL
*/

WGButton WGButtons[] = {
   
    {1, &BlackButtonObject,{BUTTONX(0),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,4096,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,ONE_FINGER},
    {2, &BlackButtonObject,{BUTTONX(1),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,2048,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,TWO_FINGERS},
    {3, &BlackButtonObject,{BUTTONX(2),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,1024,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {4, &BlackButtonObject,{BUTTONX(3),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,512,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {5, &BlackButtonObject,{BUTTONX(4),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,256,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {6, &BlackButtonObject,{BUTTONX(5),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,128,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {7, &BlackButtonObject,{BUTTONX(6),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,64,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {8, &BlackButtonObject,{BUTTONX(7),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,32,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {9, &BlackButtonObject,{BUTTONX(8),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,16,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {10, &BlackButtonObject,{BUTTONX(9),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,8,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {11, &BlackButtonObject,{BUTTONX(10),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,4,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {12, &BlackButtonObject,{BUTTONX(11),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,2,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {13, &BlackButtonObject,{BUTTONX(12),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,1,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},

    {15, &BlackButtonObject,{BUTTONX(0),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,040,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,ONE_FINGER},
    {16, &BlackButtonObject,{BUTTONX(1),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,020,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,TWO_FINGERS},
    {17, &BlackButtonObject,{BUTTONX(2),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,010,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {18, &BlackButtonObject,{BUTTONX(3),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,004,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {19, &BlackButtonObject,{BUTTONX(4),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,002,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {20, &BlackButtonObject,{BUTTONX(5),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,001,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},

    {30, &BlackButtonObject,{BUTTONX(0),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,8192,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,ONE_FINGER},
    {31, &BlackButtonObject,{BUTTONX(1),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,4096,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,TWO_FINGERS},
    {32, &BlackButtonObject,{BUTTONX(2),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,2048,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {33, &BlackButtonObject,{BUTTONX(3),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,1024,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {34, &BlackButtonObject,{BUTTONX(4),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,512,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {35, &BlackButtonObject,{BUTTONX(5),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,256,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {36, &BlackButtonObject,{BUTTONX(6),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,128,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {37, &BlackButtonObject,{BUTTONX(7),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,64,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {38, &BlackButtonObject,{BUTTONX(8),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,32,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {39, &BlackButtonObject,{BUTTONX(9),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,16,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {40, &BlackButtonObject,{BUTTONX(10),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,8,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {41, &BlackButtonObject,{BUTTONX(11),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,4,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {42, &BlackButtonObject,{BUTTONX(12),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,2,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},

    {45, &BlackButtonObject,{BUTTONX(0),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,040,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,ONE_FINGER},
    {46, &BlackButtonObject,{BUTTONX(1),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,020,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,TWO_FINGERS},
    {47, &BlackButtonObject,{BUTTONX(2),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,010,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {48, &BlackButtonObject,{BUTTONX(3),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,004,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {49, &BlackButtonObject,{BUTTONX(4),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,002,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {50, &BlackButtonObject,{BUTTONX(5),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,001,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},

    {60, &BlackButtonObject,{BUTTONX(30),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},RON,0,WG_read,WGButtonPressed,0,0,{NULL},0,RADIObuttonFSM,ONE_FINGER},
    {61, &BlackButtonObject,{BUTTONX(32),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},RON,0,WG_normal,WGButtonPressed,0,0,{NULL},0,RADIObuttonFSM,ONE_FINGER},
    {62, &BlackButtonObject,{BUTTONX(34),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},RON,0,WG_obey,WGButtonPressed,0,0,{NULL},0,RADIObuttonFSM,ONE_FINGER},

    {63, &BlackButtonObject,{BUTTONX(32),BUTTONY(0.65f),BUTTONZ(0.65f),0.0f},{0.0f,0.0f,0.0f,0.0f},MANUALDATA,0,1,WGButtonPressed,0,0,{NULL},0,TOGGLEbuttonFSM,ONE_FINGER},
    
    {64, &BlackButtonObject,{BUTTONX(22.7f),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},SELECTEDSTOP,0,1,WGButtonPressed,0,0,{NULL},0,TOGGLEbuttonFSM,ONE_FINGER},

    {101,&BlackButtonObject,{BUTTONX(30),BUTTONY(0.65f),BUTTONZ(0.65f),0.0f},{0.0f,0.0f,0.0f,0.0f} ,  CLEARSTORE,0,1,WGButtonPressed,0,0,{NULL},0,TOGGLEbuttonFSM,ONE_FINGER},
    {102,&BlackButtonObject,{BUTTONX(34),BUTTONY(0.65f),BUTTONZ(0.65f),0.0f},{0.0f,0.0f,0.0f,0.0f},       RESET,0,1,WGButtonPressed,0,0,{NULL},0,FREEbuttonFSM,ONE_FINGER},
    
    {43, &RedButtonObject,{BUTTONX(13),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,1,WGButtonPressed,0,0,{NULL},0,WGbuttonFSM,THREE_FINGERS},
    {14, &RedButtonObject,{BUTTONX(-2),BUTTONY(0),BUTTONZ(0),0.0f},{0.0f,0.0f,0.0f,0.0f},N2,0,0,WGButtonPressed,0,0,{NULL},0,RRbuttonFSM,ONE_FINGER},       
    {21, &RedButtonObject,{BUTTONX(-2),BUTTONY(1),BUTTONZ(1),0.0f},{0.0f,0.0f,0.0f,0.0f},F2,0,0,WGButtonPressed,0,0,{NULL},0,RRbuttonFSM,ONE_FINGER},
    {44, &RedButtonObject,{BUTTONX(-2),BUTTONY(2),BUTTONZ(2),0.0f},{0.0f,0.0f,0.0f,0.0f},N1,0,0,WGButtonPressed,0,0,{NULL},0,RRbuttonFSM,ONE_FINGER},
    {51, &RedButtonObject,{BUTTONX(-2),BUTTONY(3),BUTTONZ(3),0.0f},{0.0f,0.0f,0.0f,0.0f},F1,0,0,WGButtonPressed,0,0,{NULL},0,RRbuttonFSM,ONE_FINGER},
    
    {70,  &OperateBarObject,{BUTTONX(16.0f),BUTTONY(-4.9f),BUTTONZ(-1.15f),0.0f},{0.0f,0.0f,0.0f,0.0f},     OPERATE,0,1,WGOperatePressed,0,0,{NULL},0,FREEbuttonFSM,THREE_FINGERS},

    {80,  &VolumeObject,{2.720643f,2.603532f,1.193400f},{0.0f,0.0f,0.0f,0.0f},NOROW,0,0,NULL,0,0,{NULL},0,FREEbuttonFSM,ONE_FINGER},       

    {999,&ShroudObject,{BUTTONX(30),BUTTONY(0.65f),BUTTONZ(0.65f),0.0f},{0.0f,0.0f,0.0f,0.0f} ,       NOROW,0,0,           NULL,0,0,{NULL},0,NULL,0},
    {999,&ShroudObject,{BUTTONX(34),BUTTONY(0.65f),BUTTONZ(0.65f),0.0f},{0.0f,0.0f,0.0f,0.0f},       NOROW,0,0,           NULL,0,0,{NULL},0,NULL,0},


    {999,&PowerShroudObject,{BUTTONX(21.9f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},       NOROW,0,0,           NULL,0,0,{NULL},0,NULL,0},
    {999,&PowerShroudObject,{BUTTONX(24.6f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},       NOROW,0,0,           NULL,0,0,{NULL},0,NULL,0},
    {999,&PowerShroudObject,{BUTTONX(30.5f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},       NOROW,0,0,           NULL,0,0,{NULL},0,NULL,0},
    {999,&PowerShroudObject,{BUTTONX(33.2f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},       NOROW,0,0,           NULL,0,0,{NULL},0,NULL,0},
      
    {103,&PowerOnButtonObject,{BUTTONX(21.9f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},       BATON,0,1,     WGButtonPressed,0,0,{NULL},0,FREEbuttonFSM,ONE_FINGER},
    {104,&PowerOffButtonObject,{BUTTONX(24.6f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},      BATOFF,0,1,     WGButtonPressed,0,0,{NULL},0,FREEbuttonFSM,ONE_FINGER},

    {105,&PowerOnButtonObject,{BUTTONX(30.5f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},       CPUON,0,1,     WGButtonPressed,0,0,{NULL},0,FREEbuttonFSM,ONE_FINGER},
    {106,&PowerOffButtonObject,{BUTTONX(33.2f),BUTTONY(2.85f),BUTTONZ(2.85f),0.0f},{0.0f,0.0f,0.0f,0.0f},      CPUOFF,0,1,     WGButtonPressed,0,0,{NULL},0,FREEbuttonFSM,ONE_FINGER},
  
    {0,NULL,{0.0f,0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f,0.0f},0,0,0,NULL,0,0,{NULL},0,NULL,0}
};



int doSnd_2(__attribute__((unused))int state,
	    __attribute__((unused))int event,
	    __attribute__((unused)) void *data,
	    __attribute__((unused))guint time)
{
#if SOUNDS
    WGButton *button;
    button = (WGButton *) data;
    
    startSndEffect(button->sndEffects[2]);
#endif    
    return -1;
}

int doSnd_3(__attribute__((unused))int state,
	    __attribute__((unused))int event,
	    __attribute__((unused))void *data,
	    __attribute__((unused))guint time)
{
#if SOUNDS
    WGButton *button;
    button = (WGButton *) data;
    
    startSndEffect(button->sndEffects[3]);
#endif    
    return -1;
}

void doButtonFSM(WGButton *button, int event,guint time)
{
    int (*handler)(int,int,void *,guint);
    int state;
    struct fsmtable *fsmEntry;
 
    fsmEntry = button->buttonFSM;
    state = button->fsmState;
    
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
		state = (handler)(state,event,(void *) button,time);
		if(state == -1)
		    state = fsmEntry->nextState; 
	    }
	    button->fsmState = state;
	    return;
	}
	fsmEntry++;
    }
   
    g_warning("No entry in buttonFSM table for state %d event %d\n",state,event);
}

int doFREE_PRESS(__attribute__((unused))int state,
		 __attribute__((unused))int event,
		 void *data,guint time)
{
    WGButton *buttonp;
    ButtonEvent *be;
    
    buttonp = (WGButton *) data;
    buttonp->state = BUTTON_DOWN;
    rowValues[buttonp->rowId] |= buttonp->value;

    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
    be->press = TRUE;
    be->rowId = buttonp->rowId;
    be->value = buttonp->value;
    be->time  = time;

    g_async_queue_push(ButtonEventQueue,(gpointer) be);
    
#if SOUNDS    
    startSndEffect(buttonp->sndEffects[4]);
#endif
    
    return -1;
}

int doFREE_RELEASE(__attribute__((unused))int state,
		   __attribute__((unused))int event,
		   void *data,guint time)
{
    WGButton *buttonp;
    ButtonEvent *be;
    
    buttonp = (WGButton *) data;
    buttonp->state = BUTTON_UP;
    rowValues[buttonp->rowId] &= ~buttonp->value;

    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
    be->press = FALSE;
    be->rowId = buttonp->rowId;
    be->value = buttonp->value;
    be->time  = time;
	
    g_async_queue_push (ButtonEventQueue,(gpointer) be);
    
#if SOUNDS    
    startSndEffect(buttonp->sndEffects[5]);
#endif
    
    return -1;
}

void WGOperatePressed(WGButton *bp,gboolean pressed,guint time)
{
    if(pressed)
    {
	doButtonFSM(bp,WG_PRESS,time);
    }
    else
    {
	doButtonFSM(bp,WG_RELEASE,time);
    }
}

int doWG_PRESS(__attribute__((unused))int state,
	       __attribute__((unused))int event,
	       __attribute__((unused))void *data,
	       guint time)
{
    WGButton *button;
    ButtonEvent *be;

    button = (WGButton *) data;

    button->state = BUTTON_DOWN;
    rowValues[button->rowId] |= button->value;

    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
    be->press = TRUE;
    be->rowId = button->rowId;
    be->value = button->value;
    be->time  = time;

    g_async_queue_push  (ButtonEventQueue,(gpointer) be);
    
#if SOUNDS    
    if(state == 0)
	startSndEffect(button->sndEffects[1]);
    if(state == 3)
	startSndEffect(button->sndEffects[4]);
#endif    
    return -1;
}

int doWG_RELEASE(__attribute__((unused))int state,
		 __attribute__((unused))int event,
		 __attribute__((unused))void *data,
		 __attribute__((unused))guint time)
{
    WGButton *button;
    ButtonEvent *be;
    
    button = (WGButton *) data;

    buttonsReleased += 1;

    button->state = BUTTON_UP;
    rowValues[button->rowId] &= ~button->value;

    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
    be->press = FALSE;
    be->rowId = button->rowId;
    be->value = button->value;
    be->time  = time;

    g_async_queue_push  (ButtonEventQueue,(gpointer) be);

#if SOUNDS    
    if(state == 4)
	startSndEffect(button->sndEffects[5]);
#endif    
    return -1;
}


void WGButtonPressed(WGButton *bp,gboolean pressed,guint time)
{
    if(pressed)
    {
	doButtonFSM(bp,WG_PRESS,time);
    }
    else
    {
	doButtonFSM(bp,WG_RELEASE,time);
    }
}

int doRADIO_PRESS(__attribute__((unused))int state,
		  __attribute__((unused))int event,
		  void *data, guint time)
{
    int rowId;
    WGButton *buttonp,*RRbuttonp;
    ButtonEvent *be;

    RRbuttonp = (WGButton *) data;
    if(RRbuttonp->state == BUTTON_DOWN) return -1;
    RRbuttonp->state = BUTTON_DOWN;

    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
    be->press = TRUE;
    be->rowId = RRbuttonp->rowId;
    be->value = RRbuttonp->value;
    be->time  = time;

    g_async_queue_push (ButtonEventQueue,(gpointer) be);
    
    rowId = RRbuttonp->rowId;
    rowValues[rowId] |= RRbuttonp->value;

    buttonp = WGButtons;
    while(buttonp->objectId != 0)
    {	
	if((buttonp->rowId == rowId) && (buttonp != RRbuttonp))
	{
	    buttonp->state = BUTTON_UP;
	    rowValues[buttonp->rowId] &= ~buttonp->value;
		
	    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
	    be->press = FALSE;
	    be->rowId = buttonp->rowId;
	    be->value = buttonp->value;
	    be->time  = time;

	    g_async_queue_push (ButtonEventQueue,(gpointer) be);
	}
	buttonp++;
    }
#if SOUNDS
    startSndEffect(RRbuttonp->sndEffects[4]);
#endif
    return -1;
}

int doRADIO_RELEASE(__attribute__((unused)) int state,
		    __attribute__((unused)) int event,
		    __attribute__((unused)) void *data,
		    __attribute__((unused)) guint time)
{
#if SOUNDS
    WGButton *RRbuttonp;
    RRbuttonp = (WGButton *) data;

    startSndEffect(RRbuttonp->sndEffects[5]);
#endif
    return -1;
}

int doRR_PRESS(__attribute__((unused))int state,
	       __attribute__((unused))int event,
	       void *data,
	       guint time)
{
    int rowId;
    unsigned int buttonValue;
    WGButton *buttonp,*RRbutton;
 
    RRbutton = (WGButton *) data; 
    RRbutton->state = BUTTON_DOWN;
    rowId = RRbutton->rowId;

    buttonsReleased = 0;
    
    buttonp = WGButtons;
    while(buttonp->objectId != 0)
    {	
	if((buttonp->rowId == rowId) && (buttonp != RRbutton))
	{
	    doButtonFSM(buttonp,RR_PRESS,time);
	}
	buttonp++;
    }
    
    if(buttonsReleased)
    {
	buttonValue = 1U << (buttonsReleased - 1);
	buttonp = WGButtons;
	while(buttonp->objectId != 0)
	{	
	    if((buttonp->rowId == rowId) && (buttonp->value == buttonValue))
	    {
#if SOUNDS	    	
		startSndEffect(buttonp->sndEffects[7]);
#endif		 
		break;
	    }
	    buttonp++;
	}
    }
    else
    {
#if SOUNDS    	
	startSndEffect(RRbutton->sndEffects[4]);
#endif
    }
    return -1;
}

int doRR_RELEASE(__attribute__((unused))int state,
		 __attribute__((unused))int event,
		 void *data,guint time)
{
    int rowId;
    WGButton *buttonp,*RRbutton;

    RRbutton = (WGButton *) data;
    RRbutton->state = BUTTON_UP;
    rowId = RRbutton->rowId;

    buttonp = WGButtons;
    while(buttonp->objectId != 0)
    {
	if((buttonp->rowId == rowId) && (buttonp != RRbutton))
	{
	    doButtonFSM(buttonp,RR_RELEASE,time);
	}	
	buttonp++;
    }
#if SOUNDS
    startSndEffect(RRbutton->sndEffects[5]);
#endif
    return -1;
}


void OperateBarPressed(gboolean pressed,guint time)
{
    static WGButton *OperateBar = NULL;
    if(OperateBar == NULL)
    {
	for(OperateBar = WGButtons; OperateBar->objectId != 0; OperateBar++)
	{
	    if(OperateBar->objectId == 70)
		break;
	}
	if(OperateBar->objectId == 0)
	    return;
    }

    if(pressed)
    {
	doButtonFSM(OperateBar,WG_PRESS,time);
    }
    else
    {
	doButtonFSM(OperateBar,WG_RELEASE,time);
    }
}


extern GMutex SoundEffectsQueueMutex;
GString *SoundEffectsDirectory = NULL;

static void loadSndEffects(WGButton *button)
{
    int n;
    GString *fileName = NULL;

    fileName = g_string_new("");
   	 
    for(n=1; n <= 7; n++)
    {
	g_string_printf(fileName,"%s%s.%d.%d.wav",
			SoundEffectsDirectory->str,
			rowNames[button->rowId],button->value,n);

	button->sndEffects[n] = readWavData(fileName->str);
    }
    g_string_free(fileName,TRUE);
}

static gboolean startSndEffect(struct sndEffect *effect)
{
    struct sndEffect *playing;
    
    if(effect == NULL) return FALSE;

    playing = (struct sndEffect *) malloc(sizeof(struct sndEffect));

    *playing = *effect;

    g_mutex_lock(&SoundEffectsQueueMutex);

    runingSndEffects = g_slist_prepend(runingSndEffects,playing);

    g_mutex_unlock(&SoundEffectsQueueMutex);
    
    return TRUE;
}

void ButtonsInit(GString *sharedPath,__attribute__((unused)) GString *userPath)
{
    WGButton *currentButton;

    SoundEffectsDirectory = g_string_new(sharedPath->str);
    g_string_append(SoundEffectsDirectory,"sounds/");
    
    for(currentButton = WGButtons; currentButton->objectId != 0; currentButton++)
    {

	if(currentButton->objectId != 70)
	{
	
	    currentButton->TranslateDown[0] = currentButton->TranslateUp[0];
	    currentButton->TranslateDown[1] = currentButton->TranslateUp[1] -
		0.1f * cosf(2.0f*M_PIf32*10.4f/360.0f);
	    currentButton->TranslateDown[2] = currentButton->TranslateUp[2] -
		0.1f * sinf(2.0f*M_PIf32*10.4f/360.0f);
	    currentButton->TranslateDown[3] = currentButton->TranslateUp[3];
	}
	else
	{
	    currentButton->TranslateDown[0] = currentButton->TranslateUp[0];
	    currentButton->TranslateDown[1] = currentButton->TranslateUp[1];
	    currentButton->TranslateDown[2] = currentButton->TranslateUp[2];
	    currentButton->TranslateDown[3] = currentButton->TranslateUp[3];
	}
	
	loadSndEffects(currentButton);
    }
    g_string_free(SoundEffectsDirectory,TRUE);
}
