/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
#pragma once

#ifndef __GTK_H__
#include <gtk/gtk.h>
#endif

gboolean GtkInit(GString *sharedPath,int *argc,char ***argv);
void UpdateScreen(void);
gboolean GtkRun(int w,int h);
void showCursor(void);
void hideCursor(void);
