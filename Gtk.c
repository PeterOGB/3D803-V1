/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
// gtk interface
#define _GNU_SOURCE
#define G_LOG_USE_STRUCTURED

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <EGL/egl.h>
#include <assert.h>

#include <cglm/cglm.h>

#include "Gtk.h"
#include "Gles.h"

#include "3D.h"
#include "ObjLoader.h"
#include "Keyboard.h"
#include "Hands.h"
#include "Wiring.h"
#include "Logging.h"
#include "Common.h"

extern void FrontOffset2(HandInfo *hand);

static GtkBuilder *builder = NULL;
static GError *err = NULL;

static GtkWidget *mainWindow;
GtkWidget *eventBox;
GdkCursor *blankCursor,*savedCursor;
GtkWidget *splashWindow;
GtkWidget *splashImage;


static EGLDisplay eglDisplay;
static EGLSurface eglSurface;
static EGLContext eglContext;
static int windowWidth,windowHeight;
static GString *shaderPath  = NULL;


static gboolean shiftPressed = FALSE;
static gboolean controlPressed = FALSE;
static GdkSeat *seat = NULL;
static GdkWindow *cursorWindow = NULL;
static gfloat grabedAtX,grabedAtY;
static gfloat pointerAtX,pointerAtY;
static gboolean justGrabbed = FALSE;
static gboolean navigating = FALSE;

static gdouble MouseX,MouseY;

/* Widget event handler prototypes */
gboolean
on_quitButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data);

gboolean
on_ResetViewButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data);

gboolean
on_MainsOnButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data);
gboolean
on_MainsOffButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data);


void on_eventBox_realize(__attribute__((unused)) GtkWidget *widget,
			 __attribute__((unused)) gpointer data);

gboolean
on_eventBox_draw(__attribute__((unused)) GtkWidget *eventBox2,
		 __attribute__((unused)) cairo_t *cr,
		 __attribute__((unused)) gpointer data);


gboolean
on_eventBox_motion_notify_event(__attribute__((unused)) GtkWidget *widget,
				GdkEventMotion  *event,
				__attribute__((unused)) gpointer   user_data);

gboolean on_eventBox_key_press_event(__attribute__((unused))GtkWidget *widget,
				       GdkEvent  *event,
				     __attribute__((unused))gpointer   user_data);

gboolean on_eventBox_key_release_event(__attribute__((unused))GtkWidget *widget,
				       GdkEvent  *event,
				     __attribute__((unused))gpointer   user_data);

gboolean
on_eventBox_button_press_event(__attribute__((unused)) GtkWidget *widget,
			       __attribute__((unused)) GdkEventButton  *event,
			       __attribute__((unused)) gpointer   user_data);

gboolean
on_eventBox_button_release_event(__attribute__((unused)) GtkWidget *widget,
			       __attribute__((unused)) GdkEventButton  *event,
				 __attribute__((unused)) gpointer   user_data);

gboolean
on_eventBox_enter_notify_event(__attribute__((unused)) GtkWidget *widget,
                               __attribute__((unused)) GdkEvent  *event,
                               __attribute__((unused)) gpointer   user_data);


gboolean
on_ResetViewButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data)
{
    GdkEventKey ek;
    GdkDisplay *display;
    GtkAllocation allocation;

    // Reposition camera.
    ResetScene();

    // Reset hand states and positions
    LeftHandInfo = ResetLeft;
    RightHandInfo = ResetRight;

    // Reset the MVP
    gtk_widget_get_allocation(eventBox,&allocation);
    UpdateMVP(allocation.width,allocation.height);

    // simulate the end of tracking by calling
    // on_eventBox_key_release_event to reset everything
    display = gdk_display_get_default();
    seat = gdk_display_get_default_seat(display);
	
    ek.keyval = 0;
    shiftPressed = FALSE;
    controlPressed = FALSE;

    on_eventBox_key_release_event(eventBox,(GdkEvent *) &ek,NULL);
    
    // Turn the cursor off
    gdk_window_set_cursor(gtk_widget_get_window(eventBox),blankCursor);
    
    return GDK_EVENT_PROPAGATE ;
}


