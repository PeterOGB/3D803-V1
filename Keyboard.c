/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
// Keyboard code
#define G_LOG_USE_STRUCTURED

#include <gtk/gtk.h>
#include <GLES3/gl3.h>
#include <cglm/cglm.h>

#include "Gtk.h"
#include "ObjLoader.h"
#include "Keyboard.h"

#include "LoadPNG.h"

#include "ShaderDefinitions.h"

#include "3D.h"

#include "wg-definitions.h"
#include "WGbuttons.h"
#include "Gles.h"
#include "Hands.h"
#include "Wiring.h"
#include "Common.h"
#include "Parse.h"

static GLenum e;
#define CHECK(n) if((e=glGetError())!=0){printf("%d Error %s %x\n",__LINE__,n,e);} 

static GList *KeyboardObjectList = NULL;

OBJECT *WGLampOnObject = NULL;
OBJECT *WGLampOffObject = NULL;
OBJECT *RedButtonObject = NULL;
OBJECT *BlackButtonObject = NULL;
OBJECT *PowerOnButtonObject = NULL;
OBJECT *PowerOffButtonObject = NULL;
OBJECT *PowerShroudObject = NULL;
OBJECT *ShroudObject = NULL;
OBJECT *OperateBarObject = NULL;
OBJECT *VolumeObject = NULL;

OBJECT *ConsoleTopObject = NULL;
OBJECT *ConsoleFrontObject = NULL;

OBJECTINIT keyboardObjects[] = {
    {"WGLampOn",&WGLampOnObject,TRUE},
    {"WGLampOff",&WGLampOffObject,FALSE},
    {"RedButton",&RedButtonObject,TRUE},
    {"BlackButton",&BlackButtonObject,TRUE},
    {"PowerOnButton",&PowerOnButtonObject,TRUE},
    {"PowerOffButton",&PowerOffButtonObject,TRUE},
    {"PowerShroud",&PowerShroudObject,TRUE},
    {"Shroud",&ShroudObject,TRUE},
    {"OperateBar2",&OperateBarObject,TRUE},
    {"Volume",&VolumeObject,TRUE},
    {"ConsoleFront",&ConsoleFrontObject,FALSE},
    {"ConsoleTop2",&ConsoleTopObject,FALSE},
    {NULL,NULL,FALSE}
};

static WGButton *OperateBar;
extern GtkWidget *eventBox;
gfloat operateTopEdgeY;

static gboolean DontWarp = FALSE;

static float volAngle = 0.0;

static void FrontOffset(vec4 PointerXYZ,HandInfo *hand);
static void FrontOffset2(HandInfo *hand);
static void operateBarSound(float beta,guint time);

struct WgZone {
    const char *ZoneName;
    void (*enterHandlerActive)(enum WgZones,HandInfo *MovingHand);
    void (*motionHandlerActive)(vec4,HandInfo *MovingHand,guint time);
    gboolean (*exitHandlerActive)(enum WgZones,HandInfo *MovingHand,guint time);
    void (*enterHandlerInactive)(enum WgZones,HandInfo *MovingHand);
    void (*motionHandlerInactive)(vec4,HandInfo *MovingHand,guint time);
    gboolean (*exitHandlerInactive)(enum WgZones,HandInfo *MovingHand,guint time);
};

static void dummyEnterhandler(enum WgZones FromZone,HandInfo *MovingHand);
static void OutsideMotionHandler(__attribute__((unused)) vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void OutsideEnterHander(enum WgZones FromZone,HandInfo *MovingHand);
static void OutsideEnterHanderInactive(__attribute__((unused)) enum WgZones FromZone,HandInfo *MovingHand);
static gboolean OutsideExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static void WgFrontLeftInnerMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void WgFrontLeftOuterMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void WgFrontRightInnerMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void WgFrontRightOuterMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void WgTopEnterHandler(enum WgZones FromZone,HandInfo *MovingHand);
static gboolean WgTopExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static void WgTopMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void WgFrontLeftInnerEnterHandler(enum WgZones FromZone,HandInfo *MovingHand);
static void WgFrontRightInnerEnterHandler(enum WgZones FromZone,HandInfo *MovingHand);
static void WgFrontRightOuterEnterHandler(enum WgZones FromZone,HandInfo *MovingHand);
static gboolean WgFrontRightInnerExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static gboolean WgFrontRightInnerExitHandlerInactive(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static gboolean WgFrontRightOuterExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static gboolean WgFrontRightOuterExitHandlerInactive(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static gboolean WgFrontLeftInnerExitHandler(enum WgZones ToZone,HandInfo *MovingHan,guint timed);
static gboolean WgFrontLeftInnerExitHandlerInactive(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static gboolean WgFrontLeftOuterExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static gboolean WgFrontLeftOuterExitHandlerInactive(enum WgZones ToZone,HandInfo *MovingHand,guint time);
static void OperateBarMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time);
static void OperateBarEnterHandler(enum WgZones FromZone,HandInfo *MovingHand);
static gboolean OperateBarExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time);

// Handlers for moving hands between Zones.
struct WgZone WgZones[LAST_ZONE] =
{
    {"Outside",
     OutsideEnterHander,OutsideMotionHandler,OutsideExitHandler,
     OutsideEnterHanderInactive,OutsideMotionHandler,OutsideExitHandler},
    {"Top",
     WgTopEnterHandler,WgTopMotionHandler,WgTopExitHandler,
     WgTopEnterHandler,WgTopMotionHandler,WgTopExitHandler},
    {"Front Left Inner",
     WgFrontLeftInnerEnterHandler,WgFrontLeftInnerMotionHandler,WgFrontLeftInnerExitHandler,
     WgFrontLeftInnerEnterHandler,WgFrontLeftInnerMotionHandler,WgFrontLeftInnerExitHandlerInactive},
    {"Front Left Outer",
     dummyEnterhandler,WgFrontLeftOuterMotionHandler,WgFrontLeftOuterExitHandler,
     dummyEnterhandler,WgFrontLeftOuterMotionHandler,WgFrontLeftOuterExitHandlerInactive},
    {"Front Right Inner",
     WgFrontRightInnerEnterHandler,WgFrontRightInnerMotionHandler,WgFrontRightInnerExitHandler,
     WgFrontRightInnerEnterHandler,WgFrontRightInnerMotionHandler,WgFrontRightInnerExitHandlerInactive},
    {"Front Right Outer",
     WgFrontRightOuterEnterHandler,WgFrontRightOuterMotionHandler,WgFrontRightOuterExitHandler,
    WgFrontRightOuterEnterHandler,WgFrontRightOuterMotionHandler,WgFrontRightOuterExitHandlerInactive},
    {"Operate Bar",
     OperateBarEnterHandler,OperateBarMotionHandler,OperateBarExitHandler,
     OperateBarEnterHandler,OperateBarMotionHandler,OperateBarExitHandler}
};

static void OutsideEnterHander(__attribute__((unused)) enum WgZones FromZone,HandInfo *MovingHand)
{
    showCursor();
    MovingHand->useFinger = FALSE;
}

static void OutsideEnterHanderInactive(__attribute__((unused)) enum WgZones FromZone,HandInfo *MovingHand)
{
    MovingHand->useFinger = FALSE;
}

static gboolean OutsideExitHandler(enum WgZones ToZone,HandInfo *MovingHand,__attribute__((unused)) guint time)
{
    if(ToZone == WG_OPERATE)
    {
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
	return FALSE;
    }


    // Need to stop hands entering appropriate INNER zones from outside.
    if(!MovingHand->LeftHand && (ToZone ==  WG_FRONT_LEFT_INNER))
    {
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
	return FALSE;
    }
	
    if(MovingHand->LeftHand && (ToZone ==  WG_FRONT_RIGHT_INNER))
    {
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
	return FALSE;
    }
    
    
    hideCursor();
    warpMouseToXYZ(MovingHand->FingerAtXYZ); 
    MovingHand->useFinger = TRUE;

    return TRUE;
}

static void  OutsideMotionHandler(__attribute__((unused)) vec4 PointerXYZ,
				  __attribute__((unused)) HandInfo *MovingHand,
				  __attribute__((unused)) guint time)
{
}


static void WgFrontLeftInnerEnterHandler(__attribute__((unused)) enum WgZones FromZone,
					 __attribute__((unused)) HandInfo *MovingHand)
{
}

static void WgFrontRightInnerEnterHandler(__attribute__((unused)) enum WgZones FromZone,
					  __attribute__((unused)) HandInfo *MovingHand)
{
}

static void WgFrontRightOuterEnterHandler(__attribute__((unused)) enum WgZones FromZone,
					  __attribute__((unused)) HandInfo *MovingHand)
{
}


static gboolean WgFrontRightInnerExitHandlerInactive(enum WgZones ToZone,
						     HandInfo *MovingHand,
						     guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_RIGHT_OUTER) && (MovingHand->LeftHand) )
    {
	// Pop the operate bar up when hand leaves the zone
	MovingHand->operateAngle = 0.0f;
	operateBarSound(0.0,time);
	// The hand may have been constrained by the operate bar, so warp the
	// mouse to the finger.
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
    }

    if( (ToZone == WG_OPERATE) && (!MovingHand->LeftHand) )
    {
	if((MovingHand->FingerAtXYZ[1]+MovingHand->PushedOffsetXYZ[1]) <= operateTopEdgeY)
	{
	    glm_vec4_add(MovingHand->PreviousPushedOffsetXYZ,
			 MovingHand->PreviousFingerAtXYZ,
			 MovingHand->PreviousFingerAtXYZ);
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    MovingHand->Pushed = NOT_PUSHED;
	    glm_vec4_zero(MovingHand->PreviousPushedOffsetXYZ);
	    FrontOffset(MovingHand->PreviousFingerAtXYZ,MovingHand);
	    glm_vec4_copy(MovingHand->SurfaceOffsetXYZ,MovingHand->PreviousSurfaceOffsetXYZ);
	    
	    return FALSE;
	}
    }
    return TRUE;
}

