/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

/* Lower level 803 instruction emulation. */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <glib.h>
#include "E803-types.h"
#include "E803ops.h"
#include "Emulate.h"
#include "wg-definitions.h"
#include "Wiring.h"
#include "Common.h"
#include "Sound.h"

#define TRACE 0

// Defined in Cpu.c
extern unsigned int WG_ControlButtons;    // State of the buttons like Reset etc 
extern bool WG_operate_pressed;

/* Various 803 registers */
E803word ACC;           // The Accumulator
E803word AR;            // Auxillary register used in double length working
E803word MREG;          // Multipler
E803word STORE_CHAIN;   // Data read from and written back into store
E803word WG;            // value on the Word Generator buttons
E803word QACC,QAR;      // Q register for double length multiply  
E803word MANTREG;       // Mantissa and Exponents in fp maths 
int T;                  // Number of places to shift in Gp 5 
int EXPREG;

/* Short values */
int32_t BREG;       // For B modification 
int32_t IR;         // Instruction register 
int32_t SCR;        // Sequence Control Register 
int32_t STORE_MS;   // Top half on store chain 
int32_t STORE_LS;   // Bottom half on store chain 

/* Some 803 constants */
E803word E803_ONE  = 1;
E803word E803_ZERO = 0;
E803word E803_AR_MSB= 02000000000000; // Rounding bit for Fn 53 


/* Various 803 internal sisnals */
bool S;
bool SS25;
bool WI;         // Something to do with manual data
bool SS2,SS3;    // Timing for operate to clear busy on Fn70
bool R;          //  Beat counter (R=Fetch,!R=Execute)
bool FPO;        // Gp6 / floating point overflow 
bool OFLOW;      // Integer overflow flag
bool PARITY;
bool L;          // Long function, prolongs R-bar (execute) phase
bool LW;         // Last word time in some long functions */
bool B;          // Busy
bool M;          // B Modifier flag
bool N;          // Read Button
bool ALWAYS = true;   /* used for conditional jump decoding */
bool NEGA,Z;
bool GPFOUR;     // surpresses execute phase for jumps 
bool TC;         // Transfer Contol for jumps
bool J;          // Interrrupt request (unused) 


bool *Conditions[] = {&ALWAYS,&NEGA,&Z,&OFLOW};


/* Emulation variables */
int32_t IR_saved;

/* working space */
E803word tmp;

bool *DM160s_bits[] = {&PARITY,&L,&B,&FPO,&S,&OFLOW};
int DM160s_bright[7];
bool CpuRunning = false;

int PTSBusyBright;

int16_t CPUVolume = 0x100;


/* The next two run since the emulator was started */
int CPU_word_time_count = 0;
unsigned int CPU_word_time_stop = 1;
/* The next one refers to the current emulation cycle */
unsigned int word_times_done =  0;

E803word *CoreStore = NULL; 

void fn00(void); void fn01(void); void fn02(void); void fn03(void);
void fn04(void); void fn05(void); void fn06(void); void fn07(void);

void fn10(void); void fn11(void); void fn12(void); void fn13(void);
void fn14(void); void fn15(void); void fn16(void); void fn17(void);

void fn20(void); void fn21(void); void fn22(void); void fn23(void);
void fn24(void); void fn25(void); void fn26(void); void fn27(void);

void fn30(void); void fn31(void); void fn32(void); void fn33(void);
void fn34(void); void fn35(void); void fn36(void); void fn37(void);

void fn40(void); void fn41(void); void fn42(void); void fn43(void);
void fn44(void); void fn45(void); void fn46(void); void fn47(void);

void fn50(void); void fn51(void); void fn52(void); void fn53(void);
void fn54(void); void fn55(void); void fn56(void); void fn57(void);

void fn60(void); void fn61(void); void fn62(void); void fn63(void);
void fn64(void); void fn65(void); void fn66(void); void fn67(void);

void fn70(void); void fn71(void); void fn72(void); void fn73(void);
void fn74(void); void fn75(void); void fn76(void); void fn77(void);


/** Jump table for 803 op codes */
void (*functions[])(void) =
{ fn00,fn01,fn02,fn03,fn04,fn05,fn06,fn07,
  fn10,fn11,fn12,fn13,fn14,fn15,fn16,fn17,
  fn20,fn21,fn22,fn23,fn24,fn25,fn26,fn27,
  fn30,fn31,fn32,fn33,fn34,fn35,fn36,fn37,
  fn40,fn41,fn42,fn43,fn44,fn45,fn46,fn47,
  fn50,fn51,fn52,fn53,fn54,fn55,fn56,fn57,
  fn60,fn61,fn62,fn63,fn64,fn65,fn66,fn67,
  fn70,fn71,fn72,fn73,fn74,fn75,fn76,fn77};


// New version 4/11/19
static void FetchStore(int32_t address,
		       int32_t *MSp, 
		       int32_t *LSp, 
		       E803word *STORE_READ)
{
    E803word readWord;
    
    address &= 8191;
    readWord = CoreStore[address];
  
    *STORE_READ = readWord;
    
    *LSp = readWord &  0xFFFFF;  /* Bottom 20 bits */

    readWord >>= 20;
    
    *MSp = readWord &  0xFFFFF; /* Top 20 bits */
}


