/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

// Various operations on 803 words


#include <stdio.h>
#include "E803-types.h"
#include "E803ops.h"

/*
  BitsSign is the sign bit in the accumulator 
  BitsSign2 is the duplicated signbit 
  BitsCarry  is the bit in the AR to set/test for carry to ACC
  BitsOver is the bit to test in ACC to detect carry.  It should
  be the same state as the carry bit if no overflow.
*/

// *b = *b + *a 
int E803_add(E803word *a, E803word *b) 
{
    uint64_t res,signBitCopy;
    int ovr;
   
    res = *a + *b;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *b = (res & Bits39) | signBitCopy;
 
    return(ovr);
}

// *b = *b - *a
int E803_sub(E803word *a, E803word *b) 
{
    uint64_t res,signBitCopy;
    int ovr;
   
    res = *b - *a;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *b = (res & Bits39) | signBitCopy;
 
    return(ovr);
}

// 40 bit versions for divide.
int E803_add56(E803word *a, E803word *b) 
{
    uint64_t res;
   
    res = *a + *b;
    *b = (res & Bits40);
 
    return(0);
}

// *b = *b - *a
int E803_sub56(E803word *a, E803word *b) 
{
    uint64_t res;
   
    res = *b - *a;
    *b = (res & Bits40);
 
    return(0);
}

// *b = - *a
int E803_neg(E803word *a, E803word *b) 
{
    uint64_t res,signBitCopy;
    int ovr;
   
    res = - *a;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *b = (res & Bits39) | signBitCopy;
 
    return(ovr);
}

// *b = *b & *a
int E803_and(E803word *a, E803word *b) 
{
    uint64_t res,signBitCopy;
   
    res = *b & *a;

    signBitCopy = (res << 1) & BitsSign2;
    *b = (res & Bits39) | signBitCopy;
 
    return(0);
}

// *b =  *a - *b
int E803_neg_add(E803word *a, E803word *b) 
{
    uint64_t res,signBitCopy;
    int ovr;
   
    res = *a - *b;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *b = (res & Bits39) | signBitCopy;
 
    return(ovr);
}

void E803_signed_shift_right(E803word *acc, E803word *ar )
{
    uint64_t res;
     
    res = *acc;
    if((res & 1) != 0)
    {
	*ar |= BitsCarry;
    }

    if((res & BitsSign) != 0)
    {
	res >>= 1;
	res &= Bits38;
	res |= BitsSign + BitsSign2;
    }
    else
    {
	res >>= 1;
	res &= Bits38;
    }
    
    *acc =  res;
    
    res = *ar; 
    res >>= 1;
    res &= Bits38;
    *ar = res;

    return;
}

void E803_unsigned_shift_right(E803word *a)
{
    *a >>= 1;
    *a &= Bits38;   /* Sign bit and copy set to zero */
}

void E803_Double_M( E803word *a)
{
    *a = (*a << 1) & 0xFFFFFFFFFELL;
}

void E803_Acc_to_Q(E803word *acc, E803word *qacc, E803word *qar )
{
    *qar =  *acc & Bits38;
    *qacc = (*acc & BitsSign) ? Bits40 : 0x0L;
}

int E803_dadd(E803word *qacc, E803word *qar ,
		     E803word *acc, E803word *ar)
{
    uint64_t c,signBitCopy,res;
    int ovr;

    res = *ar + *qar;
    
    c = (res & BitsCarry) >> 38;
    *ar = res & Bits38;

    res = *acc + *qacc + c;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *acc = (res & Bits39) | signBitCopy;

    return(ovr);
}

int E803_dsub(E803word *qacc, E803word *qar ,
		     E803word *acc, E803word *ar)
{
    uint64_t b,signBitCopy,res;
    int ovr;

    res = *ar - *qar;
    
    b = (res & BitsCarry) >> 38;
    *ar = res & Bits38;

    res = *acc - *qacc - b;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *acc = (res & Bits39) | signBitCopy;

    return(ovr);
}

int E803_shift_left(E803word *acc, E803word *ar )
{
    uint64_t res,carry,signBitCopy;
    int ovr;

    res =  *ar << 1;
    carry = (res & BitsCarry) >> 38;
    *ar = res & Bits38; 
    
    res = (*acc << 1) | carry;

    signBitCopy = (res << 1) & BitsSign2;
    ovr = ((res ^ signBitCopy) & BitsOver) != 0;
	
    *acc = (res & Bits39) | signBitCopy;

    return(ovr);
}