// These two are needed until there is a mains switch in the 3d space.
gboolean
on_MainsOnButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data)
{

    wiring(MAINS_SUPPLY_ON,0);
    return GDK_EVENT_PROPAGATE ;
}

gboolean
on_MainsOffButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data)
{
    wiring(MAINS_SUPPLY_OFF,0);
    return GDK_EVENT_PROPAGATE ;
}


gboolean
on_quitButton_clicked (__attribute__((unused)) GtkWidget *widget,
		       __attribute__((unused)) GdkEvent  *event,
		       __attribute__((unused)) gpointer   user_data)
{
    gtk_main_quit();
    return FALSE;
}

// Seems an event box doesn't automatically get focus when the mouse enters it, so
// do it explicitly.
gboolean
on_eventBox_enter_notify_event(__attribute__((unused)) GtkWidget *widget,
                               __attribute__((unused)) GdkEvent  *event,
                               __attribute__((unused)) gpointer   user_data)
{

    //printf("%s called\n",__FUNCTION__);
    gtk_widget_grab_focus(widget);
    return FALSE;

}


extern gboolean MOUSE_WARPED;

gboolean
on_eventBox_motion_notify_event(__attribute__((unused)) GtkWidget *widget,
				GdkEventMotion  *event,
				__attribute__((unused)) gpointer   user_data)
{
    GdkEventMotion *em;
    static gfloat lastx,lasty;
    static gboolean yTopWarped = FALSE;
    static gboolean yBottomWarped = FALSE;
    static gboolean xLeftWarped = FALSE;
    static gboolean xRightWarped = FALSE;
    gfloat mouseMoveLR,mouseMoveUD;
    gfloat X_root,Y_root;
    GtkAllocation allocation;
    static gfloat pointerx,pointery;
    
    
    em = (GdkEventMotion *) event;

    // For mouse position debugging only
    MouseX = em->x;
    MouseY = em->y;
    
    X_root = (gfloat) em->x_root;
    Y_root = (gfloat) em->y_root;

    pointerAtX = X_root;
    pointerAtY = Y_root;


    // Sometimes after warping the mouse there is still an event in the queue after the
    // one that caused the warp and before the event cause by the warp.  A simple solution is to ignore
    // first event after a warp.
    if(MOUSE_WARPED)
    {
	//printf("****  MOTION AFTER WARP (%f,%f) ******\n",MouseX,MouseY);
	MOUSE_WARPED = FALSE;
	return GDK_EVENT_STOP ;
    }
     
    if((pointerx == pointerAtX) && (pointery == pointerAtY))
    {
	//printf("%s NO MOTION\n",__FUNCTION__);
	return GDK_EVENT_STOP ;
    }

    pointerx = pointerAtX;
    pointery = pointerAtY;
    
    gtk_widget_get_allocation(widget,&allocation);
    
    if(justGrabbed)
    {
	lastx = X_root;
	lasty = Y_root;

	justGrabbed = FALSE;
	navigating = TRUE;
    }
    else
    {
	if(navigating)
	{
	    // When navigating (rather than moving a hand) when the mouse reaches an edge
	    // of the event box it is warped back to the center.  THis is unseen by the
	    // user as the mouse cursor is hidden.

	    if(!yTopWarped && !yBottomWarped && !xLeftWarped && !xRightWarped)
	    {
		mouseMoveLR = X_root - lastx;
		mouseMoveUD = Y_root - lasty;
		lastx = X_root;
		lasty = Y_root;

		UpdateUser(mouseMoveLR,mouseMoveUD,shiftPressed,controlPressed);
	    }

	    if(em->x < 0.0)
	    {
		if(!xLeftWarped)
		{
		    X_root +=  (gfloat)allocation.width * 0.5f;
		    gdk_device_warp (em->device,
				     gdk_screen_get_default(),
				     (gint) X_root,
				     (gint) Y_root);

		    lastx = X_root;
		    xLeftWarped = TRUE;
		}
	    }
	    else
		xLeftWarped = FALSE;
	    
	    if(em->x > (gdouble)allocation.width)
	    {
		if(!xRightWarped)
		{
		    X_root -=  (gfloat)allocation.width * 0.5f;
		    gdk_device_warp (em->device,
				     gdk_screen_get_default(),
				     (gint) X_root,
				     (gint) Y_root);
		    lastx = X_root;
		    xRightWarped = TRUE;
		}
			
	    }
	    else
		xRightWarped = FALSE;

	    if(em->y < 0.0)
	    {
		if(!yTopWarped)
		{
		    Y_root +=  (float)allocation.height * 0.5f;
		    gdk_device_warp (em->device,
				     gdk_screen_get_default(),
				     (gint) X_root,
				     (gint) Y_root);

		    lasty = Y_root;
		    yTopWarped = TRUE;
		}
	    }
	    else
		yTopWarped = FALSE;
    
	    if(em->y > (gdouble)allocation.height)
	    {
		if(!yBottomWarped)
		{
		    Y_root -=  (gfloat)allocation.height * 0.5f;
	    
		    gdk_device_warp (em->device,
				     gdk_screen_get_default(),
				     (gint) X_root,
				     (gint) Y_root);

		    lasty = Y_root;
		    yBottomWarped = TRUE;
		}
	    }
	    else
		yBottomWarped = FALSE;
	}
	else 
	{  // Following a hand
	  
	    GLuint depth;
	    GLfloat z,y,x;
	    vec4 MouseXY =  {(float)em->x,(float)em->y,0.0f,0.0f};
	    
	    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

	    depth = getDepth(&allocation,(GLint)em->x,(GLint)(allocation.height-em->y));
	    
	    // Unproject the mouse coordinates and the depth 
	    z = (GLfloat) (depth / 4294967296.0);
	    x = (GLfloat) em->x;
	    y = (GLfloat) (allocation.height - em->y);

	    glm_unprojecti((vec3){x,y,z},mvpMatrixInv,(vec4){0.0,0.0,(GLfloat)allocation.width,
						 (GLfloat) allocation.height},XwiresXYZ);

	    // Tell relevent device about the cursor and mouse location.
	    // Only one for now
	    PointerOverKeyboard(XwiresXYZ,MouseXY,event->time);
	}
    }
        
    return  GDK_EVENT_STOP ;
}