static gboolean WgFrontRightInnerExitHandler(enum WgZones ToZone,
					     HandInfo *MovingHand,guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_RIGHT_OUTER) && (MovingHand->LeftHand) )
    {
	// Pop the operate bar up when hand leaves the zone
	MovingHand->operateAngle = 0.0f;
	operateBarSound(0.0,time);
	// The hand may have been constrained by the operate bar, so warp the
	// mouse to the finger.
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
    }

    if( (ToZone == WG_OPERATE) && (!MovingHand->LeftHand) )
    {
	if(MovingHand->FingerAtXYZ[1] <= operateTopEdgeY)
	{
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    return FALSE;
	}
    }
    return TRUE;
}

static gboolean WgFrontLeftInnerExitHandlerInactive(enum WgZones ToZone,HandInfo *MovingHand,guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_LEFT_OUTER) && (!MovingHand->LeftHand) )
    {
	// Pop the operate bar up when hand leaves the zone
	MovingHand->operateAngle = 0.0f;
	operateBarSound(0.0,time);
	// The hand may have been constrained by the operate bar, so warp the
	// mouse to the finger.
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
    }

    if( (ToZone == WG_OPERATE) && (MovingHand->LeftHand) )
    {
	if((MovingHand->FingerAtXYZ[1]+MovingHand->PushedOffsetXYZ[1]) <= operateTopEdgeY)
	{

	    glm_vec4_add(MovingHand->PreviousPushedOffsetXYZ,
			 MovingHand->PreviousFingerAtXYZ,
			 MovingHand->PreviousFingerAtXYZ);
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    MovingHand->Pushed = NOT_PUSHED;
	    glm_vec4_zero(MovingHand->PreviousPushedOffsetXYZ);
	    FrontOffset(MovingHand->PreviousFingerAtXYZ,MovingHand);
	    glm_vec4_copy(MovingHand->SurfaceOffsetXYZ,MovingHand->PreviousSurfaceOffsetXYZ);
	    
	    return FALSE;
	}
    }
    return TRUE;
}

static gboolean WgFrontLeftInnerExitHandler(enum WgZones ToZone,HandInfo *MovingHand,guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_LEFT_OUTER) && (!MovingHand->LeftHand) )
    {
	// Pop the operate bar up when hand leaves the zone
	MovingHand->operateAngle = 0.0f;
	operateBarSound(0.0,time);
	// The hand may have been constrained by the operate bar, so warp the
	// mouse to the finger.
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
    }

    if( (ToZone == WG_OPERATE) && (MovingHand->LeftHand) )
    {
	if(MovingHand->FingerAtXYZ[1] <= operateTopEdgeY)
	{
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    return FALSE;
	}
    }
    return TRUE;
}


static gboolean WgFrontRightOuterExitHandlerInactive(enum WgZones ToZone,
						     HandInfo *MovingHand,
						     __attribute__((unused)) guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_RIGHT_INNER) && (MovingHand->LeftHand) )
    {
	if((MovingHand->FingerAtXYZ[1]+InactiveHand->PushedOffsetXYZ[1]) <= operateTopEdgeY)
	{
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    return FALSE;
	}
    }
    return TRUE;
}

static gboolean WgFrontRightOuterExitHandler(enum WgZones ToZone,
					     HandInfo *MovingHand,
					     __attribute__((unused)) guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_RIGHT_INNER) && (MovingHand->LeftHand) )
    {
	if(MovingHand->FingerAtXYZ[1] <= operateTopEdgeY)
	{
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    return FALSE;
	}
    }
    return TRUE;
}   

static gboolean WgFrontLeftOuterExitHandlerInactive(enum WgZones ToZone,
						    HandInfo *MovingHand,
						    __attribute__((unused)) guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_LEFT_INNER) && (!MovingHand->LeftHand) )
    {
	if((MovingHand->FingerAtXYZ[1]+InactiveHand->PushedOffsetXYZ[1]) <= operateTopEdgeY)
	{
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    return FALSE;
	}
    }
    return TRUE;
}   

static gboolean WgFrontLeftOuterExitHandler(enum WgZones ToZone,
					    HandInfo *MovingHand,
					    __attribute__((unused)) guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
	return TRUE;
    }

    if( (ToZone == WG_FRONT_LEFT_INNER) && (!MovingHand->LeftHand) )
    {
	if(MovingHand->FingerAtXYZ[1] <= operateTopEdgeY)
	{
	    warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);
	    return FALSE;
	}
    }
    return TRUE;
}   

static void WgTopEnterHandler(__attribute__((unused)) enum WgZones FromZone,
			      __attribute__((unused)) HandInfo *MovingHand)
{
}


static gboolean WgTopExitHandler(__attribute__((unused)) enum WgZones ToZone,
				 HandInfo *MovingHand,
				 __attribute__((unused)) guint time)
{
    if(ToZone == OFF_PISTE)
    {
	glm_vec4_copy(MovingHand->PreviousFingerAtXYZ,MovingHand->FingerAtXYZ);
	glm_vec4_copy(MovingHand->PreviousPushedOffsetXYZ,MovingHand->PushedOffsetXYZ);
	glm_vec4_copy(MovingHand->PreviousSurfaceOffsetXYZ,MovingHand->SurfaceOffsetXYZ);
    }
    return TRUE;
}


static void OperateBarEnterHandler(__attribute__((unused)) enum WgZones FromZone,
				   __attribute__((unused)) HandInfo *MovingHand)
{
}


static gboolean OperateBarExitHandler(__attribute__((unused)) enum WgZones ToZone,
				      HandInfo *MovingHand,guint time)
{
    if(ToZone == OFF_PISTE)
    {
	MovingHand->PreviousFingerAtXYZ[1] = 1.82f;
	MovingHand->PreviousFingerAtXYZ[2] = 3.5424f;
	FrontOffset2(MovingHand);
	warpMouseToXYZ(MovingHand->PreviousFingerAtXYZ);

	return FALSE;
    }
    
    // Only pop up the operate bar if no fingers are pressing it.
    if( ( (ToZone == WG_FRONT_RIGHT_INNER) && (!MovingHand->LeftHand) ) ||
	( (ToZone == WG_FRONT_LEFT_INNER ) && ( MovingHand->LeftHand) ) )
    {
	// Pop the operate bar up when hand leaves the zone
	MovingHand->operateAngle = 0.0f;
	operateBarSound(0.0,time);
    }
   
    warpMouseToXYZ(MovingHand->FingerAtXYZ);
    return TRUE;
}


