/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#include "WGbuttons.h"



enum HandTypes {HandNeutral=0,HandNeutralIndexDown,LastHand};

enum HandConstraints {HAND_NOT_CONSTRAINED=0,HAND_CONSTRAINED_BY_PRESS,HAND_CONSTRAINED_BY_OPERATE};


struct handImages
{
    GList *Objects;
    const char *filename;
};



gboolean HandsInit(  GString *sharedPath,
		     GString *userPath);
void DrawHands(void);

// Mostly taken from the 2D GTK3 version



enum DodgingStates{IDLE=0,DODGING_LEFT,DODGING_RIGHT,DODGING_BACK_LEFT,DODGING_BACK_RIGHT,RESTORING};

enum PushedFlags {NOT_PUSHED=0,PUSHED_X,PUSHED_XY};

const char *DodgingModeNames[6];

vec4 DodgingVectors[6];

typedef struct handinfo
{
    const char *handName;

    vec4 FingerAtXYZ;           // Where the finger is on the surface
    vec4 PushedOffsetXYZ;       // Offfset to Add to FingerAtXYZ to give pushed hand position
    vec4 SurfaceOffsetXYZ;      // Offfset to Add to pushed hand positio to give DrawAtXYZ
                                // Normally (0.0,0.12,0.0,0,0) on top surface

    vec4 DrawAtXYZ;          // Where the hand is actually draw.
    
    vec4 PreviousFingerAtXYZ;   // Where the finger was before it was moved.
    vec4 PreviousPushedOffsetXYZ;       // Offfset to Add to FingerAtXYZ to give pushed hand position
    vec4 PreviousSurfaceOffsetXYZ;
   
    vec4 WayPoint;
    GQueue *WayPoints;          // List of vec4 with waypoints
    gboolean WayPointActive;

    gboolean LeftHand;
    gboolean useFinger;
    enum PushedFlags Pushed;
    WGButton *NearestButton;
    gint fingersPressed;
    gboolean Trackable;
    enum WgZones InZone;
    enum WgZones PreviousZone;
    int SnapState;
    gfloat operateAngle;
} HandInfo;

HandInfo *Hands[2]; 
HandInfo *ActiveHand,*InactiveHand,LeftHandInfo,RightHandInfo;
HandInfo ResetLeft,ResetRight;
vec4 *HandOutlineLeft;
vec4 *HandOutlineRight;
void transformHandOutlines(void);
float HandCollisionDetect(enum DodgingStates *mode,vec4 difference);
float HandCollisionDetect2(void);