gboolean on_eventBox_key_press_event(__attribute__((unused))GtkWidget *widget,
				       GdkEvent  *event,
				       __attribute__((unused))gpointer   user_data)
{
    GdkEventKey *ek;
    gboolean grab = FALSE;
    ek = (GdkEventKey *)event;
    
    switch(ek->keyval)
    {
    case GDK_KEY_Shift_L: 
	//printf("Pressed GDK_KEY_Shift_L\n");
	grab = shiftPressed = TRUE;
	break;
	
    case GDK_KEY_Control_L: 
	//printf("Pressed GDK_KEY_Control_L\n");
	grab = controlPressed = TRUE;
	break;

    case GDK_KEY_q:
	gtk_main_quit();
	break;


    case GDK_KEY_z:
	if((!shiftPressed) && (!controlPressed))
	{
	    HandInfo *tmp;
	    enum WgZones zone;

	    // Don't swap hands if
	    // Inactive hand has been pushed off the edge or
	    // Inactive hand is off screnn
	
	    if(( !((InactiveHand->InZone == OFF_PISTE) &&
		   (InactiveHand->Pushed != NOT_PUSHED)) ) &&
	       (InactiveHand->Trackable)) 
	    {
		if(InactiveHand->Pushed != NOT_PUSHED)
		{
		    glm_vec4_add(InactiveHand->PushedOffsetXYZ,
				 InactiveHand->FingerAtXYZ,
				 InactiveHand->FingerAtXYZ);
		    glm_vec4_zero(InactiveHand->PushedOffsetXYZ);
		    InactiveHand->Pushed = NOT_PUSHED;
		    zone = XYZtoZone(InactiveHand->FingerAtXYZ);
		    if( (( InactiveHand->LeftHand) && (zone == WG_FRONT_RIGHT_INNER)) ||
			((!InactiveHand->LeftHand) && (zone == WG_FRONT_LEFT_INNER )) )
			zone = WG_OPERATE;

		    InactiveHand->InZone = zone;
		}
		warpMouseToXYZ(InactiveHand->FingerAtXYZ);	    
	    
		tmp = ActiveHand;
		ActiveHand = InactiveHand;
		InactiveHand = tmp;

		InactiveHand->SnapState = 0;
	    }
	}
	break;
	
    default:
	break;
    }

    if(grab && !seat)
    {
	GdkDisplay *display;
    
	GdkWindow *window;

	window = gtk_widget_get_window(eventBox);
    
	display = gdk_display_get_default();
	seat = gdk_display_get_default_seat(display);

	grabedAtX = pointerAtX; // = eb->x_root;
	grabedAtY = pointerAtY; // = eb->y_root;
	justGrabbed =  TRUE;

	cursorWindow = window;
	gdk_seat_grab (seat,
		       window,
		       GDK_SEAT_CAPABILITY_POINTER|GDK_SEAT_CAPABILITY_KEYBOARD, 
		       FALSE, //gboolean owner_events,
		       NULL, //GdkCursor *cursor,
		       NULL, //const GdkEvent *event,
		       NULL, //GdkSeatGrabPrepareFunc prepare_func,
		       NULL //gpointer prepare_func_data);
	    );
    }
    return GDK_EVENT_PROPAGATE ;
}