static void OperateBarMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time)
{
    float activeBeta;

    glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
    FrontOffset2(MovingHand);

    if(MovingHand->FingerAtXYZ[1] < 1.4f)
    {
	// User previous X value as the value in PointerXYZ may be
	// outside the operate bar edges due to projection into the background.
	MovingHand->FingerAtXYZ[0] = MovingHand->PreviousFingerAtXYZ[0];
	MovingHand->FingerAtXYZ[1] = 1.82f;
	MovingHand->FingerAtXYZ[2] = 3.5524f;
	FrontOffset2(MovingHand);
	warpMouseToXYZ(MovingHand->FingerAtXYZ);
    }
    
    // Limit hands motion when operate bar motion limit is reached.
    if(MovingHand->FingerAtXYZ[1] < 1.82f)
    {
	MovingHand->FingerAtXYZ[1] = 1.82f;
	MovingHand->FingerAtXYZ[2] = 3.5524f;
	FrontOffset2(MovingHand);
    }

    // Rotate the  operate bar when it is being pressed
    if(MovingHand->FingerAtXYZ[1] <= 1.97f)
    {
	activeBeta = 3.3333333f * (1.97f - MovingHand->FingerAtXYZ[1]);
    }
    else
    {
	activeBeta = 0.0f;
    }
    MovingHand->operateAngle = activeBeta;
    operateBarSound(activeBeta,time);
 }

static void PushedOffset(HandInfo *hand)
{
    float z,alpha;
    vec4 pushedXYZ;
    enum WgZones zone;

    // Add X part of PushedOffset to hand to get zone.
    glm_vec4_copy(hand->FingerAtXYZ,pushedXYZ);
    pushedXYZ[0] += hand->PushedOffsetXYZ[0];
    zone = XYZtoZone(pushedXYZ);

    if(hand->LeftHand)
    {
	// Set PushedOffset Y & Z to move the hand to just above the operate bar.
	z = 1.98f -hand->FingerAtXYZ[1];
	if(z > 0.0f)
	{
	    if((zone == WG_OPERATE ) || (zone == WG_FRONT_RIGHT_INNER))
	    {
		hand->PushedOffsetXYZ[1] =  z;
		hand->PushedOffsetXYZ[2] = -z/3.8468f;
	    }
	    if(zone == WG_FRONT_RIGHT_OUTER)
	    {
		// alpha gradually moves the hand up so it will miss the operate
		// bar if it is pushed that far.
		alpha = 1.0f - ( (hand->FingerAtXYZ[0] + hand->PushedOffsetXYZ[0] - 1.82f) /
				 (hand->FingerAtXYZ[0] - 1.82f));
		hand->PushedOffsetXYZ[1] =  alpha * z;
		hand->PushedOffsetXYZ[2] = alpha * -z / 3.8468f;
	    }	    
	}
	else
	{
	   hand->PushedOffsetXYZ[1] = 0.0f;
	   hand->PushedOffsetXYZ[2] = 0.0f;
	}
    }
    else
    {
	// Set PushedOffset Y & Z to move the hand to just above the operate bar.
	z = 1.98f -hand->FingerAtXYZ[1];
	if(z > 0.0f)
	{
	    if((zone == WG_OPERATE ) || (zone == WG_FRONT_LEFT_INNER))
	    {
		hand->PushedOffsetXYZ[1] =  z;
		hand->PushedOffsetXYZ[2] = -z/3.8468f;
	    }
	    if(zone == WG_FRONT_LEFT_OUTER)
	    {
		// alpha gradually moves the hand up so it will miss the operate
		// bar if it is pushed that far.
		alpha = 1.0f - ( (hand->FingerAtXYZ[0] + hand->PushedOffsetXYZ[0]  + 2.2f ) /
				 (  hand->FingerAtXYZ[0] + 2.2f));
		//printf("alpha = %f",(double) alpha);
		hand->PushedOffsetXYZ[1] =  alpha * z;
		hand->PushedOffsetXYZ[2] = alpha * -z / 3.8468f;
	    }	    
	}
	else
	{
	   hand->PushedOffsetXYZ[1] = 0.0f;
	   hand->PushedOffsetXYZ[2] = 0.0f;
	}
    }
}

void FrontOffset2(HandInfo *hand)
{
    float alpha,beta,gamma;
    float boundary = 2.0f;
    vec4 positionXYZ;
    
    hand->NearestButton = NULL;

    glm_vec4_add(hand->FingerAtXYZ,hand->PushedOffsetXYZ,positionXYZ);

    hand->SurfaceOffsetXYZ[0] = 0.0f;
 
    if(positionXYZ[1] > boundary)
    {
	alpha = (2.204f - positionXYZ[1] ) / (2.204f - boundary);
	beta = 1.0f;
    }
    else 
    {
	alpha = 1.0f;
	beta = (positionXYZ[1] - 1.367f) / (boundary - 1.367f);
    }
    if(hand->LeftHand)
    {
   	if(positionXYZ[0] > 0.0f)
	{
	    gamma = 0.2f + (0.5f * positionXYZ[0] / 3.6509f);
	}
	else
	{
	    gamma = 0.2f - (0.3f * positionXYZ[0] / 4.04f);
	}
    }
    else
    {
  	if(positionXYZ[0] < 0.0f)
	{
	    gamma = 0.2f - (0.5f * positionXYZ[0] / 4.04f);
	}
	else
	{
	    gamma = 0.2f + (0.3f * positionXYZ[0] / 3.6509f);
	}
    }

    hand->SurfaceOffsetXYZ[2] = gamma * alpha;
    hand->SurfaceOffsetXYZ[1] = 0.12f * beta; 
}

static void FrontOffset(vec4 PointerXYZ,HandInfo *hand)
{
    float alpha,beta,gamma;
    float boundary = 2.0f;
    
    hand->NearestButton = NULL;

    glm_vec4_copy(PointerXYZ,hand->FingerAtXYZ);

    hand->SurfaceOffsetXYZ[0] = 0.0f;

 
    if(PointerXYZ[1] > boundary)
    {
	alpha = (2.204f - PointerXYZ[1] ) / (2.204f - boundary);
	beta = 1.0f;
    }
    else 
    {
	alpha = 1.0f;
	beta = (PointerXYZ[1] - 1.367f) / (boundary - 1.367f);
    }
    if(hand->LeftHand)
    {
   	if(PointerXYZ[0] > 0.0f)
	{
	    gamma = 0.2f + (0.5f * PointerXYZ[0] / 3.6509f);
	}
	else
	{
	    gamma = 0.2f - (0.3f * PointerXYZ[0] / 4.04f);
	}
    }
    else
    {
  	if(PointerXYZ[0] < 0.0f)
	{
	    gamma = 0.2f - (0.5f * PointerXYZ[0] / 4.04f);
	}
	else
	{
	    gamma = 0.2f + (0.3f * PointerXYZ[0] / 3.6509f);
	}
    }

    hand->SurfaceOffsetXYZ[2] = gamma * alpha;
    hand->SurfaceOffsetXYZ[1] = 0.12f * beta; 
}


