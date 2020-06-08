/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "Wiring.h"

const char *WiringEventDescriptions[] = {
    "No Event",
    "Mains turned on at the contactor",
    "Mains turned off at the contactor",
    "Mains on to the Charger",
    "Mains off to the Charger",
    "Computer On pressed",
    "Computer Off pressed",
    "Battery On pressed",
    "Battery Off pressed",
    "Power supplies On",
    "Power supplies Off",
    "Wordgen F1 buttons",
    "Wordgen N1 buttons",
    "Wordgen F2 buttons",
    "Wordgen N2 buttons",
    "Wordgen Read Obey Normal buttons",
    "Wordgen Manual Data",
    "Wordgen Reset",
    "Wordgen Clear Store",
    "Wordgen Single Step",
    "Wordgen Operate Bar",
    "100Hz timer signal",
    NULL
};

static GSList *Interconnections[LAST_WIRING_EVENT] = { NULL };

void connectWires(enum WiringEvent event,Connectors handler)
{
    if((event >= 1) && (event < LAST_WIRING_EVENT))
    {
	Interconnections[event] = g_slist_append(Interconnections[event],(gpointer)handler);
    }
    else
    {
	g_error("%s event %d out of range (1..%d)\n",__FUNCTION__,event,LAST_WIRING_EVENT);
    }
}

// Call registered handlers when a wire state is set.
void wiring(enum WiringEvent event,unsigned int values)
{

    GSList *handler;
    Connectors connectors;
    if((event >= 1) && (event < LAST_WIRING_EVENT))
    {
	handler = Interconnections[event];
	while(handler != NULL)
	{
	    connectors = (Connectors ) handler->data;
	    (connectors)(values);
	    handler = g_slist_next(handler);
	}
    }
}