gboolean on_eventBox_key_release_event(__attribute__((unused)) GtkWidget *widget,
				       GdkEvent  *event,
				       __attribute__((unused)) gpointer   user_data)
{
    GdkEventKey *ek;
    
    ek = (GdkEventKey *)event;
    
    switch(ek->keyval)
    {
    case GDK_KEY_Shift_L: 
	//printf("Released GDK_KEY_Shift_L\n");
	shiftPressed = FALSE;
	break;
    case GDK_KEY_Control_L: 
	//printf("Released GDK_KEY_Control_L\n");
	controlPressed = FALSE;
	break;
    default:
	break;
    }

    if((!shiftPressed) && (!controlPressed))
    {
	GtkAllocation allocation;

	// Update the draawing used for getting depths.
	gtk_widget_get_allocation(widget,&allocation);
	
	eglMakeCurrent (eglDisplay, eglSurface, eglSurface, eglContext);
	
	DrawKeyboardForDepthTracking(&allocation);
    }

    if(!shiftPressed && !controlPressed && seat!=NULL)    // i.e. if(released) 
    {
	vec3 ActiveMouseXYZ,InactiveMouseXYZ;
	int ox,oy;
	GdkWindow *win;
	GtkAllocation allocation;
	int flags;
	HandInfo *tmp;
	
	gdk_seat_ungrab(seat);
	win = gtk_widget_get_parent_window (eventBox);
	gdk_window_get_origin (win,&ox,&oy);
	gtk_widget_get_allocation(eventBox,&allocation);

	// If the original Finger position is still on the screen, move the mouse pointer
	// back to the same XYZ position 

	glm_project( ActiveHand->FingerAtXYZ,mvpMatrix,(vec4){0.0,0.0,(GLfloat)allocation.width,
		    (GLfloat) allocation.height},ActiveMouseXYZ);
	// Round mouse to nearest integer values to stop mouse drift.
	ActiveMouseXYZ[0] = roundf(ActiveMouseXYZ[0]);
	ActiveMouseXYZ[1] = roundf(ActiveMouseXYZ[1]);
	ActiveMouseXYZ[2] = roundf(ActiveMouseXYZ[2]);

	glm_project( InactiveHand->FingerAtXYZ,mvpMatrix,(vec4){0.0,0.0,(GLfloat)allocation.width,
		    (GLfloat) allocation.height},InactiveMouseXYZ);
	// Round mouse to nearest integer values to stop mouse drift.
	InactiveMouseXYZ[0] = roundf(InactiveMouseXYZ[0]);
	InactiveMouseXYZ[1] = roundf(InactiveMouseXYZ[1]);
	InactiveMouseXYZ[2] = roundf(InactiveMouseXYZ[2]);

	flags = 0;

	if((ActiveMouseXYZ[0] >= 0.0f) && (ActiveMouseXYZ[0] <= (GLfloat)allocation.width) &&
	   (ActiveMouseXYZ[1] >= 0.0f) && (ActiveMouseXYZ[1] <= (GLfloat) allocation.height)) flags |= 1;

	if((InactiveMouseXYZ[0] >= 0.0f) && (InactiveMouseXYZ[0] <= (GLfloat)allocation.width) &&
	   (InactiveMouseXYZ[1] >= 0.0f) && (InactiveMouseXYZ[1] <= (GLfloat) allocation.height)) flags |= 2;


	g_debug("flags = %d",flags);
	
	switch(flags)
	{
	case 0:
	    ActiveHand->Trackable = FALSE;
	    InactiveHand->Trackable = FALSE;
	    // Put cursor in the middle of the window
	    gdk_device_warp (gdk_seat_get_pointer(seat),
			     gdk_screen_get_default(),
			     ox + (allocation.width / 2),
			     oy + (allocation.height / 2));
	    break;
	case 1:
	    ActiveHand->Trackable = TRUE;
	    InactiveHand->Trackable = FALSE;
	    // Put cursor at active hand
	    gdk_device_warp (gdk_seat_get_pointer(seat),
			     gdk_screen_get_default(),
			     ox + (gint) ActiveMouseXYZ[0],
			     oy + allocation.height - (gint) ActiveMouseXYZ[1]);
	    break;
	case 2:
	    tmp = ActiveHand;
	    ActiveHand = InactiveHand;
	    InactiveHand = tmp;
	    
	    ActiveHand->Trackable = TRUE;
	    InactiveHand->Trackable = FALSE;
	    // Put cursor at inactive hand
	    gdk_device_warp (gdk_seat_get_pointer(seat),
			     gdk_screen_get_default(),
			     ox + (gint) InactiveMouseXYZ[0],
			     oy + allocation.height - (gint) InactiveMouseXYZ[1]);

	    
	    break;
	case 3:
	    ActiveHand->Trackable = TRUE;
	    InactiveHand->Trackable = TRUE;
	    // Put cursor at active hand
	    gdk_device_warp (gdk_seat_get_pointer(seat),
			     gdk_screen_get_default(),
			     ox + (gint) ActiveMouseXYZ[0],
			     oy + allocation.height - (gint) ActiveMouseXYZ[1]);
	    break;
	}

	seat = NULL;
	justGrabbed = FALSE;
	navigating = FALSE;
    }
    return GDK_EVENT_PROPAGATE ;
}