static void  WgFrontLeftInnerMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time)
{
    float activeBeta;
    
    glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
    FrontOffset2(MovingHand);

    if(!MovingHand->LeftHand)
    {
	if(MovingHand->FingerAtXYZ[1] < 1.4f)
	{
	    // Because the curosr is off the surface the X,Y,Z values
	    // need to be reset
	    MovingHand->FingerAtXYZ[0] = MovingHand->PreviousFingerAtXYZ[0]; 
	    MovingHand->FingerAtXYZ[1] = 1.82f;
	    MovingHand->FingerAtXYZ[2] = 3.5524f;
	    FrontOffset2(MovingHand);
	    warpMouseToXYZ(MovingHand->FingerAtXYZ);
	}
	
	// Limit hands motion when operate bar motion limit is reached.
	if(MovingHand->FingerAtXYZ[1] < 1.82f)
	{
	    MovingHand->FingerAtXYZ[1] = 1.82f;
	    MovingHand->FingerAtXYZ[2] = 3.5524f;
	    FrontOffset(MovingHand->FingerAtXYZ,MovingHand);
	}

	// Rotate the  operate bar when it is being pressed
	if(MovingHand->FingerAtXYZ[1] < 1.97f)
	{
	    activeBeta = 3.3333333f * (1.97f - MovingHand->FingerAtXYZ[1]);
	}
	else
	{
	    activeBeta = 0.0f;
	}
	MovingHand->operateAngle = activeBeta;
	operateBarSound(activeBeta,time);
    }
}

static void WgFrontLeftOuterMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,
					  __attribute__((unused)) guint time)
{
    glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
    FrontOffset2(MovingHand);

}

static void WgFrontRightInnerMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time)
{
    float activeBeta;
    
    glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
    FrontOffset2(MovingHand);

    if(MovingHand->LeftHand)
    {
	if(MovingHand->FingerAtXYZ[1] < 1.4f)
	{
	    // Because the curosr is off the surface the X,Y,Z values
	    // need to be reset
	    MovingHand->FingerAtXYZ[0] = MovingHand->PreviousFingerAtXYZ[0]; 
	    MovingHand->FingerAtXYZ[1] = 1.82f;
	    MovingHand->FingerAtXYZ[2] = 3.5524f;
	    FrontOffset2(MovingHand);
	    warpMouseToXYZ(MovingHand->FingerAtXYZ);
	}
	
	// Limit hands motion when operate bar motion limit is reached.
	if(MovingHand->FingerAtXYZ[1] < 1.82f)
	{
	    MovingHand->FingerAtXYZ[1] = 1.82f;
	    MovingHand->FingerAtXYZ[2] = 3.5524f;
	    FrontOffset(MovingHand->FingerAtXYZ,MovingHand);
	}

	// Rotate the  operate bar when it is being pressed
	if(MovingHand->FingerAtXYZ[1] < 1.97f)
	{
	    activeBeta = 3.3333333f * (1.97f - MovingHand->FingerAtXYZ[1]);
	}
	else
	{
	    activeBeta = 0.0f;
	}
	MovingHand->operateAngle = activeBeta;
	operateBarSound(activeBeta,time);
    }
}

static void WgFrontRightOuterMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,
					   __attribute__((unused)) guint time)
{
    glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
    FrontOffset2(MovingHand);
}



static void WgTopMotionHandler(vec4 PointerXYZ,HandInfo *MovingHand,guint time)
{
    WGButton *thisButton,*closestButton = NULL;
    float closest = 1000.0f;
    float distance;
    vec4 offset =  {0.0f,0.12f,0.0f,0.0f};
    static float volumeZ;

    thisButton = WGButtons;
    
    glm_vec4_copy(offset, MovingHand->SurfaceOffsetXYZ);
    
    while(thisButton->objectId != 0)
    {
	if( (thisButton->objectId != 999) && (thisButton->objectId != 70))
	{
	    distance = hypotf(thisButton->TranslateUp[0] - PointerXYZ[0],
			      thisButton->TranslateUp[2] - PointerXYZ[2]);
	    if(distance < closest)
	    {
		closest = distance;
		closestButton = thisButton;
	    }
	}
	thisButton++;
    }
    // Deal with the volume control
    if((closestButton != NULL) && (closestButton->objectId == 80))
    {
	switch(MovingHand->SnapState)
	{
	case 0:
	    if((fabsf(closestButton->TranslateUp[0] - PointerXYZ[0]) < 0.1f) &&
	       (fabsf(closestButton->TranslateUp[2] - PointerXYZ[2]) < 0.3f))
	    {
		// normal move finger to pointer
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 1;
		MovingHand->NearestButton = closestButton;
		volumeZ = PointerXYZ[2];
	    }
	    else
	    {
		// normal move finger to pointer
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);	
	    }
	    break;
	case 1:
	    if((fabsf(closestButton->TranslateUp[0] - PointerXYZ[0]) < 0.1f) &&
	       (fabsf(closestButton->TranslateUp[2] - PointerXYZ[2]) < 0.3f))
	    {
		if(MovingHand->fingersPressed != 0)
		{
		    ButtonEvent *be;
		    
		    volAngle += 2.0f * (PointerXYZ[2] - volumeZ) ;
		    volAngle = fminf(volAngle,0.93f);
		    volAngle = fmaxf(volAngle,-0.25f);

		    be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
		    be->press = TRUE;
		    be->rowId = VOLUME;
		    be->value = (guint)(2560.0f * ((volAngle + 0.25f)/1.18f));
		    be->time  = time;

		    g_async_queue_push(ButtonEventQueue,(gpointer) be);
		}

		volumeZ = PointerXYZ[2];
		// normal move finger to pointer
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
		
	    }
	    else
	    {
		// normal move finger to pointer
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 0;
		MovingHand->NearestButton = NULL;
	    }
	default:
	    break;
	}
    }
    
