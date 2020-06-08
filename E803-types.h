/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#pragma once
/* types for 803 39 bit words */ 
#include <stdint.h>

// Removed union with a byte array
// Store 803 39bit word in a 64 bit value.

typedef uint64_t  E803word;