static gboolean
on_eventBox_button_press_eventNew(__attribute__((unused)) GtkWidget *widget,
			       __attribute__((unused)) GdkEventButton  *event,
			       __attribute__((unused)) gpointer   user_data)
{
    enum WgZones zone;

    // Don't act on  any presses while navigating
    if((!shiftPressed) && (!controlPressed))
    {
	HandInfo *tmp;

	// New swapping method
	if(((event->button == 1) && ( InactiveHand->LeftHand) ) ||
	   ((event->button == 3) && ( ActiveHand->LeftHand)))

	{
	
	    // Don't swap hands if
	    // Inactive hand has been pushed off the edge or
	    // Inactive hand is off screnn

	    if(( !((InactiveHand->InZone == OFF_PISTE) && (InactiveHand->Pushed != NOT_PUSHED)) ) &&
	       (InactiveHand->Trackable)) 
	    {
		if(InactiveHand->Pushed != NOT_PUSHED)
		{
		    glm_vec4_add(InactiveHand->PushedOffsetXYZ,InactiveHand->FingerAtXYZ, InactiveHand->FingerAtXYZ);
		    glm_vec4_zero(InactiveHand->PushedOffsetXYZ);
		    InactiveHand->Pushed = NOT_PUSHED;
		    zone = XYZtoZone(InactiveHand->FingerAtXYZ);
		    if( (( InactiveHand->LeftHand) && (zone == WG_FRONT_RIGHT_INNER)) ||
			((!InactiveHand->LeftHand) && (zone == WG_FRONT_LEFT_INNER )) )
			zone = WG_OPERATE;

		    InactiveHand->InZone = zone;
		}
		warpMouseToXYZ(InactiveHand->FingerAtXYZ);	    
	    
		tmp = ActiveHand;
		ActiveHand = InactiveHand;
		InactiveHand = tmp;

		// Leave snap state as is so still snapped when swapped back
		//InactiveHand->SnapState = 0;
	    }
	    return GDK_EVENT_STOP;
	}
    }

    printf("ActiveHand->NearestButton=%p ActiveHand->SnapState=%d\n",
	   ActiveHand->NearestButton,ActiveHand->SnapState);
	   
    
    if( (ActiveHand->NearestButton != NULL) && (ActiveHand->SnapState != 0))
    {
	ActiveHand->fingersPressed = 1;
	if( (ActiveHand->NearestButton->objectId != 70) && (ActiveHand->NearestButton->handler != NULL)) 
	{
	    (ActiveHand->NearestButton->handler)(ActiveHand->NearestButton,event->type == GDK_BUTTON_PRESS,event->time);
	}
    }
    return GDK_EVENT_PROPAGATE ;
}