/*  Snapping to buttons :
0 : Not snapped
1 : Finger has approached a button and finger and pointer have 
    snapped to the middle of the button. Finger does not follow pointer
2:  Pointer has moved away and been warped back to the center.  Finger 
    now tracks pointer
*/
    if((closestButton != NULL) && (closestButton->objectId != 80))
    {
	// Needs refactoring !
	switch(MovingHand->SnapState)
	{
	case 0:   // Not snappped top button
	    if(closest < 0.1f)
	    {
		// Move finger and pointer to middle of snapped button.
		MovingHand->NearestButton = closestButton;
		glm_vec4_copy(closestButton->TranslateUp,MovingHand->FingerAtXYZ);
		warpMouseToXYZ( MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 1;
	    }
	    else
	    {
		// normal move finger to pointer
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);	
	    }
	    break;
	    
	case 1:
	    // Keep finger over snapped button
	    glm_vec4_copy(MovingHand->NearestButton->TranslateUp,MovingHand->FingerAtXYZ);
	    
	    if((closest >= 0.1f) || (MovingHand->NearestButton != closestButton))
	    {
		warpMouseToXYZ( MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 2;

		// Finger and Button up when snap 1 -> 2
		if(MovingHand->fingersPressed != 0)
		{
		    MovingHand->fingersPressed = 0;
		    if(MovingHand->NearestButton->handler != NULL)
		    {
			(MovingHand->NearestButton->handler)(MovingHand->NearestButton,FALSE,time); 
		    }
		}
	    }
	    break;	    

	case 2:
	    if((closest < 0.1f) && (MovingHand->NearestButton == closestButton))
	    {
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
	    }
	    else if((closest >= 0.1f) && (MovingHand->NearestButton == closestButton))
	    {
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 0;
	    }
	    else if((closest < 0.1f) && (MovingHand->NearestButton != closestButton))
	    {
		MovingHand->NearestButton = closestButton;
		glm_vec4_copy(closestButton->TranslateUp,MovingHand->FingerAtXYZ);
		warpMouseToXYZ( MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 1;
	    }
	    else
	    {
		glm_vec4_copy(PointerXYZ,MovingHand->FingerAtXYZ);
		MovingHand->SnapState = 0;
	    }
	    
	    if(MovingHand->SnapState == 0)
	    {
		// Finger and Button up when snap 2 -> 0
		if(MovingHand->fingersPressed != 0)
		{
		    MovingHand->fingersPressed = 0;
		    if(MovingHand->NearestButton->handler != NULL)
		    {
			(MovingHand->NearestButton->handler)(MovingHand->NearestButton,FALSE,time); 
		    }
		}
		MovingHand->NearestButton = NULL;
	    }
	    break;
	    
	default:
	    break;
	}
    }
}


// Use beta crossing 0.25 to generate operate bar press/release events
static void operateBarSound(float newBeta,guint time)
{
    static float oldBeta = 0.0;

    if((oldBeta <= 0.25f) && (newBeta > 0.25f))
    {
	OperateBarPressed(TRUE,time);
    }
	
    else if((oldBeta >= 0.25f) && (newBeta < 0.25f))
    {
	OperateBarPressed(FALSE,time);
    }
    oldBeta = newBeta;
}

static void dummyEnterhandler(__attribute__((unused)) enum WgZones FromZone,
			      __attribute__((unused)) HandInfo *MovingHand)
{
}

gboolean MOUSE_WARPED = FALSE;

void warpMouseToXYZ(vec4 XYZ)
{
    vec3 ActiveMouseXYZ;
    int ox,oy;
    GdkWindow *win;
    GtkAllocation allocation;
    GdkDisplay *display;
    GdkSeat *seat;

    if(DontWarp) return;
    
    display = gdk_display_get_default();
    seat = gdk_display_get_default_seat(display);
			
    win = gtk_widget_get_parent_window (eventBox);
    gdk_window_get_origin (win,&ox,&oy);
    gtk_widget_get_allocation(eventBox,&allocation);
			
    glm_project(XYZ,mvpMatrix,(vec4){0.0,0.0,(GLfloat)allocation.width,
		(GLfloat) allocation.height},ActiveMouseXYZ);
  
    ActiveMouseXYZ[0] = roundf(ActiveMouseXYZ[0]);
    ActiveMouseXYZ[1] = roundf(ActiveMouseXYZ[1]);
    ActiveMouseXYZ[2] = roundf(ActiveMouseXYZ[2]);

    g_debug("[% f,% f,% f]  (% f,% f,% f)\n", 
	   (double)XYZ[0],(double)XYZ[1],(double)XYZ[2],
	   (double) ActiveMouseXYZ[0],
	   (double)  allocation.height  - (double) ActiveMouseXYZ[1],
	   (double) ActiveMouseXYZ[2]);
    
    
    gdk_device_warp (gdk_seat_get_pointer(seat),
		     gdk_screen_get_default(),
		     ox + (gint) ActiveMouseXYZ[0],
		     oy + allocation.height - (gint) ActiveMouseXYZ[1]);

    MOUSE_WARPED = TRUE;
}



#define FACEDEBUG 0

enum WgZones XYZtoZone(vec4 XYZ)
{

   
#if FACEDEBUG
    printf("%s %s %s (%f,%f,%f) ",__FUNCTION__,WgZones[ActiveHand->InZone].ZoneName,
	   WgZones[InactiveHand->InZone].ZoneName,
	   (double)XYZ[0],(double)XYZ[1],(double)XYZ[2]);


    printf("Operate Bar Check ");
    if((XYZ[0]>=-1.08f) && (XYZ[0]<= 0.75f)) printf("X OK ");
    if((XYZ[1]>  1.37f) && (XYZ[1]<= 2.5047f)) printf("Y OK ");
    if((XYZ[2]>3.4201f) && (XYZ[2]<= 3.665f)) printf("Z OK ");
#endif
    
    // Check for operate bar
    if((XYZ[0]>=-1.08f) && (XYZ[0]<= 0.75f) &&
       (XYZ[1]>  1.37f) && (XYZ[1]<= 2.5047f) &&
       (XYZ[2]>3.4201f) && (XYZ[2]<= 3.665f) )
    {
#if FACEDEBUG
	printf("Over Operate Bar\n");
#endif
	return WG_OPERATE;
    }

#if FACEDEBUG
    printf("Front Face check ");
    if((XYZ[0]>=-4.00f) && (XYZ[0]<= 3.64f)) printf("X OK ");
    if((XYZ[1]>  1.37f) && (XYZ[1]<= 2.5047f)) printf("Y OK ");
    if((XYZ[2]>3.4201f) && (XYZ[2]<= 3.665f)) printf("Z OK ");
#endif

    // Check for wordgenerator front face
    if((XYZ[0]>=-4.00f) && (XYZ[0]<= 3.64f) &&
       (XYZ[1]>  1.37f) && (XYZ[1]<= 2.5047f) &&
       (XYZ[2]>3.4201f) && (XYZ[2]<= 3.665f) )
    {
#if FACEDEBUG
	printf("Over Front Face\n");
#endif
	if(XYZ[0] < 0.0f)
	    if(XYZ[0]<-2.2f)
		return  WG_FRONT_LEFT_OUTER;
	    else
		return  WG_FRONT_LEFT_INNER;
	else
	    if(XYZ[0]> 1.82f)
		return WG_FRONT_RIGHT_OUTER;
	    else
		return WG_FRONT_RIGHT_INNER;
    }
    
#if FACEDEBUG	    
    printf("Top Face check ");
    if((XYZ[0]>=-4.00f) && (XYZ[0]<= 3.64f)) printf("X OK ");
    if((XYZ[1]> 2.202f) && (XYZ[1]<= 2.88f)) printf("Y OK ");
    if((XYZ[2]>-0.326f) && (XYZ[2]<= 3.4201f)) printf("Z OK ");
#endif
    //2.2047
    // Check for wordgenerator top face
    if((XYZ[0]>=-4.00f) && (XYZ[0]<= 3.64f) &&
       (XYZ[1]>2.202f) && (XYZ[1]<= 2.88f) &&
       (XYZ[2]>-0.326f) && (XYZ[2]<= 3.4201f) )
    {
#if FACEDEBUG
	printf(" Over Top Face\n");
#endif
	return WG_TOP;
    }

 #if FACEDEBUG
	printf(" Off Piste\n");
#endif
	return OFF_PISTE;   
}

/*
Called in the worker thread, so put events on the queue to turn 
the console light on and off.
*/

static gboolean MainsOn = FALSE;
static gboolean PowerOn = FALSE;

static void lampOn(gboolean on)
{
    LampsEvent *le;

    le = (LampsEvent *) malloc(sizeof(LampsEvent));
    le->lampId = 0;
    le->on = on;

    g_async_queue_push(LampsEventQueue,(gpointer) le);
}

static void mainsOn(__attribute__((unused)) unsigned int dummy)
{
    MainsOn = TRUE;
    if(MainsOn && PowerOn)
    {
	lampOn(TRUE);
    }    
}

static void mainsOff(__attribute__((unused)) unsigned int dummy)
{
    MainsOn = FALSE;
    lampOn(FALSE);
}

static void powerOn(__attribute__((unused)) unsigned int dummy)
{
    PowerOn = TRUE;
    if(MainsOn && PowerOn)
    {
	lampOn(TRUE);
    }   
}

static void powerOff(__attribute__((unused)) unsigned int dummy)
{
    PowerOn = FALSE;
    lampOn(FALSE);
}

/****************************************************************/


static int savedButtonStateHandler(int nn)
{
    WGButton *theButton;
    int id,state,fsmState;
    
    theButton = WGButtons;

    id = atoi(getField(nn+1));
    state = atoi(getField(nn+2));
    fsmState = atoi(getField(nn+3));
    
    while(theButton->objectId != 0)
    {
	if(theButton->objectId == id)
	{
	    theButton->state = state;
	    theButton->fsmState = fsmState;
	    return TRUE;
	}
	theButton++;
    }
    return FALSE;
}

static int savedVolumeAngleHandler(int nn)
{
    volAngle = strtof(getField(nn+1),NULL);
    return TRUE;

}

static Token savedStateTokens[] = {
    {"ButtonState",0,savedButtonStateHandler},
    {"VolumeAngle",0,savedVolumeAngleHandler},
    {NULL,0,NULL}
};

gboolean KeyboardInit(  GString *sharedPath,
			__attribute__((unused)) GString *userPath)
{
    vec4 offset =  {0.0f,0.12f,0.0f,0.0f};
    WGButton *buttonp;
    ButtonEvent *be;
    GString *directory;

    directory = g_string_new(sharedPath->str);
    g_string_append(directory,"objects/");

    KeyboardObjectList = loadObjectsFromFile(directory,"Feb03.14.obj",keyboardObjects);

    g_string_free(directory,TRUE);
    
    if(KeyboardObjectList  == NULL)
	return FALSE;

    // Should be guarded !
    OperateBar = WGButtons;
    while(OperateBar->objectId != 70) OperateBar++;

    readConfigFile("KeyboardState",userPath,savedStateTokens);

    // Send button events to set CPU to restored state.
    buttonp =  WGButtons;
    
    while(buttonp->objectId != 0)
    {
	be = (ButtonEvent *) malloc(sizeof(ButtonEvent));
	be->press = buttonp->state;
	be->rowId = buttonp->rowId;
	be->value = buttonp->value;
	be->time  = 0;

	g_async_queue_push(ButtonEventQueue,(gpointer) be);

	buttonp++;
    }

    glm_vec4_copy(offset, LeftHandInfo.SurfaceOffsetXYZ);
    glm_vec4_copy(offset,RightHandInfo.SurfaceOffsetXYZ);

    // These will get called in the worker thread
    connectWires(MAINS_SUPPLY_ON,mainsOn);
    connectWires(MAINS_SUPPLY_OFF,mainsOff);
    connectWires(SUPPLIES_ON, powerOn);
    connectWires(SUPPLIES_OFF,powerOff);
    
    return TRUE;
}

void KeyboardTidy(GString *userPath)
{
    GString *configText;
    WGButton *theButton;
    
    configText = g_string_new("# Keyboard configuration\n");

    theButton = WGButtons;

    while(theButton->objectId != 0)
    {
	if(theButton->objectId < 70)
	    g_string_append_printf(configText,"ButtonState %d %d %d\n",
				   theButton->objectId,
				   theButton->state,
				   theButton->fsmState);
	theButton++;
    }

    g_string_append_printf(configText,"VolumeAngle %f\n",(double) volAngle);
    
    updateConfigFile("KeyboardState",userPath,configText);

    g_string_free(configText,TRUE);  
}

GLfloat XwiresVertices[] = { 
    -0.1f,  0.0f,  0.0f, 
    0.1f,  0.0f,  0.0f,
    0.0f,  0.0f, -0.1f,
    0.0f,  0.0f,  0.1f,
    0.0f, -0.1f,  0.0f,
    0.0f,  0.1f,  0.0f
};

// DM160 light patches
GLfloat vVertices[] =
{

-0.305415f,2.711687f,0.288979f,
-0.307402f,2.683376f,0.446410f,


-0.056787f,2.708329f,0.307582f,
-0.048931f,2.683191f,0.449272f,

-0.281594f,2.632130f,0.731724f,
-0.290847f,2.607686f,0.869499f,

-0.051075f,2.630181f,0.743671f,
-0.052604f,2.607849f,0.869992f,

-0.295464f,2.555800f,1.160420f,
-0.300911f,2.528953f,1.311327f,

-0.062565f,2.550484f,1.188460f,
-0.056688f,2.528662f,1.312755f,

-0.295024f,2.477106f,1.602002f,
-0.295310f,2.450330f,1.747627f,

-0.042326f,2.473203f,1.621135f,
-0.050740f,2.449604f,1.750833f,

-0.296046f,2.397267f,2.045638f,
-0.302432f,2.372996f,2.182527f,

-0.059111f,2.395382f,2.058261f,
-0.054695f,2.372032f,2.187047f,

-0.290178f,2.319284f,2.485093f,
-0.287755f,2.295028f,2.619241f,

-0.064502f,2.316206f,2.497028f,
-0.069560f,2.294224f,2.625156f,

};


vec4 DM160GREEN = {0.0f,1.0f,0.7f,1.0f};
vec4 DM160bright;

unsigned int lamps = 0;
gfloat lampsBright[7];

#define DRAW_OPER 0

// Draw console top surface into a frame buffer.  This is called when motion stops (when last
// key is released.  This only holds the depth component so that the mouse curso can be
// unprojected to produce the Xwires and hand motion.
void DrawKeyboardForDepthTracking(GtkAllocation *allocation)
{
    OBJECT *currentObject = NULL;
    ELEMENTS *currentElements = NULL;
    GLfloat ButtonTranslate[4] = {0.0f,0.0f,0.0f,0.0f};
    //GLenum e;
    int index;
    vec4 WGCameraPosition;


    glm_vec4_copy(UserXYZ,WGCameraPosition);

#if DRAW_OPER
    // Note that operate bar is index = 2
    OBJECT *objects[] = {ConsoleFrontObject,ConsoleTopObject,*OperateBar->object,NULL};
#else
    OBJECT *objects[] = {ConsoleFrontObject,ConsoleTopObject,NULL};
#endif
    	
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    
    glViewport (0, 0,allocation->width , allocation->height);

    glClear(GL_DEPTH_BUFFER_BIT);
  
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glUseProgram(WGProg);
    glUniform1i(WGRotFlagLoc,(GLint) 0);
    
    for(index = 0; objects[index] != NULL; index++)
    {
	currentObject = objects[index];

	for(GList *ele = currentObject->ElementsList; ele != NULL; ele = ele->next)
	{
	    currentElements = (ELEMENTS *) ele->data;
	    // Load the vertex data
	    glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0,
				  currentElements->elementVertices);

	    CHECK("glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, \
				      currentElements->elementVertices);");
	
	    glEnableVertexAttribArray(WGPositionLoc);
	    CHECK("	glEnableVertexAttribArray(WGPositionLoc);");

	    glVertexAttribPointer (WGNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );
	    CHECK(" glVertexAttribPointer (WGNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );");  

	    glEnableVertexAttribArray (WGNormalsLoc );

	    CHECK(" glEnableVertexAttribArray (WGNormalsLoc );");  

	    // Load the MVP matrix
	    glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);
	    CHECK("glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);");  
	    glUniform4f(WGColourLoc ,
			currentElements->Material->KdR,
			currentElements->Material->KdG,
			currentElements->Material->KdB,
			1.0);

	    glUniform4fv(WGLightPositionLoc , 1,  (GLfloat *) &WGLightPosition[0]);
	    glUniform4fv(WGCameraPositionLoc , 1,  WGCameraPosition);

#if DRAW_OPER
	    // Need to translate the operate bar into its correct position
	    if(index != 2)
	    {
		glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) &ButtonTranslate[0]);
	    }
	    else
	    {
		glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) OperateBar->TranslateUp);
	    }