void setCPUVolume(unsigned int level)
{
    CPUVolume = (int16_t) level;
}

// Called before emulate to handle button presses etc.
void PreEmulate(bool updateFlag)
{
    // Check if the machine is on 
    if(CpuRunning)
    {
	if(WG_operate_pressed)
	{
	    //printf("Operate Bar pressed\n");
	    if(S) SS25 = true;
	    if(WI && !R) SS3 = true; /* 17/4/06 added !R */
	}

	if(SS25) PARITY = FPO = false;
    }

    if(updateFlag)
    {
	for(int n=0; n<7; n+=1)
	{
	    DM160s_bright[n] = 0;
	}
    }
    
    PTSBusyBright = 0;
    word_times_done = 0;
}


void PostEmulate(bool updateFlag)
{
    LampsEvent *le;
    
    if(updateFlag)
    {
	for(unsigned int n = 0; n < 7; n++)
	{
	    le = (LampsEvent *) malloc(sizeof(LampsEvent));
	    le->lampId = n+1; // 1U << n;
	    le->on = DM160s_bright[n] > 0 ? TRUE : FALSE;
	    le->brightness = (gfloat) DM160s_bright[n];
	    
	    g_async_queue_push(LampsEventQueue,(gpointer) le);
	}
    }
}

void Emulate(int wordTimesToEmulate)
{
    static int ADDRESS,fn;
    
    while(wordTimesToEmulate--)
    {
	CPU_word_time_count += 1;
	if(CpuRunning)
	{
	    /* This is the heart of the emulation.  It fetches instructions and executes them. */
	    if (R)
	    { /* fetch */
		// 23/2/10 There may be more to do when reset is pressed, but not reseting OFLOW was the
		// visible clue to the bug!
		if (WG_ControlButtons & WG_reset)
		{
		    OFLOW = 0;
		}
		if (!S)
		{ // Not stopped
		    if (WG_ControlButtons & WG_clear_store)
		    {
			STORE_CHAIN = E803_ZERO;
			STORE_LS = STORE_MS = 0;
			M = 0;
			if ((ADDRESS = IR & 8191) >= 4)
			{
			    CoreStore[ADDRESS] = STORE_CHAIN;
			}
		    }
		    else
		    {
			FetchStore(IR, &STORE_MS, &STORE_LS, &STORE_CHAIN);
			if (SCR & 1) /* H or D */
			{ /* F2 N2 */
			    if (M == 0)
			    { /* No B-mod */
				IR = STORE_LS;
				BREG = 0;
			    }
			    else
			    { /* B-Mod */
				IR = STORE_LS + BREG;
				BREG = 0;
				M = false;
			    }
			}
			else
			{ /* F1 N1 */
			    IR = STORE_MS;
			    BREG = STORE_LS; /* Save for B-mod later */
			    M = (STORE_LS & 0x80000) ? true : false;
			}
		    }
		}

		IR_saved = IR;

		/* Need to check RON to see if S should be set */
		S |= (WG_ControlButtons & (WG_read | WG_obey | WG_reset)) ? true : false;

		/* Selected stop */
		if(WG_ControlButtons & WG_selected_stop)
		{
		    if((WG & 017777) == ((SCR >> 1) & 017777))
		    {
			S = true;
		    }
		}

		/* Do a single instruction if operate been pressed */
		if (SS25 && (WG_ControlButtons & (WG_normal |WG_obey)))
		{
		    SS25 = S = false;
		}

		if (SS25 && (WG_ControlButtons & WG_read))
		{
		    N = true;
		    M = false;
		    SS25 = false;
		}
		
		if (!S)
		{
		    fn = (IR >> 13) & 077;
#if TRACE
		    if (traceChannel != NULL)
		    {
			g_string_truncate(traceText,0);	

			g_string_printf(traceText,"\nSCR= %"PRId32"%c IR = %02o %4"PRId32" \nACC=",
					(SCR>>1),SCR & 1 ? '+' : ' ', fn,IR & 8191);
			traceText = dumpWord(traceText,&ACC);

			g_string_append_printf(traceText,"\nAR =");
			traceText = dumpWord(traceText,&AR);

			g_string_append_printf(traceText,"\nSTR=");
			traceText= dumpWord(traceText,&CoreStore[IR & 8191]);

			//g_string_append_c(traceText,');
			g_io_channel_write_chars(traceChannel,traceText->str,-1,NULL,NULL);
			//printf("%s\n",traceText->str);
		    }
#endif
		    if ((fn & 070) == 040)
		    {
			GPFOUR = true;

			if (*Conditions[fn & 3]) /* if(TC)  */
			{
			    SCR = ((IR & 8191) << 1) + ((fn >> 2) & 1);
			    M = false;

			    if ((fn & 3) == 3)
				OFLOW = 0; 
			    TC = true; /* set TC */
			}
			else
			{
			    TC = false;

			    SCR += 1;
			    SCR &= 16383;
			}

			IR = SCR >> 1;
		    }
		    else
		    {
			GPFOUR = false;
			R = !R;
		    }

		    if (fn & 040)
		    {
			addSamplesFromCPU(0x0000, CPUVolume);
		    }
		    else
		    {
			addSamplesFromCPU(0x0000,0x0000);
		    }
		}
		else
		{ /* S == TRUE  --> stopped */
		    if (N)
		    {
			IR = (WG >> 20) & 0x7FFFF;
			N = false;
		    }

		    addSamplesFromCPU(0x0000,0x0000);
		}
		TC = GPFOUR = false;
	    }
	    else
	    { /* execute R-bar*/
		if (fn & 040)
		{
		    addSamplesFromCPU( CPUVolume, CPUVolume);
		}
		else
		{
		    addSamplesFromCPU(0x0000,0x0000);
		}

		ADDRESS = IR & 8191;

		if (ADDRESS >= 4)
		{
		    STORE_CHAIN = CoreStore[ADDRESS];
		}
		else
		{
		    STORE_CHAIN = E803_ZERO;
		}

		/* Call the handler for the current instruction */

		(functions[fn])();

		if (ADDRESS >= 4)
		{
		    CoreStore[ADDRESS] = STORE_CHAIN;
		}

		Z = (ACC & Bits39) ? false : true;

		NEGA = (ACC & BitsSign) ? true : false;
		
		/* Added PTSBusy to variables to set cleared by reset
		   Fri Aug  8 20:20:31 BST 1997*/
		{

		    if (WG_ControlButtons & (WG_reset | WG_clear_store))
		    {
			OFLOW = B = L = J = 0; /*= F77State*/
		    }
		}

		if ( !(L | B))
		{
		    R = !R;

		    SCR += 1;
		    SCR &= 16383;

		    /* If M is set, don't replace previous address in IR with SCR */
		    if (!M)
		    {
			IR = SCR >> 1;
		    }
		}
	    }

	    DM160s_bright[6]+=1;  // Use for a maximum value
	    for(int n=0;n<6;n++)
	    {
		// If the signal is up, increase the brightness
		if(*DM160s_bits[n]) DM160s_bright[n]+=1;
	    }
	}
	else
	{  // The computer is turned off, but there are still thing to do....
	    addSamplesFromCPU(0x0000,0x0000);
	}
    }
}