static gboolean
on_eventBox_button_release_eventNew(__attribute__((unused)) GtkWidget *widget,
			       __attribute__((unused)) GdkEventButton  *event,
			       __attribute__((unused)) gpointer   user_data)
{
    //if(event->button == 3) return  GDK_EVENT_PROPAGATE ;
    
    ActiveHand->fingersPressed = 0;
    if(ActiveHand->NearestButton != NULL)
    {
	if(ActiveHand->NearestButton->handler != NULL)
	{
	    (ActiveHand->NearestButton->handler)(ActiveHand->NearestButton,event->type == GDK_BUTTON_PRESS,event->time); 
	}
	warpMouseToXYZ(ActiveHand->FingerAtXYZ);	   
    }
    return  GDK_EVENT_PROPAGATE ;
}



static gboolean
on_eventBox_button_press_eventOld(__attribute__((unused)) GtkWidget *widget,
			       __attribute__((unused)) GdkEventButton  *event,
			       __attribute__((unused)) gpointer   user_data)
{
    enum WgZones zone;

    // Don't allow hand swap while navigating
    if((event->button == 3) && (!shiftPressed) && (!controlPressed))
    {
	HandInfo *tmp;

	// Don't swap hands if
	// Inactive hand has been pushed off the edge or
	// Inactive hand is off screnn

	if(( !((InactiveHand->InZone == OFF_PISTE) && (InactiveHand->Pushed != NOT_PUSHED)) ) &&
	   (InactiveHand->Trackable)) 
	{
	    if(InactiveHand->Pushed != NOT_PUSHED)
	    {
		glm_vec4_add(InactiveHand->PushedOffsetXYZ,InactiveHand->FingerAtXYZ, InactiveHand->FingerAtXYZ);
		glm_vec4_zero(InactiveHand->PushedOffsetXYZ);
		InactiveHand->Pushed = NOT_PUSHED;
		zone = XYZtoZone(InactiveHand->FingerAtXYZ);
		if( (( InactiveHand->LeftHand) && (zone == WG_FRONT_RIGHT_INNER)) ||
		    ((!InactiveHand->LeftHand) && (zone == WG_FRONT_LEFT_INNER )) )
		    zone = WG_OPERATE;

		InactiveHand->InZone = zone;
	    }
	    warpMouseToXYZ(InactiveHand->FingerAtXYZ);	    
	    
	    tmp = ActiveHand;
	    ActiveHand = InactiveHand;
	    InactiveHand = tmp;

	    // Leave snap state as is so still snapped when swapped back
	    //InactiveHand->SnapState = 0;
	}
        return GDK_EVENT_STOP;
    }

    
    if( (ActiveHand->NearestButton != NULL) && (ActiveHand->SnapState != 0))
    {
	ActiveHand->fingersPressed = 1;
	if( (ActiveHand->NearestButton->objectId != 70) && (ActiveHand->NearestButton->handler != NULL)) 
	{
	    (ActiveHand->NearestButton->handler)(ActiveHand->NearestButton,event->type == GDK_BUTTON_PRESS,event->time);
	}
    }
    return GDK_EVENT_PROPAGATE ;
}