#else
	    glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) &ButtonTranslate[0]);
#endif	    
	    glDrawRangeElements ( GL_TRIANGLES,
			     0, currentElements->nextElement -1 ,
			     currentElements->nextIndex , GL_UNSIGNED_SHORT,
			     currentElements->elementIndices );
	    CHECK("glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT, \
				 currentElements->elementIndices );");
	
	}
    }
    glFlush();
    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

extern struct pngData *paperMaterial;
void DrawKeyboard(void)
{

    GList *Objects = NULL;
    OBJECT *currentObject = NULL;
    ELEMENTS *currentElements = NULL;
    gboolean programNotSet = TRUE;
    gboolean usingWGProg;
    mat4 VolumeRotate;
    vec4 WGCameraPosition;

    glm_vec4_copy(UserXYZ,WGCameraPosition);
    
    static vec4 VolumeTranslate = {2.7384f,2.3437f,1.1288f,0.0f};
    // Draw WG buttons
    glUseProgram(WGProg);
#if 0
    // Move the light across the scene for lighting testing
    {
	static float theta = 0.0;

	theta += 0.1;
	WGLightPosition[0] = 10.0f * sin(theta);
	WGLightPosition[2] = 10.0f * cos(theta);
	//if(WGLightPosition[0] > 5.0f) WGLightPosition[0] = -5.0f;
    }
#endif    
    glUniform1i(WGRotFlagLoc,(GLint) 0);
   
    
    currentElements = NULL;
    for(WGButton *currentButton = WGButtons; currentButton->objectId != 0; currentButton++)
    {
	if(currentButton->objectId == 80) continue;
	
	// Check for the operatebar
	if((currentButton->objectId == 70)) 
	{
	    float angle;
	    mat4 OperRotate;

	    angle = fmaxf(LeftHandInfo.operateAngle,RightHandInfo.operateAngle);
	    operateTopEdgeY = -((angle / 3.3333333f) - 1.97f);
	    glm_rotate_x(GLM_MAT4_IDENTITY,angle,OperRotate);
	    
	    glUniform1i(WGRotFlagLoc,(GLint) 1);
	    glUniformMatrix4fv( WGRotateLoc, 1, GL_FALSE, (GLfloat*) OperRotate );
	}
	else
	{
	    glUniform1i(WGRotFlagLoc,(GLint) 0);
	}
	
	currentObject = *currentButton->object;

	for(GList *ele = currentObject->ElementsList; ele != NULL; ele = ele->next)
	{
	    ELEMENTS *nextElements = (ELEMENTS *) ele->data;
	    if( (nextElements != currentElements) || (currentElements == NULL))
	    {
		currentElements = nextElements;
		// Load the vertex data
		glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0,
				      currentElements->elementVertices);

		CHECK("glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, \
				      currentElements->elementVertices);");
	
		glEnableVertexAttribArray(WGPositionLoc);
		CHECK("	glEnableVertexAttribArray(WGPositionLoc);");

		glVertexAttribPointer (WGNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );
		glEnableVertexAttribArray (WGNormalsLoc );

		// Load the MVP matrix
		glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);
		CHECK("glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);");

		glUniform4f(WGColourLoc ,
			    currentElements->Material->KdR,
			    currentElements->Material->KdG,
			    currentElements->Material->KdB,
			    1.0);

		glUniform4fv(WGLightPositionLoc , 1,  (GLfloat *) &WGLightPosition[0]);
		glUniform4fv(WGCameraPositionLoc , 1,  WGCameraPosition);
	    }
	    
	    if(currentButton->state)
	    {
		glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) currentButton->TranslateDown );
	    }
	    else
	    {
		glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) currentButton->TranslateUp );
	    }
	    glDrawRangeElements ( GL_TRIANGLES,
			     0, currentElements->nextElement -1 ,
			     currentElements->nextIndex , GL_UNSIGNED_SHORT,
			     currentElements->elementIndices );
	    CHECK("glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT, \
				 currentElements->elementIndices );");
	}
    }

    // Draw the DM160s usingthe brightnesses from the emulation.
    glUseProgram(simpleProg);

    // Load the vertex data
    glVertexAttribPointer ( simplePositionLoc, 3, GL_FLOAT, GL_FALSE, 0, vVertices );
    glEnableVertexAttribArray ( simplePositionLoc );

    // Load the MVP matrix
    glUniformMatrix4fv(simpleMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);

    for(int n = 0; n < 6; n+=1)
    {
	glm_vec4_scale(DM160GREEN,lampsBright[n]/lampsBright[6],DM160bright);
	glUniform4fv(simpleColourLoc , 1,  (GLfloat *) &DM160bright[0]);
	glDrawArrays ( GL_TRIANGLE_STRIP, 4*n, 4 );
    }

    // Draw the Body of the console 
    
    for(Objects=KeyboardObjectList;Objects != NULL; Objects=Objects->next)
    {
	currentObject = (OBJECT *) Objects->data;
	if(currentObject->hidden) continue;

	for(GList *ele = currentObject->ElementsList; ele != NULL; ele = ele->next)
	{
	    currentElements = (ELEMENTS *) ele->data;

	    if(currentElements->Material->hasTexture == TRUE)
	    {
		if(programNotSet || usingWGProg)
		{
		    glUseProgram(textureProg);
		    CHECK("glUseProgram(textureProg);");

		    programNotSet = FALSE;
		    usingWGProg = FALSE; 
		}
		// Load the vertex data
		glVertexAttribPointer(texturePositionLoc, 3, GL_FLOAT, GL_FALSE, 0,
				      currentElements->elementVertices);
		glVertexAttribPointer(textureTexelCoordLoc, 2, GL_FLOAT, GL_FALSE, 0,
				      currentElements->elementTexels);
		
		glEnableVertexAttribArray(texturePositionLoc);
		glEnableVertexAttribArray(textureTexelCoordLoc);

		glActiveTexture(GL_TEXTURE0);
		CHECK("glActiveTexture(GL_TEXTURE0);");

		glBindTexture(GL_TEXTURE_2D,currentElements->Material->texture->textureId);
		CHECK("glBindTexture(GL_TEXTURE_2D,currentElements->Material->texture);");
	
	
		glUniform1i(textureTextureLoc,0);
		CHECK("glUniform1i(textureTextureLoc,0);");
	
		glUniformMatrix4fv(textureMvpLoc, 1, GL_FALSE, (GLfloat*) &mvpMatrix[0][0] );
		CHECK("glUniformMatrix4fv(textureMvpLoc, 1, GL_FALSE, (GLfloat*) &mvpMatrix[0][0] );");
		
		glDrawRangeElements ( GL_TRIANGLES,
				 0, currentElements->nextElement -1 ,
				 currentElements->nextIndex , GL_UNSIGNED_SHORT,
				 currentElements->elementIndices );
		CHECK("glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT, \
				 currentElements->elementIndices );");
	    }
	    else
	    {
		if(programNotSet || !usingWGProg)
		{
		    glUseProgram(WGProg);
		    CHECK("glUseProgram(WGProg);");
		    
		    programNotSet = FALSE;
		    usingWGProg = TRUE;
		}

		// Load the vertex data
		glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0,
				      currentElements->elementVertices);

		CHECK("glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, \
				      currentElements->elementVertices);");
	
		glEnableVertexAttribArray(WGPositionLoc);
		CHECK("	glEnableVertexAttribArray(WGPositionLoc);");

		glVertexAttribPointer (WGNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );
		glEnableVertexAttribArray (WGNormalsLoc );

		// Load the MVP matrix
		glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);
		CHECK("glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);");

		glUniform4f(WGColourLoc ,
			    currentElements->Material->KdR,
			    currentElements->Material->KdG,
			    currentElements->Material->KdB,
			    1.0);
		
		glUniform4fv(WGLightPositionLoc , 1,  (GLfloat *) &WGLightPosition[0]);
		glUniform4fv(WGCameraPositionLoc , 1,  WGCameraPosition);
		
		glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) GLM_VEC4_ZERO);

		glDrawRangeElements ( GL_TRIANGLES,
				 0, currentElements->nextElement -1 ,
				 currentElements->nextIndex , GL_UNSIGNED_SHORT,
				 currentElements->elementIndices );
		CHECK("glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT, \
				 currentElements->elementIndices );");
	
	    }
	}
    }

    glm_rotate_x(GLM_MAT4_IDENTITY,volAngle,VolumeRotate);
   
    // Draw the volume control
    currentObject = VolumeObject;
    glUseProgram(WGProg);
    for(GList *ele = currentObject->ElementsList; ele != NULL; ele = ele->next)
    {
	currentElements = (ELEMENTS *) ele->data;
	// Load the vertex data
	glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0,
			      currentElements->elementVertices);

	CHECK("glVertexAttribPointer(WGPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, \
				      currentElements->elementVertices);");

	glEnableVertexAttribArray(WGPositionLoc);
	CHECK("	glEnableVertexAttribArray(WGPositionLoc);");

	glVertexAttribPointer (WGNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );
	glEnableVertexAttribArray (WGNormalsLoc );

	// Load the MVP matrix
	glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);
	CHECK("glUniformMatrix4fv(WGMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);");

	glUniform1i(WGRotFlagLoc,(GLint) 1);
	glUniformMatrix4fv( WGRotateLoc, 1, GL_FALSE, (GLfloat*) &VolumeRotate[0] );

	glUniform4f(WGColourLoc ,
		    currentElements->Material->KdR,
		    currentElements->Material->KdG,
		    currentElements->Material->KdB,
		    1.0);

	glUniform4fv(WGLightPositionLoc , 1,  (GLfloat *) &WGLightPosition[0]);
	glUniform4fv(WGCameraPositionLoc , 1,  WGCameraPosition);
		
	glUniform4fv(WGTranslateLoc , 1,  (GLfloat *) &VolumeTranslate[0]);

	glDrawRangeElements ( GL_TRIANGLES,
			 0, currentElements->nextElement -1 ,
			 currentElements->nextIndex , GL_UNSIGNED_SHORT,
			 currentElements->elementIndices );
	CHECK("glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT, \
				 currentElements->elementIndices );");
    }
    glUniform1i(WGRotFlagLoc,(GLint) 0);
}

