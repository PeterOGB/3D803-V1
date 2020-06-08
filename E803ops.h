/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#pragma once
/* Declarations to functions (that used to be in assembler code!)  */
int E803_add(E803word *, E803word *);
int E803_sub(E803word *, E803word *);              
int E803_neg(E803word *, E803word *);
int E803_and(E803word *, E803word *);
int E803_neg_add(E803word *, E803word *);
void E803_signed_shift_right(E803word *, E803word * );
void E803_unsigned_shift_right(E803word *);
void E803_Double_M( E803word *);
void E803_Acc_to_Q(E803word *, E803word *, E803word * );
int E803_dadd(E803word *, E803word * ,E803word *, E803word * );
int E803_dsub(E803word *, E803word *, E803word *, E803word * );
int E803_shift_left(E803word *, E803word * );
int E803_Shift_M_Right(E803word * );
void E803_AR_to_ACC(E803word *, E803word * );
int E803_mant_shift_right(E803word *,int  ,int);
int E803_fp_split( E803word * , E803word *);
void E803_fp_join( E803word *, int *, E803word *);
int E803_mant_add( E803word *, E803word *);
int E803_mant_sub( E803word *, E803word *);
void E803_rotate_left(E803word *, int * );
void E803_shift_left_F65( E803word *,int * );
void E803_SCR_to_STORE(int32_t  *SCR, E803word *wptr);
int E803_add56(E803word *a, E803word *b) ;
int E803_sub56(E803word *a, E803word *b) ;
int E803_shift_left56(E803word *acc, E803word *ar );



// Some bit masks for specific bits in an 803 word.
#define Bits38     (0x3FFFFFFFFFLL)
#define Bits39     (0x7FFFFFFFFFLL)
#define Bits40     (0xFFFFFFFFFFLL)
#define BitsSign   (0x4000000000LL)
#define BitsSign2  (0x8000000000LL)
#define BitsCarry  (0x4000000000LL)
#define BitsOver   (0x8000000000LL)
