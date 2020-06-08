/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <GLES3/gl3.h>
#include <cglm/cglm.h>
#include <pwd.h>

#include "config.h"
#include "Gtk.h"
#include "3D.h"
#include "ObjLoader.h"
#include "WGbuttons.h"
#include "Sound.h"
#include "Cpu.h"
#include "PowerCabinet.h"
#include "Charger.h"
#include "PTS.h"
#include "Parse.h"
#include "Common.h"
#include "Logging.h"
#include "Keyboard.h"
#include "Hands.h"

#include <glib.h>

// Paths to directories. 
static GString *sharedPath = NULL;
static GString *configPath = NULL;

static gchar *loadCoreFileName =  NULL;
static gchar *saveCoreFileName =  NULL;
static gchar *windowSize = NULL;
static gchar *alsaName = NULL;

gboolean oldHandSwap = FALSE;

// Command line options
static GOptionEntry entries[] =
{
    { "loadcorefile", 'l', 0, G_OPTION_ARG_FILENAME, &loadCoreFileName, "Load the core store from file.", NULL },
    { "savecorefile", 's', 0, G_OPTION_ARG_FILENAME, &saveCoreFileName, "Save the core store to file.", NULL },
    { "windowsize", 'w', 0, G_OPTION_ARG_STRING, &windowSize, "Window size as widthxheight.", NULL },
    { "handswap", 'h' , 0, G_OPTION_ARG_NONE, &oldHandSwap, "Use old (right click) hand swap method.",NULL },
    { "device", 'D' , 0,  G_OPTION_ARG_STRING, &alsaName, "Select ALSA output device.",NULL},
    { NULL }
};

extern int callCount;

GMutex SoundEffectsQueueMutex;
GMutex ButtonEventMutex;
GMutex LampsEventMutex;
gboolean Running = TRUE; 

int main(int argc,char **argv)
{
    struct passwd *pw;
    uid_t uid;
    gboolean createConfigDirectory = FALSE;
    GOptionContext *context;
    GError *error = NULL;
    GThread *EmulationThread = NULL;
    int windowWidth,windowHeight;
    gboolean windowSizeOk = FALSE;
    
    /* Set the locale according to environment variable */
    if (!setlocale(LC_CTYPE, ""))
    {
	fprintf(stderr, "Can't set the specified locale! "
		"Check LANG, LC_CTYPE, LC_ALL.\n");
	exit(EXIT_FAILURE);
    }
    // Install simple logging to stdout.
    LoggingInit(FALSE);


    /* Set global path to user's configuration and machine state files */
   
    uid = getuid();
    pw = getpwuid(uid);

    configPath = g_string_new(pw->pw_dir);
    configPath = g_string_append(configPath,"/.803-3D-Emulator/");

    // Now Check it exists.   If it is missing it is not an
    // error as it may be the first time this user has run the emulator.
    {
	GFile *gf = NULL;
	GFileType gft;
	GFileInfo *gfi = NULL;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(configPath->str);
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_warning("Could not read user configuration directory: %s\n", error2->message);
	    createConfigDirectory = TRUE;
	}
	else
	{
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_DIRECTORY)
	    {
		g_warning("User's configuration directory (%s) is missing.\n",configPath->str);
		createConfigDirectory = TRUE;
	    }
	}
	if(gfi) g_object_unref(gfi);
	if(gf) g_object_unref(gf);
    }

    // Find out where the executable is located.
    {
	GFile *gf;
	GFileType gft;
	GFileInfo *gfi;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path("/proc/self/exe");
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_error("Could not read /proc/self/exe: %s\n", error2->message);
	}
	else
	{
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_SYMBOLIC_LINK)
	    {
		const char *exename = g_file_info_get_symlink_target(gfi);
		g_info("exename is (%s)\n",exename);

		if(strcmp(PROJECT_DIR"/803",exename)==0)
		{
		    g_info("Running in the build tree, using local resources.\n");
		    /* Set global path to local icons, pictures and sound effect files */
		    sharedPath = g_string_new(PROJECT_DIR"/803-Resources/");		
		}
		else
		{
		    g_info("Running from installed file, using shared resources.\n");
		    /* Set global path to shared icons, pictures and sound effect files */
		    sharedPath = g_string_new("/usr/local/share/803-Resources/");
		}
	    }
	}
	g_object_unref(gfi);
	g_object_unref(gf);
    }

    // Check that the resources directory exists.
    {
	GFile *gf;
	GFileType gft;
	GFileInfo *gfi;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(sharedPath->str);
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_error("Could not read Resources directory:%s\n", error2->message);
	}
	else
	{
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_DIRECTORY)
		g_warning("803 Resources directory (%s) is missing.\n",sharedPath->str);
	}
	g_object_unref(gfi);
	g_object_unref(gf);
    }

    // Once a tape store is added here is where a default set of tapes for a user
    // will need to be created if the users config directory is missing.

    if(createConfigDirectory == TRUE)
    {
	GFile *gf;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(configPath->str);
	
	g_file_make_directory (gf,
                       NULL,
                       &error2);
	
	if(error2 != NULL)
	{
	    g_error("Could not create  directory:%s\n", error2->message);
	}

	g_object_unref(gf);
    }

    // New command line option parsing....
    context = g_option_context_new ("- Elliott 803 Emulator");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
    }

    if(loadCoreFileName != NULL)
	g_info("Core file set to %s\n",loadCoreFileName);
    
    if(windowSize != NULL)
    {
	if( (sscanf(windowSize,"%dx%d",&windowWidth,&windowHeight) == 2) &&
	    (windowWidth > 0) && (windowHeight > 0))
	{
	    windowSizeOk = TRUE;
	    g_info("Window size set to %d x %d\n",windowWidth,windowHeight);
	}
	else
	{
	    g_warning("%s is not a valid window size.\n",windowSize);
	}
    }

    if(!windowSizeOk)
    {
	windowWidth = 800;
	windowHeight = 600;
    }


    if(alsaName != NULL)
    {
	setDevice(alsaName);
	
    }

    
    // Initialise queues so that they can be used in initialisation code
    // to restore CPU state.
    LampsEventQueue = g_async_queue_new();
    ButtonEventQueue = g_async_queue_new();
    
    if(GtkInit(sharedPath,&argc, &argv))
    {
	
	if(!KeyboardInit(sharedPath,configPath)) goto done;
	if(!HandsInit(sharedPath,configPath)) goto done;

	// These can't fail
	ChargerInit(sharedPath,configPath);  
	PowerCabinetInit(sharedPath,configPath);
	CpuInit(sharedPath,configPath,loadCoreFileName);

	// initPLTS can fail but should not stop emualtor starting.
	PTSInit(sharedPath,configPath);

	// This can fail but isn't critical
	LoadScene(sharedPath,configPath);

	// This can't fail
	ButtonsInit(sharedPath,configPath);

	loadTextures1(sharedPath,configPath);

	// Start up the machine emulation in a separate thread
	EmulationThread = g_thread_new ("Emulation Code",
					worker,
					NULL);

	// It all happend in here !
	GtkRun(windowWidth,windowHeight);

	// This will stop the emualtion thread.
	Running = FALSE;

	g_thread_join(EmulationThread);

	CpuTidy(configPath,saveCoreFileName);

	KeyboardTidy(configPath);
	
	SaveScene(sharedPath,configPath);
    }
done:
    return(EXIT_SUCCESS);
}