#define MIN_PROXIMITY 0.02f
#define DELTA 0.01f

void PointerOverKeyboard(vec4 PointerXYZ,__attribute__((unused)) vec4 MouseXY,guint time)
{
    enum WgZones newZone,previousZone;
    vec4 position;
    float minDistance,delta;

    // Save some hand state incase....
    glm_vec4_copy(ActiveHand->FingerAtXYZ ,ActiveHand->PreviousFingerAtXYZ);
    glm_vec4_copy(ActiveHand->PushedOffsetXYZ ,ActiveHand->PreviousPushedOffsetXYZ);
    glm_vec4_copy(ActiveHand->SurfaceOffsetXYZ ,ActiveHand->PreviousSurfaceOffsetXYZ);
    previousZone = ActiveHand->PreviousZone = ActiveHand->InZone;

    (WgZones[ActiveHand->InZone].motionHandlerActive)(PointerXYZ,ActiveHand,time);
    if(ActiveHand->useFinger)
	newZone = XYZtoZone(ActiveHand->FingerAtXYZ);
    else
	newZone = XYZtoZone(PointerXYZ);
    
    if(newZone != previousZone)
    {
	gboolean exitOk;

	printf("Exiting %s\n",WgZones[previousZone].ZoneName);
	exitOk = (WgZones[previousZone].exitHandlerActive)(newZone,ActiveHand,time);
	

	if(exitOk)
	{
	    ActiveHand->InZone= newZone;
	    printf("Entering %s\n",WgZones[ActiveHand->InZone].ZoneName);
	    (WgZones[ActiveHand->InZone].enterHandlerActive)(previousZone,ActiveHand);
	}
	else
	{
	    glm_vec4_copy(ActiveHand->PreviousFingerAtXYZ,ActiveHand->FingerAtXYZ);
	    glm_vec4_copy(ActiveHand->PreviousPushedOffsetXYZ,ActiveHand->PushedOffsetXYZ);
	    glm_vec4_copy(ActiveHand->PreviousSurfaceOffsetXYZ,ActiveHand->SurfaceOffsetXYZ);
	    ActiveHand->InZone = ActiveHand->PreviousZone;
	}
    }
    
    // Save some hand state incase....
    glm_vec4_copy(InactiveHand->FingerAtXYZ ,InactiveHand->PreviousFingerAtXYZ);
    glm_vec4_copy(InactiveHand->PushedOffsetXYZ ,InactiveHand->PreviousPushedOffsetXYZ);
    glm_vec4_copy(InactiveHand->SurfaceOffsetXYZ ,InactiveHand->PreviousSurfaceOffsetXYZ);
    previousZone = InactiveHand->PreviousZone = InactiveHand->InZone;

    glm_vec4_copy(ActiveHand->FingerAtXYZ,
		  ActiveHand->DrawAtXYZ);
    glm_vec4_addadd(ActiveHand->PushedOffsetXYZ,ActiveHand->SurfaceOffsetXYZ,
		    ActiveHand->DrawAtXYZ);

    glm_vec4_copy(InactiveHand->FingerAtXYZ,
		  InactiveHand->DrawAtXYZ);
    glm_vec4_addadd(InactiveHand->PushedOffsetXYZ,InactiveHand->SurfaceOffsetXYZ,
		    InactiveHand->DrawAtXYZ);
    
    transformHandOutlines();
    minDistance = HandCollisionDetect2();
    delta = minDistance - MIN_PROXIMITY;

    if(!InactiveHand->LeftHand)
    {
	delta = -delta;
    }

    if(InactiveHand->Pushed == NOT_PUSHED)
    {
	if(minDistance < MIN_PROXIMITY)
	{
	    gboolean overFront;
	    enum WgZones InZone;

	    // If the inactive hand is off the bottom of the front, use FingerAtXYZ (which will
	    // still be over the front face) to re-evaluate the zone so that it will be pushed
	    // up and over the operate bar.
	    if(InactiveHand->InZone == OFF_PISTE)
	    {
		InZone = XYZtoZone(InactiveHand->FingerAtXYZ);
	    }
	    else
	    {
		InZone = InactiveHand->InZone;
	    }
	    
	    overFront =
		(InZone == WG_FRONT_LEFT_INNER) ||
		(InZone == WG_FRONT_LEFT_OUTER) ||
		(InZone == WG_FRONT_RIGHT_INNER) ||
		(InZone == WG_FRONT_RIGHT_OUTER);

	    InactiveHand->PushedOffsetXYZ[0] += delta;
	    InactiveHand->Pushed = overFront ? PUSHED_XY : PUSHED_X;

	    InactiveHand->fingersPressed = 0;
	    if((InactiveHand->NearestButton != NULL) && (InactiveHand->NearestButton->handler != NULL))
	    {
		(InactiveHand->NearestButton->handler)(InactiveHand->NearestButton,FALSE,time); 
	    }
	}
    }
    else
    {
	if(minDistance < (MIN_PROXIMITY-DELTA))
	{
	    InactiveHand->PushedOffsetXYZ[0] += delta;
	}

	if(minDistance > (MIN_PROXIMITY+DELTA))
	{
	    InactiveHand->PushedOffsetXYZ[0] += delta;
	    if( ((!InactiveHand->LeftHand) && (InactiveHand->PushedOffsetXYZ[0] < 0.0f)) ||
		(( InactiveHand->LeftHand) && (InactiveHand->PushedOffsetXYZ[0] > 0.0f))) 
	    {
		glm_vec4_zero(InactiveHand->PushedOffsetXYZ);
		InactiveHand->Pushed = NOT_PUSHED;
	    }
	}
    }
	
    if((InactiveHand->InZone != WG_TOP) && (InactiveHand->Pushed == PUSHED_XY))
    {
	vec4 t,s;
	glm_vec4_copy(InactiveHand->FingerAtXYZ,s);
	glm_vec4_add(InactiveHand->PushedOffsetXYZ,InactiveHand->FingerAtXYZ,t);
	FrontOffset(t,InactiveHand);
	glm_vec4_copy(s,InactiveHand->FingerAtXYZ);
	    
	PushedOffset(InactiveHand);
    }

    if(InactiveHand->Pushed != NOT_PUSHED)
    {
	glm_vec4_add(InactiveHand->FingerAtXYZ,InactiveHand->PushedOffsetXYZ,position);
	InactiveHand->InZone = newZone = XYZtoZone(position);

	if(newZone != previousZone)
	{
	    gboolean exitOk;

	    DontWarp = TRUE;

	    exitOk = (WgZones[previousZone].exitHandlerInactive)(newZone,InactiveHand,time);
	    if(exitOk)
	    {
		// exit handler may have changed InactiveHand->InZone
		(WgZones[InactiveHand->InZone].enterHandlerInactive)(previousZone,InactiveHand);
	    }
	    else
	    {
		glm_vec4_copy(InactiveHand->PreviousFingerAtXYZ,InactiveHand->FingerAtXYZ);
		glm_vec4_copy(InactiveHand->PreviousPushedOffsetXYZ,InactiveHand->PushedOffsetXYZ);
		glm_vec4_copy(InactiveHand->PreviousSurfaceOffsetXYZ,InactiveHand->SurfaceOffsetXYZ);
		InactiveHand->InZone = InactiveHand->PreviousZone;
		
	    }
	    DontWarp = FALSE;
	}
    }
}

void KeyboardTimerTick2(void)
{
    LampsEvent *le;
    
    if(LampsEventQueue != NULL)
    {	
	while((le = (LampsEvent *) g_async_queue_try_pop(LampsEventQueue)) != NULL)
	{
		if(le->lampId == 0)
		{
		    WGLampOnObject->hidden  = !le->on;
		    WGLampOffObject->hidden =  le->on;
		}
		else
		{
		    lampsBright[le->lampId - 1] = le->brightness;
		}
		free(le);
	    }
    }
}

