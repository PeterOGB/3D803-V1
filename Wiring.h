/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

enum WiringEvent {MAINS_SUPPLY_ON=1,MAINS_SUPPLY_OFF,CHARGER_CONNECTED,CHARGER_DISCONNECTED,
		  BATTERY_ON_PRESSED,BATTERY_OFF_PRESSED,COMPUTER_ON_PRESSED,COMPUTER_OFF_PRESSED,
		  SUPPLIES_ON,SUPPLIES_OFF,PTS24VOLTSON,
		  F1WIRES,N1WIRES,F2WIRES,N2WIRES,
		  RONWIRES,MDWIRE,RESETWIRE,CSWIRE,SSWIRE,OPERATEWIRE,
		  TIMER100HZ,UPDATE_DISPLAYS,VOLUME_CONTROL,
		  F71,F72,F74,F75,F76,F77,READY,ACT,TRLINES,CLINES,LAST_WIRING_EVENT};

// UPDATE_DISPLAYS is used by PostEmulate to tell WG to update the DM160s

typedef void (*Connectors)(unsigned int);

// Set a wire to a value
void wiring(enum WiringEvent event,unsigned int values);

// Register a connection (handler) to a type of wire. 
void connectWires(enum WiringEvent event,Connectors handler);


