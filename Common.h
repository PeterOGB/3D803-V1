/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/   

struct fsmtable {
    int state;
    int event;
    int nextState;
    int (*handler)(int state,int event,void *data,guint time);
};

struct fsm {
    const char *name;
    int state;
    struct fsmtable *table;
    const char **stateNames;
    const char **eventNames;
    int debugFlag;
    int nextEvent;
};

struct sndEffect 
{
    int effectType;
    gint16 *frames;
    int frameCount;
};

struct sndEffect *readWavData(const char *wavFileName);
void doFSM(struct fsm *fsm,int event,void *data);
gboolean Running;

typedef struct _buttonEvent
{
    gboolean press;
    int rowId;
    guint value;
    guint time;
} ButtonEvent;

GAsyncQueue *ButtonEventQueue;

typedef struct _lampsEvent
{
    unsigned int lampId;
    gboolean on;
    gfloat brightness;
    
} LampsEvent;

GAsyncQueue *LampsEventQueue;
GAsyncQueue *PlotterMovesQueue;
gboolean oldHandSwap;



