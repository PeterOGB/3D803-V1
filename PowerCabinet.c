/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <gtk/gtk.h>

#include "Common.h"
#include "Wiring.h"
#include "PowerCabinet.h"

static gboolean SuppliesOn = FALSE;
struct fsm PowerFSM;

// Power switching event handlers
static int doBatOnPress(__attribute__((unused)) int state,
			__attribute__((unused)) int event,
			__attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    PowerFSM.nextEvent = CHARGER_CONNECTED;
    return -1;
}

static int doBatOffPress(__attribute__((unused)) int state,
			 __attribute__((unused)) int event,
			 __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    PowerFSM.nextEvent = CHARGER_DISCONNECTED;
    return -1;
}

static int doCompOnPress(__attribute__((unused)) int state,
			 __attribute__((unused)) int event,
			 __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    PowerFSM.nextEvent = SUPPLIES_ON;
    return -1;
}

static int doCompOffPress(__attribute__((unused)) int state,
			  __attribute__((unused)) int event,
			  __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    PowerFSM.nextEvent = SUPPLIES_OFF;
    return -1;
}

static int doSuppliesOn(__attribute__((unused)) int state,
			__attribute__((unused)) int event,
			__attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    g_debug("\n");
    
    wiring(SUPPLIES_ON,1);
    SuppliesOn = TRUE;
    return -1;
}

static int doSuppliesOff(int state,
			 __attribute__((unused)) int event,
			 __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    wiring(SUPPLIES_OFF,0);
    SuppliesOn = FALSE;
    if((state == 11) || (state == 13))
	PowerFSM.nextEvent = CHARGER_DISCONNECTED;
    return -1;
}

static int doChrgrConnect(__attribute__((unused)) int state,
			  __attribute__((unused)) int event,
			  __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    wiring(CHARGER_CONNECTED,1);
    return -1;
}

static int doChrgrDisconnect(int state,
			     __attribute__((unused)) int event,
			     __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    wiring(CHARGER_DISCONNECTED,0);
    if(state == 8)
	PowerFSM.nextEvent = SUPPLIES_OFF;
    return -1;
}

static int doMainsOff(__attribute__((unused)) int state,
		      __attribute__((unused)) int event,
		      __attribute__((unused)) void *data,
			__attribute__((unused)) guint time)
{
    PowerFSM.nextEvent = CHARGER_DISCONNECTED  ;
    return -1;
}


// Power switching sequencer FSM

struct fsmtable PowerTable[] = {
    { 0,    MAINS_SUPPLY_ON,           1, NULL },      // Should start fan noise

    { 1,    MAINS_SUPPLY_OFF,          0, NULL },      // Should stop fan noise
    { 1,    BATTERY_ON_PRESSED,        2, doBatOnPress },

    { 2,    CHARGER_CONNECTED,         3, doChrgrConnect },      // Set charger op to Vbat

    { 3,    COMPUTER_ON_PRESSED,       4, doCompOnPress },
    { 3,    BATTERY_OFF_PRESSED,       7, doBatOffPress },
    { 3,    MAINS_SUPPLY_OFF,         10, doMainsOff },
    
    { 4,    SUPPLIES_ON,               5, doSuppliesOn },

    { 5,    COMPUTER_OFF_PRESSED,      6, doCompOffPress },
    { 5,    BATTERY_OFF_PRESSED,       8, doBatOffPress },
    { 5,    MAINS_SUPPLY_OFF,         11, NULL },
    
    { 6,    SUPPLIES_OFF,              3, doSuppliesOff },

    { 7,    CHARGER_DISCONNECTED,      1, doChrgrDisconnect },

    { 8,    CHARGER_DISCONNECTED,      9, doChrgrDisconnect },

    { 9,    SUPPLIES_OFF,              1, doSuppliesOff },

    { 10,   CHARGER_DISCONNECTED,      0, doChrgrDisconnect },

    { 11,   MAINS_SUPPLY_ON,           5, NULL },
    { 11,   SUPPLIES_OFF,             12, doSuppliesOff },
    { 11,   COMPUTER_OFF_PRESSED,     13, doCompOffPress },
    
    { 12,   CHARGER_DISCONNECTED,      0, doChrgrDisconnect },

    { 13,   SUPPLIES_OFF,             12, doSuppliesOff },
    {-1,    -1,                        -1, NULL }
};
    

struct fsm PowerFSM = { "Power FSM", 0, PowerTable,NULL,NULL,0,-1};

// Translate wire state changes into FSM events.

static void PowerCabinetMainsOn(__attribute__((unused)) unsigned int dummy)
{
    doFSM(&PowerFSM,MAINS_SUPPLY_ON,NULL);
}

static void PowerCabinetMainsOff(__attribute__((unused)) unsigned int dummy)
{
    doFSM(&PowerFSM,MAINS_SUPPLY_OFF,NULL);
}

static void PowerCabinetComputerOn(unsigned int value)
{
    if(value != 0)
	doFSM(&PowerFSM,COMPUTER_ON_PRESSED,NULL);
}

static void PowerCabinetComputerOff(unsigned int value)
{
    if(value != 0)
	doFSM(&PowerFSM,COMPUTER_OFF_PRESSED,NULL);
}

static void PowerCabinetBatteryOn(unsigned int value)
{
    if(value != 0)
	doFSM(&PowerFSM,BATTERY_ON_PRESSED,NULL);
}

static void PowerCabinetBatteryOff(unsigned int value)
{
    if(value != 0)
	doFSM(&PowerFSM,BATTERY_OFF_PRESSED,NULL);
}
double Vbat;

static void checkVbat(__attribute__((unused)) unsigned int value)
{
    if(SuppliesOn && (Vbat < 20.0))
    {
	doFSM(&PowerFSM,SUPPLIES_OFF,NULL);
    }
}


// Wire up the power cabinet !

void PowerCabinetInit(__attribute__((unused)) GString *sharedPath,
		      __attribute__((unused)) GString *userPath)
{
    connectWires(MAINS_SUPPLY_ON,PowerCabinetMainsOn);
    connectWires(MAINS_SUPPLY_OFF,PowerCabinetMainsOff);

    connectWires(COMPUTER_ON_PRESSED,PowerCabinetComputerOn);
    connectWires(COMPUTER_OFF_PRESSED,PowerCabinetComputerOff);

    connectWires(BATTERY_ON_PRESSED,PowerCabinetBatteryOn);
    connectWires(BATTERY_OFF_PRESSED,PowerCabinetBatteryOff);
		
    connectWires(TIMER100HZ,checkVbat);
}

void PowerCabinetTidy(__attribute__((unused)) GString *userPath)
{
}
