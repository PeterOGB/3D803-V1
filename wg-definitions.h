/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

/* Sybolic definitions for bits in WG_buttons */
                                                                                                                             
#define WG_operate              (1U<<0)                      
#define WG_read                 (1U<<1)
#define WG_normal               (1U<<2)
#define WG_obey                 (1U<<3)
#define WG_selected_stop        (1U<<4)
#define WG_reset                (1U<<5)
#define WG_manual_data          (1U<<6)
#define WG_clear_store          (1U<<7)
#define WG_803_on               (1U<<8)
#define WG_803_off              (1U<<9)
#define WG_battery_on           (1U<<10)
#define WG_battery_off          (1U<<11)
 
 
#define OBEY_WG_switch          (1U<<12)

extern void WG_Lamp(int n);

// Wordgen row/button ids used in button events
enum rows {F1=0,N1,F2,N2,OPERATE,RON,CLEARSTORE,MANUALDATA,RESET,BATOFF,BATON,CPUOFF,CPUON,
	   SELECTEDSTOP,POWER,VOLUME,NOROW,ROWCOUNT};