static gboolean
on_eventBox_button_release_eventOld(__attribute__((unused)) GtkWidget *widget,
				    __attribute__((unused)) GdkEventButton  *event,
				    __attribute__((unused)) gpointer   user_data)
{
    if(event->button == 3) return  GDK_EVENT_PROPAGATE ;
    
    ActiveHand->fingersPressed = 0;
    if(ActiveHand->NearestButton != NULL)
    {
	if(ActiveHand->NearestButton->handler != NULL)
	{
	    (ActiveHand->NearestButton->handler)(ActiveHand->NearestButton,event->type == GDK_BUTTON_PRESS,event->time); 
	}
	warpMouseToXYZ(ActiveHand->FingerAtXYZ);	   
    }
    return  GDK_EVENT_PROPAGATE ;
}


/*
Old : Use right mouse click to swap active hand
      Use left mouse click to press buttons

New:  If left  hand active, left  click presses and right click swaps hands
      If Right hand active, right click presses and left  clcik swaps hands

 */





gboolean
on_eventBox_button_press_event(GtkWidget *widget,
			       GdkEventButton  *event,
			       gpointer   user_data)
{
    gboolean ret;
    if(oldHandSwap)
    {
	ret = on_eventBox_button_press_eventOld(widget,event,user_data);
    }
    else
    {
	ret = on_eventBox_button_press_eventNew(widget,event,user_data);
    }
    return ret;
}

gboolean
on_eventBox_button_release_event(__attribute__((unused)) GtkWidget *widget,
				 __attribute__((unused)) GdkEventButton  *event,
				 __attribute__((unused)) gpointer   user_data)
{
    gboolean ret;
    if(oldHandSwap)
    {
	ret = on_eventBox_button_release_eventOld(widget,event,user_data);
    }
    else
    {
	ret = on_eventBox_button_release_eventNew(widget,event,user_data);
    }
    return ret;
}


// Lots happens here !  EGL and OpenGLES are intiailised. 
void on_eventBox_realize(__attribute__((unused)) GtkWidget *widget,
				__attribute__((unused)) gpointer data)
{
    EGLBoolean result;
    EGLint num_config;
    GdkWindow *gdkWindow;
    Window xWindow;
    GtkAllocation allocation;

    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 8,
	    EGL_DEPTH_SIZE,16,
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_NONE
	};

    static const EGLint context_attributes[] = 
	{
	    EGL_CONTEXT_CLIENT_VERSION, 3,
	    EGL_NONE
	};
    EGLConfig config;
    
    // Recover the xwindows window id from the gtk widget
    gdkWindow = gtk_widget_get_window (widget);
    xWindow = gdk_x11_window_get_xid (gdkWindow);
    
    // get an EGL display connection
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(eglDisplay != EGL_NO_DISPLAY);

    // initialize the EGL display connection
    result = eglInitialize(eglDisplay, NULL, NULL);
    assert(EGL_FALSE != result);

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(eglDisplay, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);

    // create an EGL rendering context
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, context_attributes);
    assert(eglContext!=EGL_NO_CONTEXT);

    gtk_widget_get_allocation(widget,&allocation);
    windowWidth = allocation.width; 
    windowHeight = allocation.height;

    eglSurface = eglCreateWindowSurface( eglDisplay, config, xWindow, NULL );
    assert(eglSurface != EGL_NO_SURFACE);

    //eglSurfaceAttrib(eglDisplay,eglSurface,EGL_SWAP_BEHAVIOR,EGL_BUFFER_DESTROYED);
    // connect the context to the surface
    result = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    assert(EGL_FALSE != result);

    // Display update rate is set by timer callback.
    eglSwapInterval(eglDisplay,0);    // 1 = 60Hz   2 = 30Hz
    
    GlesInit(shaderPath,windowWidth,windowHeight);

    // Assume all blender files loaded so ...
    loadTextures2();

    UpdateMVP(allocation.width , allocation.height);
    DrawKeyboardForDepthTracking(&allocation);
}