// nop
void fn00(void)
{

}

// a' = -a   n' = n
void fn01(void)
{
    OFLOW |= E803_neg(&ACC,&ACC);
}

// a' = n+1   n' = n
void fn02(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_add(&E803_ONE,&ACC);
}

// a' = a & n   n' = n
void fn03(void)
{
    E803_and(&STORE_CHAIN,&ACC);
}

// a' = a + n   n' = n
void fn04(void)
{
    OFLOW |= E803_add(&STORE_CHAIN,&ACC);
}

// a' = a - n   n' = n
void fn05(void)
{
    OFLOW |= E803_sub(&STORE_CHAIN,&ACC);
}

// a' = 0   n' = n
void fn06(void)
{
    ACC = E803_ZERO;
}

// a' = n - a   n' = n
void fn07(void)
{
    OFLOW |= E803_neg_add(&STORE_CHAIN,&ACC);
}

//a' = n   n' = a   
void fn10(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
}

// a' = -n   n' = a
void fn11(void)
{
    tmp = ACC;
    OFLOW |= E803_neg(&STORE_CHAIN,&ACC);
    STORE_CHAIN = tmp;
}

// a' = n+1   n' = a
void fn12(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    OFLOW |= E803_add(&E803_ONE,&ACC);
}

// a' = a & n   n' = a
void fn13(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    E803_and(&STORE_CHAIN,&ACC);
}

// a' = a + n   n' = a
void fn14(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    OFLOW |= E803_add(&STORE_CHAIN,&ACC);
}

// a' = a - n   n' = a
void fn15(void)
{
    tmp = STORE_CHAIN;
    STORE_CHAIN = ACC;
    E803_sub(&tmp,&ACC);
}

// a' = 0   n' = a
void fn16(void)
{
    STORE_CHAIN = ACC;
    ACC = E803_ZERO;
}

// a' = n-a   n' = a
void fn17(void)
{
    tmp = STORE_CHAIN;;
    STORE_CHAIN = ACC;
    OFLOW |= E803_neg_add(&tmp,&ACC);
}

// a' = a'   n' = a
void fn20(void)
{
    STORE_CHAIN = ACC;
}

// a' = a'   n' = -a
void fn21(void)
{
    OFLOW |= E803_neg(&ACC,&STORE_CHAIN);  
}

// a' = a'   n' = n + 1
void fn22(void)
{
    OFLOW |= E803_add(&E803_ONE,&STORE_CHAIN); 
}

// a' = a'   n' =  a & n
void fn23(void)
{
    E803_and(&ACC,&STORE_CHAIN); 
}

// a' = a'   n' = a + n
void fn24(void)
{
    OFLOW |= E803_add(&ACC,&STORE_CHAIN); 
}

// a' = a'   n' = a - n
void fn25(void)
{
    OFLOW |= E803_neg_add(&ACC,&STORE_CHAIN); 
}

// a' = a'   n' = 0
void fn26(void)
{
    STORE_CHAIN = E803_ZERO;
}

// a' = a'   n' = n - a
void fn27(void)
{
    OFLOW |= E803_sub(&ACC,&STORE_CHAIN); 
}

