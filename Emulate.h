/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#pragma once
/* variables in the emulator that are used by network event handlers */

#define UPDATE_RATE 5
//float channel1charTime,channel2charTime,channel3charTime;

void Emulate(int wordTimesToEmulate);
void PreEmulate(bool updateFlag);
void PostEmulate(bool updateFlag);
void StartEmulate(char *coreFileName);

void ReadFileToBuffer(char *filename);
int getComputer_on_state(void);
int getBattery_on_state(void);
void setPTSpower(int n);
int getPTSpower(void);

int texttoword(const char *text,int address);

void setCPUVolume(unsigned int level);
