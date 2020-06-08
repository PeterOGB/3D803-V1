/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/


#pragma once
void CpuInit( __attribute__((unused)) GString *sharedPath,
	      __attribute__((unused)) GString *userPath,
	      gchar *coreFileName);


void CPU_sound(__attribute__((unused)) void *buffer, 
	       __attribute__((unused))int sampleCount,
	       __attribute__((unused))double bufferTime,
	       int wordTimes);

void CpuTidy(GString *userPath,gchar *coreFileName);