// a' = n   n' = n
void fn30(void)
{
    ACC = STORE_CHAIN;
}

// a' = n   n' = -a
void fn31(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_neg(&STORE_CHAIN,&STORE_CHAIN);
}

// a' = n   n' = n + 1
void fn32(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_add(&E803_ONE,&STORE_CHAIN);
}

// a' = n   n' = a & n
void fn33(void)
{
    tmp = STORE_CHAIN;
    E803_and(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

// a' = n   n' = a + n
void fn34(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_add(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

// a' = n   n' = a - n
void fn35(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_neg_add(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

// a' = n   n' = 0
void fn36(void)
{
    ACC = STORE_CHAIN;
    STORE_CHAIN = E803_ZERO;
}

// a' = n   n' = n - a
void fn37(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_sub(&ACC,&STORE_CHAIN);
    ACC = tmp;
}


// Jumps are handled in the fetch/emulate loop as they
// don't have an execute beat.
void fn40(void)
{
}

void fn41(void)
{
}

void fn42(void)
{
}

void fn43(void)
{
}

void fn44(void)
{
}

void fn45(void)
{
}

void fn46(void)
{
}

void fn47(void)
{
}

// Double length arithmetic shift right
void fn50(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
    }

    if(T--< 0)
    { /* Last Word */
	L = false;
    }
    else
    {
	E803_signed_shift_right(&ACC,&AR);
    }
}


// Single length right shift. Clear AR
void fn51(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
    }

    if(T--< 0)
    { /* Last Word */
	L = false;
	AR = E803_ZERO;
    }
    else
    {
	E803_unsigned_shift_right(&ACC);
    }

}

//Double length mutiply
void fn52(void)
{
    int m,n,action;


    if(!L)
    {  /* First Word time */

	L = true;
	MREG = STORE_CHAIN;
	E803_Double_M(&MREG);    /* shift one bit left so that M.bytes[0] &
				    3 gives the action code */
	E803_Acc_to_Q(&ACC,&QACC,&QAR);
      
	ACC = AR = E803_ZERO;
	return;
    }

    action = MREG & 3;
    switch(action)
    {
	case 0:   break;

	case 1:   /* Add */
	    OFLOW |= E803_dadd(&QACC,&QAR,&ACC,&AR);
	    break;

	case 2:   /* Subtract */
	    OFLOW |= E803_dsub(&QACC,&QAR,&ACC,&AR);
	    break;
		
	case 3:   break;
    }

    E803_shift_left(&QACC,&QAR);
    n = E803_Shift_M_Right(&MREG);
    m = (n >> 8) & 0xFF;
    n = n & 0xFF;

    if( (n == 0xFF) || (m == 0x00) )
    {
	L = false;
    }
}

// Single length multiply.  Clear AR
void fn53(void)
{
    int m,n,action;


    if(!L)
    {  /* First Word time */

	L = true;
	MREG = STORE_CHAIN;
	E803_Double_M(&MREG);    /* shift one bit left so that M.bytes[0] &
				    3 gives the action code */
	E803_Acc_to_Q(&ACC,&QACC,&QAR);
      
	ACC = AR = E803_ZERO;
	return;
    }

    if(LW)
    {
	L = LW = false;
	OFLOW |= E803_dadd(&E803_ZERO,&E803_AR_MSB,&ACC,&AR);
	AR = E803_ZERO;
    }
    else
    {
	action = MREG & 3;
	switch(action)
	{
	    case 0:   break;
		   
	    case 1:   /* Add */
		OFLOW |= E803_dadd(&QACC,&QAR,&ACC,&AR);
		break;

	    case 2:   /* Subtract */
		OFLOW |= E803_dsub(&QACC,&QAR,&ACC,&AR);
		break;
		
	    case 3:   break;
	}

	E803_shift_left(&QACC,&QAR);
	n = E803_Shift_M_Right(&MREG);
	m = (n >> 8) & 0xFF;
	n = n & 0xFF;

	if( (n == 0xFF) || (m == 0x00) )
	{
	    LW = true;
	}
    }
}

// Double length arithmetic shift left 
void fn54(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
    }

    /* Note first and last words can be the same word for 0 bit shift */
  
    if(T-- < 0)
    {   /* Last Word */
	L = false;
    }
    else
    {
	OFLOW |= E803_shift_left(&ACC,&AR);
    }
}

// Single length sift left.  Clear AR
void fn55(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
	AR = E803_ZERO;   /* This is not quite what happens on a real 803.
			     The AR should be cleared during LW, but clearing
			     it here allows the use of the normal double
			     length left shift function */
    }

    /* Note first and last words can be the same word for 0 bit shift */
  
    if(T-- < 0)
    {   /* Last Word */
	L = false;
	AR = E803_ZERO;
    }
    else
    {
	OFLOW |= E803_shift_left(&ACC,&AR);
    }
}


// Double length divide, sinlge length answer.  Clear AR
void fn56(void)
{
    static E803word M_sign;
    E803word ACC_sign;

    if(!L)
    {  /* First Word time */
	uint64_t *m,mm,n;

    

	L = true;
	MREG = STORE_CHAIN;

	/* Bug fixed 11/4/2010   The remainder needs an extra bit so
	   MREG and ACC/AR are sign extended to 40 bits and special 
	   versions of add and subtract are used.
	   Oddly the listings of the DOS version from 1992 have 
	   code to shift these values one place right. 
	*/

	m =  &MREG;
	mm = n = *m;
	n  &= 0x4000000000LL;   // Sign bit
	mm &= 0x7FFFFFFFFFLL;   // 39 bits

	n *= 6; // Two duplicate sign bits
	*m =mm | n;
	
	m =  &ACC;
	mm = n = *m;
	n  &= 0x4000000000LL;   // Sign bit
	mm &= 0x7FFFFFFFFFLL;   // 39 bits

	n *= 6; // Two duplicate sign bits
	*m =mm | n;
	
	M_sign = MREG;
       
	T = 40;   /* ? */

	QAR = QACC = E803_ZERO;  /* Clear QAR so I can use the double
				    length left shift on QACC */
	/*printf("  ");       
	  dumpACCAR();
	  printf("\n");
	*/
	return;
    }

    ACC_sign = 0;
   
    if(T--)
    {
#if TRACE
	if(traceChannel != NULL)
	{
	    g_string_truncate(traceText,0);	
	    g_string_printf(traceText,"\n%s T=%d\nMREG=",__FUNCTION__,T);
	    dumpWord(traceText,&MREG);
	    g_string_append_printf(traceText,"\nACC =");
	    dumpWord(traceText,&ACC);
	    g_string_append_printf(traceText,"\nAR  =");
	    dumpWord(traceText,&AR);
	    g_string_append_printf(traceText,"\nQACC=");
	    dumpWord(traceText,&QACC);
      }
#endif
	if(T == 39)
	{  /* Second word */
	    ACC_sign = ACC;
	}
       
	E803_shift_left(&QACC,&QAR);
	if((ACC ^ M_sign) & 0x8000000000)
	{  /* Signs different , so add */
	    E803_add56(&MREG,&ACC);
	}
	else
	{  /* Signs same, so subtract */
	    E803_sub56(&MREG,&ACC);
	    if(T != 39) QACC |= 1;
	}
#if TRACE
	if(traceChannel != NULL)
	{
	  g_string_append_printf(traceText,"\nACC2=");
	  dumpWord(traceText,&ACC);
	  g_string_append_printf(traceText,"\nAR2 =");
	  dumpWord(traceText,&AR);
	  g_string_append_c(traceText, '\n');
	  g_io_channel_write_chars(traceChannel,traceText->str,-1,NULL,NULL);	  
	}
#endif

	if(T == 39)
	{
	    if(( (ACC ^ ACC_sign) & 0x8000000000) == 0)
	    {
		OFLOW |= true;
	    }
	}
	E803_shift_left56(&ACC,&AR);
    }
    else
    {   /* Last word */
	ACC = QACC;
	AR = E803_ZERO;
	L = false;
    }
}

// 6/3/10  BUG FIXED.  Fn 57 does NOT clear the AR as it is not 
// a long instruction so LW is not up.
void fn57(void)
{
    E803_AR_to_ACC(&ACC,&AR);
}

/* NOTE on Gp 6 timings.
   Although on the real machine Gp 6 functions carry on into the the
   next instruction's R & RBAR times (to complete standardising shift),
   for the emulation everything has to be completed before L is reset.
*/


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





static void fn6X(int FN)
{
    int ACCexp,STOREexp,diff,negdiff,RightShift,LeftShift,NED,NED16,NEDBAR16,K;
    int oflw,round;
    E803word ACCmant,STOREmant,VDin,TMPmant;
  
    if(!L)
    {  /* First Word time */
	L = true;
	round = 0;

	if(FN != 062)
	{
	    ACCexp   = E803_fp_split(&ACC,&ACCmant);
	    STOREexp = E803_fp_split(&STORE_CHAIN,&STOREmant);
	}
	else
	{
	    /* Just reverse the input parameters */
	    ACCexp   = E803_fp_split(&STORE_CHAIN,&ACCmant);
	    STOREexp = E803_fp_split(&ACC,&STOREmant);
	}
/*
	if(trace != NULL)
	{
	    fprintf(trace,"ACCexp=%d ACCmant=",ACCexp);
	    dumpWordFile(trace,&ACCmant);
	    fprintf(trace," STOREexp=%d STOREmant=",STOREexp);
	    dumpWordFile(trace,&STOREmant);
	    fprintf(trace,"\n");
	}
*/
	K = NED = NED16 = NEDBAR16 = false;

	diff    = ACCexp - STOREexp;
	negdiff = STOREexp - ACCexp;

	if(diff < 0) NED = true;
       
	RightShift = 0;
       
	if(diff & 0x1E0)
	{
	    NED16 = true;
	    RightShift = 32 - ( diff & 0x1F );
	}

	if(negdiff & 0x1E0)
	{
	    NEDBAR16 = true;
	    RightShift = 32 - ( negdiff & 0x1F );
	}

	if(NED16 && NEDBAR16) RightShift = 32;

	if(NED)
	{
	    VDin = ACCmant;
	    ACCexp = STOREexp;
	}
	else
	{
	    VDin = STOREmant;
	}
	/* Since Add only keeps one extra significant bit, test bottom
	   7 for rounding bits */
	if(RightShift) round |= E803_mant_shift_right(&VDin,RightShift,7);

	if(NED)
	{
	    ACCmant = VDin;
	}
	else
	{
	    STOREmant = VDin;
	}

	if(FN == 060)
	{
/*	    if(trace != NULL)
	    {
		fprintf(trace,"FN60 before STOREmant=");
		dumpWordFile(trace,&STOREmant);
		fprintf(trace," ACCmant=");
		dumpWordFile(trace,&ACCmant);
		fprintf(trace,"\n");
	    }
*/
	    oflw = E803_mant_add(&STOREmant,&ACCmant);
/*
	    if(trace != NULL)
	    {
		fprintf(trace,"FN60 after  STOREmant=");
		dumpWordFile(trace,&STOREmant);
		fprintf(trace," ACCmant=");
		dumpWordFile(trace,&ACCmant);
		fprintf(trace," oflw=%d\n",oflw);
	    }
*/
	}
	else
	{
	    oflw = E803_mant_sub(&STOREmant,&ACCmant);
	}

	LeftShift = 0;
	if( oflw )
	{   /* The mant overflowed */
	    K = true;
	}
	else
	{   /* No overflow, so we may need to standardise */
	    /* May need to check for zero here too */

	    TMPmant = ACCmant;
	    if(E803_mant_shift_right(&TMPmant,31,7) == 0)
	    {
		ACCexp = 0;
		ACCmant = E803_ZERO;
	    }
	    else
	    {
		while( !oflw )
		{
		    TMPmant = ACCmant;
		    oflw = E803_mant_add(&TMPmant,&ACCmant);
		    LeftShift += 1;
		}
		LeftShift -= 1;
		ACCmant = TMPmant;
	    }
	}

	if(K)
	{
	    RightShift = 1;
	    round |= E803_mant_shift_right(&ACCmant,RightShift,7);
	    ACCexp += 1;
	}

	ACCexp -= LeftShift;
       
	if(ACCexp < 0)
	{
	    /* Exp underflow !! */
	    ACCexp = 0;
	    ACCmant = E803_ZERO;
	}

	if(ACCexp > 511)
	{
	    S = true;
	    FPO = true;
	}

	if( ACCmant & 0x80) round = 1;
       
	if(round)
	{   /* Force the rounding bit */
	    ACCmant |= 0x100;
	}
/*
	if(trace != NULL)
	{
	    fprintf(trace,"PreJoin  ACCmant=");
	    dumpWordFile(trace,&ACCmant);
	    fprintf(trace," ACCexp%d\n",ACCexp);
	}
*/
	E803_fp_join(&ACCmant,&ACCexp,&ACC);
/*
	if(trace != NULL)
	{
	    fprintf(trace,"PostJoin ACC    =");
	    dumpWordFile(trace,&ACC);
	    fprintf(trace,"\n");
	}
*/
    }
    else
    {  /* Second word time */
	L = false;
	AR = E803_ZERO;
    }
    return;
}

#if TRACE
static
void dumpWordFile(FILE *fp,E803word *wp)
{

    E803word bit;
    int n;
    E803word w;
    w = *wp;
    bit = 0x8000000000;
    
    n = 40;
    while(n--)
    {
	fputc((w&bit)?'1':'0',fp);
	bit >>= 1;
    }
//  REG_decode(&wp->bytes[0],text);
}

#endif

// a' = a + n
void fn60(void)
{
    fn6X(060);
}

// a' = a - n
void fn61(void)
{
    fn6X(061);
}

// a' = n - a
void fn62(void)
{
    fn6X(062);
}

// a' = a * n
void fn63(void)
{
    int ACCexp,STOREexp,op,oflw,LeftShift;
    E803word TMPmant;
    static E803word ACCmant;
    static int round; 
   
    if(!L)
    {  /* First Word time */
	L = true;
	T = 16 ; /* ? */
	round = 0;

	ACCexp = E803_fp_split(&ACC,&ACCmant);

	/* Multiplier mantissa to MREG */
	STOREexp = E803_fp_split(&STORE_CHAIN,&MREG);

	EXPREG = ACCexp + STOREexp;
      
	MANTREG = E803_ZERO;
    }

    op = (*((int *) &MREG) >> 7) & 0x7;
  
    round += E803_mant_shift_right(&MANTREG,2,6);  /* Note 2 extra bits */  
    switch(op)
    {
	case 0:
	    break;
	case 1:
	case 2:
	    E803_mant_add(&ACCmant,&MANTREG);
	    break;
	case 3:
	    E803_mant_add(&ACCmant,&MANTREG);
	    E803_mant_add(&ACCmant,&MANTREG);
	    break;
	    
	case 4:
	    E803_mant_sub(&ACCmant,&MANTREG);
	    E803_mant_sub(&ACCmant,&MANTREG);
	    break;
	case 5:
	case 6:
	    E803_mant_sub(&ACCmant,&MANTREG);
	    break;
	case 7:
	    break;
    }

    E803_mant_shift_right(&MREG,2,1);

    T -= 1;
    if( T == 1)
    {  /* This is the word time when "end" is set */
	MREG = E803_ZERO;
    }

    if(T == 0)
    {  /* This is the AS & FR & SD word times combined */
	L = 0;
	oflw = 0;
	LeftShift = 0;
	TMPmant = MANTREG;
	if(E803_mant_shift_right(&TMPmant,31,7) == 0)
	{
	    ACCexp = 0;
	    ACCmant = E803_ZERO; 	
	    /* Bug fixed 27/3/05   Multiply by zero was NOT giving zero result!
	       The next to lines were needed */
	    EXPREG = 0;
	    MANTREG = E803_ZERO;
	}
	else
	{
	    while( !oflw )
	    {
		TMPmant = MANTREG;
		oflw = E803_mant_add(&TMPmant,&MANTREG);
		LeftShift += 1;
	    }
	    LeftShift -= 1;
	    MANTREG = TMPmant;
  
	    EXPREG -= (255 + LeftShift);

	    if(EXPREG < 0)
	    {
		/* Exp underflow !! */
		EXPREG = 0;
		MANTREG = E803_ZERO;
		round = 0;
	    }
       
	    if(EXPREG > 511)
	    {
		S = true;
		FPO = true;
	    }
      
	    /* Do I need to check the MANTREG for bits which should set
	       round ?   I think I do.... */

	    TMPmant = MANTREG;
	    round |= E803_mant_shift_right(&TMPmant,2,6);
      
	    if(round)
	    {   /* Force the rounding bit */
		MANTREG |= 0x100;
	    }
	}

	E803_fp_join(&MANTREG,&EXPREG,&ACC);
	AR = E803_ZERO;
    }
    return;
}

  
// a' =  a / n
void fn64(void)
{
    static E803word TBIT =  0x1000000000; 
    static E803word TsignBit = 0xE000000000;
    static E803word TshiftBit,TBit;
    static E803word ACCmant;
    int MantZ,Same,oflw,LeftShift,STOREexp,ACCexp;
    static int firstbit,exact;
    E803word TMPmant;
    bool DivByZero;

    if(!L)
    {
	/* First word time */
	TBit = E803_ZERO;     /* To ignore the first bit in the answer */
	TshiftBit = TBIT;
	firstbit = true;
	T = 0;   /* 31 bits of quotient to form */
	L = true;
	
	exact = false;
       
	/* divisor  mantissa from store  to MREG */
	STOREexp = E803_fp_split(&STORE_CHAIN,&MREG);

	/* Split the dividend */
	ACCexp = E803_fp_split(&ACC,&ACCmant);

	EXPREG = ACCexp - STOREexp;
      
	MANTREG = E803_ZERO;   /* Form result in here */
	QACC = E803_ZERO;
      
	E803_mant_shift_right(&ACCmant,1,1); 

	DivByZero = ((STORE_CHAIN & Bits40) != 0)  ? false : true;
	
	if(DivByZero)
	{
	    S = true;
	    FPO = true;
	}
    }
    else
    {
	Same = !((ACCmant ^ MREG) & 0x8000000000);
	MantZ = ((ACCmant & Bits40) != 0) ? false : true;

	if(MantZ) exact = true;

	if(MantZ || Same)
	{
	    E803_mant_sub(&MREG,&ACCmant);
	    E803_mant_add(&TBit,&MANTREG);
	}
	else
	{
	    E803_mant_add(&MREG,&ACCmant);
	}

	E803_mant_add(&ACCmant,&ACCmant);

	if(firstbit)
	{
	    TBit = TsignBit;
	    firstbit = false;
	}
	else
	{
	    TBit = TshiftBit;
	    E803_mant_shift_right(&TshiftBit,1,1);
	}

#if TRACE
	printf("T=%d \n    TBit=",T);
	dumpWordFile(stdout,&TBit);
	printf("\n    MREG=");
	dumpWordFile(stdout,&MREG);
	printf("\n ACCmant=");
	dumpWordFile(stdout,&ACCmant);
	printf("\n");
#endif
	T += 1;

	if( (T == 32) || (MantZ))
	{

	    L = 0;

	    oflw = 0;
	    LeftShift = 0;

	    MantZ = ((MANTREG & Bits40) != 0) ? false : true;

	    if(MantZ)
	    {
		EXPREG = 0;
		exact = true;
	    }
	    else
	    {
		TMPmant = MANTREG;
		while( !oflw )
		{
		    TMPmant = MANTREG;
		    oflw = E803_mant_add(&TMPmant,&MANTREG);

		    if(!oflw) LeftShift += 1;
		}
		MANTREG = TMPmant;
		EXPREG += (257 - LeftShift);
	    }

	    if(!exact)
	    {   /* Force the rounding bit */
		MANTREG |= 0x100;
	    }
       
	    if(EXPREG < 0)
	    {
		/* Exp underflow !! */
		EXPREG = 0;
		MANTREG = E803_ZERO;
	    }
     	    if(EXPREG > 511)
	    {
		S = true;
		FPO = true;
	    }  
	    E803_fp_join(&MANTREG,&EXPREG,&ACC);
	    AR = E803_ZERO;
	}
    }
}

// Floating point standardise OR fast rotate left
void fn65(void)
{
    int count,EXP,oflw,*ip;
    E803word temp;

    if((IR & 8191) < 4096)
    {   /* Shift */
      count = IR & 63;
      //count = IR & 0x2F;   // Simulate the fault on TNMOC's 803 on 4/4/2010

	if(count == 0) return;
      
	if(count <= 39)
	{
	    E803_rotate_left(&ACC,&count);
	}
	else
	{
	    count -= 39;
	    E803_shift_left_F65(&ACC,&count);
	}
    }
    else
    {   /* Fp standardisation */
	AR = E803_ZERO;

	EXP = ((ACC & Bits40) != 0) ? false : true;
	if(EXP) return;
      
	oflw = false;
      
	EXP = 256+38;
      
	while(1)
	{
	    temp = ACC;
	    oflw = E803_shift_left(&ACC,&AR);
	    if(oflw) break;
	
	    EXP -= 1;
	}

	ACC = temp;
	ip = (int *) &ACC;
      
	if(*ip & 511) *ip |= 512;
      
	*ip &= ~511;
	*ip |= (EXP & 511);
    }
}

// Only implemented on machines used for process control
// TNMOC's 803 has these instructions, so I could validate
// implementations.

// Fast integer divide giving 13 bit result.
// AR not cleared 
void fn66(void)
{
  
}

// Integer square root of accumulator giving 13 bit result.
// AR not cleared
void fn67(void)
{

}

// a' = word generator
void fn70(void)
{
    WI = B = (WG_ControlButtons & WG_manual_data) ? true : false;
  
    if(B) 
    {
	if(SS3)
	{
	    B = false;
	    WI = false;
	    SS3 = false;
	    ACC = WG;
	}
    }
    else
    {
	B = false;
	WI = false;
	SS3 = false;
	ACC = WG;
    }
}


static bool Ready = false;
static unsigned int TRLines = 0;

static void setReady(unsigned int value)
{
    Ready = (value != 0);
}

static void setTRLines(unsigned int value)
{
    TRLines = value;
}


// Read character from tape reader
void fn71(void)
{
    wiring(CLINES,IR&8191);
    wiring(F71,1);    // Send F71 to PTS.  This will set READY if appropriate

    if(Ready)
    {
	wiring(ACT,1);
	ACC |= TRLines & 0x1F;
	wiring(ACT,0);
	wiring(F71,0);
	B = false;
    }
    else
    {
	B = true;
    }
    return;
}    





int plotterAct = 0;
// Output to channel 2
void fn72(void)
{
    wiring(CLINES,IR&8191);
    wiring(F72,1);

    if(Ready)
    {
	wiring(ACT,1);
	wiring(ACT,0);
	wiring(F72,0);
	B = false;
    }
    else
    {
	B = true;
    }

}


// n' = sequence control register.  (link)
void fn73(void)
{
    E803_SCR_to_STORE(&SCR,&STORE_CHAIN);
}

/* Speed is now controlled using the polling techniques developed 
   for the film controler.
*/

int F74punchAt;
// Output tp PTS (punches or teleprinter)
void fn74(void)
{
    wiring(CLINES,IR&8191);
    wiring(F74,1);

    if(Ready)
    {
	wiring(ACT,1);
	wiring(ACT,0);
	wiring(F74,0);
	B = false;
    }
    else
    {
	B = true;
    }
}



void fn75(void)
{
    B = true;
}




void fn76(void)
{
    B = true;
}



void fn77(void)
{
    L = true;
}



static void cpuPowerOn(__attribute__((unused)) unsigned int dummy)
{
    CpuRunning = true;
    S = PARITY = true;
    wiring(UPDATE_DISPLAYS,0);
}

static void cpuPowerOff(__attribute__((unused)) unsigned int dummy)
{
    CpuRunning = false;
    PARITY = false;
    wiring(UPDATE_DISPLAYS,0);
}

#if TRACE
static
void dumpWord(E803word word)
{
    unsigned int f1,n1,b,f2,n2;
    
    f1 = (word >> 33) & 077;
    n1 = (word >> 20) & 017777;
    b =  (word >> 19) & 01;
    f2 = (word >> 13) & 077;
    n2 = word & 017777;

    printf("%02o %4d%c%02o %4d\n",f1,n1,b?'/':':',f2,n2);
    
}
#endif

// Initial instructions
const char *T1[4] = {"264:060","224/163","555:710","431:402"};

// Called from CpuInit
void StartEmulate(char *coreImage)
{
    uint64_t f1,n1,f2,n2,bbit;
    char b;
    E803word II;
    int n;

    if(coreImage == NULL)
    {
	CoreStore = (E803word *) calloc(8194,sizeof(E803word));  //8194 ???
    }
    else
    {
	CoreStore = (E803word *) coreImage;
    }

    for(n=0;n<4;n++)
    {
    
	sscanf(T1[n],"%2" SCNo64 "%" SCNu64 "%c %2" SCNo64 "%" SCNu64,&f1,&n1,&b,&f2,&n2);
	bbit = (b == '/')?1:0 ;
	II = (f1 << 33) | (n1 << 20) | (bbit << 19) | (f2 << 13) | n2;
	CoreStore[n] = II;
    }

    connectWires(SUPPLIES_ON,cpuPowerOn);
    connectWires(SUPPLIES_OFF,cpuPowerOff);
    connectWires(READY,setReady);
    connectWires(TRLINES,setTRLines);
}


