/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
#pragma once

enum FINGERS {ONE_FINGER=1,TWO_FINGERS,THREE_FINGERS};

typedef struct _button 
{
    int objectId;
    OBJECT  **object;
    
    vec4 TranslateUp;
    vec4 TranslateDown;
    int rowId;
    struct _button *resetButton;
    unsigned int value;
    void (*handler)(struct _button *bp,gboolean pressed,guint time);
    int state;
    int moreState;
    struct sndEffect *sndEffects[10];
    int fsmState;
    struct fsmtable *buttonFSM;
    enum FINGERS finger;
} WGButton;

extern WGButton WGButtons[];

void ButtonsInit(GString *sharedPath,GString *userPath);
void OperateBarPressed(gboolean pressed,guint time);


