/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
gboolean KeyboardInit(  GString *sharedPath,
			GString *userPath);

void KeyboardTidy(GString *userPath);


void DrawKeyboard(void);

void DrawKeyboardForDepthTracking(GtkAllocation *allocation);
vec4 XwiresXYZ; 
void PointerOverKeyboard(vec4 PointerXYZ,vec4 MOuseAtXY,guint time);
void KeyboardTimerTick(void);
void KeyboardTimerTick2(void);
void warpMouseToXYZ(vec4 XYZ);


enum WgZones {OFF_PISTE=0,WG_TOP,
	      WG_FRONT_LEFT_INNER,WG_FRONT_LEFT_OUTER,
	      WG_FRONT_RIGHT_INNER,WG_FRONT_RIGHT_OUTER,
	      WG_OPERATE,LAST_ZONE};
enum WgZones XYZtoZone(vec4 XYZ);

OBJECT *WGLampOnObject;
OBJECT *WGLampOffObject;
OBJECT *RedButtonObject;
OBJECT *BlackButtonObject;
OBJECT *PowerOnButtonObject;
OBJECT *PowerOffButtonObject;
OBJECT *PowerShroudObject;
OBJECT *ShroudObject;
OBJECT *OperateBarObject;
OBJECT *VolumeObject;
OBJECT *ConsoleFrontObject,*ConsoleTopObject;
OBJECT *HandVertexCloudObject;
OBJECT *HandVertexLimitsObject;
