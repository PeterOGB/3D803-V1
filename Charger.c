/*  The Battery Charger for the Elliott 803 emulator.

    Copyright © 2020  Peter Onion
  
    See LICENCE file. 

*/
#define G_LOG_USE_STRUCTURED

#include <stdlib.h>
#include <gtk/gtk.h>
#include <math.h>

#include "Charger.h"
#include "Wiring.h"
#include "Common.h"



/*
Constants for current meter damping from PID.c
*/
static double Va,Vb,Vc,Vd,Ve,Vf,Vg;
static double Ia,Ib,Ic,Id,Ie,If;

static double KPvoltageMeter = 0.10;

static double KAcharger = 0.029;
static double KCcharger = 0.83;

static double Rbat = (1.0/88.0);


static double Icharger = 0.0;
static double Icomputer = 0.0;
static double Ipts = 0.0;
static double Ireader = 0.0;
static double Ipunch = 0.0;
static double Ibat;
static double CurrentMeterReading;
static double Vop;
double Vbat = 26.5;
static double VoltageReading;
static gboolean mainsAvailable = FALSE;
static gboolean chargerConnected = FALSE;

static void ChargerConnected(__attribute__((unused)) unsigned int dummy)
{
    g_debug("\n");
    Va = Vb = Vc = Vd = Ve =  Vf = Vg = 0.0;
    chargerConnected  = TRUE;
}

static void ChargerDisconnected(__attribute__((unused)) unsigned int dummy)
{
    g_debug("\n");
    chargerConnected = FALSE;
}

static void CpuLoadOn(__attribute__((unused)) unsigned int dummy)
{
    g_debug("\n");
    Icomputer = 40.0;
}  

static void CpuLoadOff(__attribute__((unused)) unsigned int dummy)
{
    g_debug("\n");
    Icomputer = 0.0;
} 

static void MainsSupplyOn(__attribute__((unused)) unsigned int dummy)
{
    g_debug("\n");
    mainsAvailable = TRUE;
}  

static void MainsSupplyOff(__attribute__((unused)) unsigned int dummy)
{
    g_debug("\n");
    mainsAvailable = FALSE;
} 

// Called 100 times/sec to model the bahaviour of the Voltage and Current  meters */
static void updateCharger(__attribute__((unused)) unsigned int dummy)
{
   
    double error,dt;
    static double Ierror;
    double C,R;
    static gdouble ShowingI=-1.0,ShowingV=-1.0;

    Ipunch = 0.0;

    // Time constants etc
    dt = 0.015;
    C = 1.0E-3;
    R = 52.0 * 2.0;

    if(chargerConnected)
    {
	// Model the slow current rise when the computer is turned on.
	Vbat += Ibat * KAcharger * dt; // Battery charge/discahrge slope

	Vop = Vbat + (Ibat * Rbat);

	Ibat = Icharger - (Icomputer + Ipts + Ireader + Ipunch) ;

	if(mainsAvailable)
	{

	    Ierror = (27.0 - Vop) * 88.0 * KCcharger;
	    // Cap Ierror rather tnat Icharger to give a smoother meter motion
	    if(Ierror > 50.0) Ierror = 50.0;
	    Va = Ierror;

	    Ia = (Va - Vb) / R;
	    Vb += Ia * dt / C;

	    Ib = (Vb - Vc) / R;
	    Vc += Ib * dt / C;

	    Ic = (Vc - Vd) / R;
	    Vd += Ic * dt / C;

	    Id = (Vd - Ve) / R;
	    Ve += Id * dt / C;
	
	    Ie = (Ve - Vf) / R;
	    Vf += Ie * dt / C;

	    If = (Vf - Vg) / R;
	    Vg += If * dt / C;

	    Icharger = Vg;
	}
	else
	{
	    	    Icharger *= 0.95;
	}
    }
    else
    {
	// Model the quick current fall when the computer is turned off.
	Vop *= 0.99;
	Icharger *= 0.95;
	Va = Vb = Vc = Vd = Ve =  Vf = Vg = 0.0;
    }

    CurrentMeterReading = Icharger;
    error = Vop - VoltageReading;
    VoltageReading += error * KPvoltageMeter ;

    // Only update display if something has changed
    if((fabs(VoltageReading-ShowingV)>0.1) || (fabs(CurrentMeterReading-ShowingI)>0.1))
    {
	ShowingV = VoltageReading;
	ShowingI = CurrentMeterReading;
    }
}



// Save the window position in the "ChargerState" config file.
void ChargerTidy(__attribute__((unused)) GString *userPath)
{

}



void ChargerInit(
		 __attribute__((unused)) GString *sharedPath,
		 __attribute__((unused)) GString *userPath)
{
    // Wire up the battery charger
    connectWires(CHARGER_CONNECTED,ChargerConnected);
    connectWires(CHARGER_DISCONNECTED,ChargerDisconnected);
    connectWires(SUPPLIES_ON,CpuLoadOn);
    connectWires(SUPPLIES_OFF,CpuLoadOff);
    connectWires(MAINS_SUPPLY_ON,MainsSupplyOn);
    connectWires(MAINS_SUPPLY_OFF,MainsSupplyOff);

    // Hook updateCharger into the 100Hz timer.
    connectWires(TIMER100HZ,updateCharger);
}