gboolean
on_eventBox_draw(__attribute__((unused)) GtkWidget *eventBox2,
		 __attribute__((unused)) cairo_t *cr,
		 __attribute__((unused)) gpointer data)
{

    GtkAllocation allocation;
    
    eglMakeCurrent (eglDisplay, eglSurface, eglSurface, eglContext);
    gtk_widget_get_allocation(eventBox2,&allocation);
    
    UpdateMVP(allocation.width , allocation.height);
    GlesDraw(allocation.width , allocation.height);
    
    eglSwapBuffers(eglDisplay,eglSurface);
    
    return  GDK_EVENT_PROPAGATE ;
}

void UpdateScreen(void)
{
    gtk_widget_queue_draw(eventBox);
} 
    
static gboolean timerTick(__attribute__((unused)) gpointer user_data)
{
    KeyboardTimerTick2();
    UpdateScreen();
    
    return  G_SOURCE_CONTINUE;
}


// Helper 
static GObject *gtk_builder_get_object_CHECKed(GtkBuilder *_builder,const gchar *name)
{
    GObject *gotten;

    gotten = gtk_builder_get_object (_builder,name);
    if(gotten == NULL)
    {
	g_error("FAILED TO GET (%s)\n",name);
    }
    return gotten;
}
    

// Macro for geting widgets from the builder
#define GETWIDGET(name,type) name=type(gtk_builder_get_object_CHECKed (builder,#name))



gboolean GtkInit(GString *sharedPath,int *argc,char ***argv)
{
    GString *gladeFileName;


    shaderPath  = g_string_new(sharedPath->str);
    g_string_append(shaderPath,"shaders/");
    
    // Normal GTK application start up
    gtk_init (argc, argv);

    // Create a builder
    builder = gtk_builder_new();

    // And read gui definition from a file
    gladeFileName = g_string_new(sharedPath->str);
    g_string_append(gladeFileName,"generic900x900.glade");
    if( gtk_builder_add_from_file (builder,gladeFileName->str , &err) == 0)
    {
        g_info("gtk_builder_add_from_file (%s) FAILED (%s)\n",gladeFileName->str,err->message);
	g_string_free(gladeFileName,TRUE);
	return(FALSE);
    }

    g_string_free(gladeFileName,TRUE);
    
    GETWIDGET(mainWindow,GTK_WIDGET);
    GETWIDGET(eventBox,GTK_WIDGET);
    GETWIDGET(splashWindow,GTK_WIDGET);
    GETWIDGET(splashImage,GTK_WIDGET);


     {
	 GString *splashPath;

	 splashPath = g_string_new(sharedPath->str);
	 g_string_append(splashPath,"graphics/803splash.png");
	 gtk_image_set_from_file(GTK_IMAGE(splashImage),splashPath->str);

	 g_string_free(splashPath,TRUE);
     }


     
    
    return TRUE;
}

void showCursor(void)
{
    gdk_window_set_cursor(gtk_widget_get_window(eventBox),savedCursor);
}


void hideCursor(void)
{
    gdk_window_set_cursor(gtk_widget_get_window(eventBox),blankCursor);
}

static gboolean splashOff(__attribute__((unused)) gpointer user_data)
{
    gtk_widget_hide(splashWindow);
    return G_SOURCE_REMOVE;
    
}


gboolean GtkRun(int w,int h)
{
    
    gtk_builder_connect_signals(builder,NULL);

    gtk_window_set_default_size(GTK_WINDOW(mainWindow),w,h);
    gtk_window_set_resizable(GTK_WINDOW(mainWindow),FALSE); 
    
    gtk_widget_show_all (mainWindow);
    
    g_timeout_add(50,timerTick,NULL);
    
    blankCursor = gdk_cursor_new_from_name (gdk_display_get_default(),"none");
    savedCursor = gdk_window_get_cursor(gtk_widget_get_window(eventBox));

    hideCursor();


    gtk_widget_show(splashWindow);
    
    g_timeout_add(5000,splashOff,NULL);
    
    gtk_main ();
    
    return TRUE;
}