int E803_shift_left56(E803word *acc, E803word *ar )
{
    uint64_t res,carry;

    res =  *ar << 1;
    carry = (res & BitsCarry) >> 38;
    *ar = res & Bits38; 
    
    *acc = ((*acc << 1) | carry) & Bits40;

    return(0);
}

/* Return value 
0xXX00   if M == 0  XX = 00 else FF
0x00YY   if M == -1 YY = FF else 00
*/
int E803_Shift_M_Right(E803word *m )
{
    uint64_t res,signBitCopy;
    int ret;

    res = *m >> 1;

    signBitCopy = (res << 1) & BitsSign2;
	
    *m = (res & Bits39) | signBitCopy;
    
    ret = 0;
    
    res &= Bits39;

    if(res != 0) ret |= 0xFF00;
    if(res == Bits39) ret |= 0xFF;

    return(ret);
}

void E803_AR_to_ACC(E803word *acc, E803word *ar )
{
    *acc = *ar & Bits38;
}

int E803_mant_shift_right(E803word *mant,int shiftA ,int shiftB)
{
    uint64_t res,signBitCopy;
    int ret;  

    ret = 0;
    
    res = *mant;

    while(shiftA--)
    {
	ret += (int) (res & 1);
	
	res >>= 1;
	
	signBitCopy = (res << 1) & BitsSign2;
	res = (res & Bits39) | signBitCopy;
    }
	
    *mant = res;

    while(shiftB--)
    {
	ret += (int) (res & 1);
	
	res >>= 1;
	
	// Are these needed ? res is not used 
	signBitCopy = (res << 1) & BitsSign2;
	res = (res & Bits39) | signBitCopy;
    }

    return(ret);
}

/*

803 floating point format numbers

E803word.bytes[]       4	3	 2	 1	  0
                   SMMMMMMM MMMMMMMM MMMMMMMM MMMMMMmE EEEEEEEE
	           
S = replicated Sign bit
M = Mantissa    m = 2^-29  LSB of Mantisa
E = Exponent

Separated Mantissae are held as...
E803word.bytes[]       4	 3	 2	   1	    0
                    SSMMMMMM MMMMMMMM MMMMMMMM MMMMMMMm nn000000

n = extra mantisa bits held during calculations
*/

#define BitsMant (0xFFFFFFFF00LL)

int E803_fp_split( E803word *acc , E803word *mant)
{
    uint64_t res,signBitCopy;
    
    res = *acc;

    signBitCopy = res & BitsSign2;
    res >>= 1;
    // Does this need an extra mask ?

    res = (res | signBitCopy) & BitsMant;

    *mant = res;

    return(*acc & 511);
}

void E803_fp_join( E803word *mant, int *exp, E803word *acc)
{
     uint64_t res;

     res = (*mant << 1) & (BitsMant - 0x100LL);
     res |= *exp & 0x1FFLL;
     *acc = res;
}

int E803_mant_add( E803word *a, E803word *b)
{

    uint64_t res,signBitCopy;
    int ovr;

    res = *a + *b;

    signBitCopy = (res << 1) & BitsSign;
    ovr = ((res ^ signBitCopy) & BitsSign) != 0;
	
    *b = res;

    return(ovr);
}

int E803_mant_sub( E803word *a, E803word *b)
{
    uint64_t res,signBitCopy;
    int ovr;
    
    res = *b - *a;

    signBitCopy = (res << 1) & BitsSign;
    ovr = ((res ^ signBitCopy) & BitsSign) != 0;
	
    *b = res;

    return(ovr);
}

void E803_rotate_left(E803word *acc, int *cnt )
{
    uint64_t res,c,signBitCopy;
    int count;

    count = *cnt;

    res = *acc;

    while(count--)
    {
	c = (res & BitsSign) >> 38;
	res <<= 1;
	res += c;
    }

    signBitCopy = (res << 1) & BitsSign2;
    *acc = (res & Bits39) | signBitCopy;
}

void E803_shift_left_F65( E803word *acc, int *cnt )
{
    uint64_t res,signBitCopy;
    int count;

    count = *cnt;

    res = *acc;

    res <<= count;

    signBitCopy = (res << 1) & BitsSign2;
    *acc = (res & Bits39) | signBitCopy;
}

/* FIxed bug 1/1/06 */
/* Fixed again 6/3/10 ! */

void E803_SCR_to_STORE(int32_t *SCR, E803word *ptr)

{
    uint64_t res,scr;

    scr = (uint64_t) *SCR;

    // Modified 14/10/09 after checking on the real 803 at TNMOC.
    res = (scr >> 1) & 8191;
    // The half bit is present in the upper copy of the SCR.
    res |= ((scr) & 16383) << 23;

    *ptr = res;
}

