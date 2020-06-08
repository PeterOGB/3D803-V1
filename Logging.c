/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#define G_LOG_DOMAIN "803 Emulator"

#include <gtk/gtk.h>
#include "Logging.h"

static gboolean debugFlag = FALSE;
static int debugCount = 0;



static GLogWriterOutput
log_writer(GLogLevelFlags log_level,
	   const GLogField *fields,
	   __attribute__((unused)) gsize            n_fields,
	   __attribute__((unused))gpointer         user_data)
{
    gsize n;
    const gchar *file,*line,*func,*message,*level;
    const GLogField *field;
    gboolean debugMessage = FALSE;

    g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
    g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

    file=line=func=message=level=NULL;
    
    for(n=0;n<n_fields;n++)
    {
	field = &fields[n];

	if (g_strcmp0 (field->key, "MESSAGE") == 0)
	    message = field->value;
	else if (g_strcmp0 (field->key, "CODE_FILE") == 0)
	    file = field->value;
	else if (g_strcmp0 (field->key, "CODE_LINE") == 0)
	    line = field->value;
	else if (g_strcmp0 (field->key, "CODE_FUNC") == 0)
	    func = field->value;
	else if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
	    func = field->value;
	// These next two are ignored (probably NOT the right thing to do !)
	else if (g_strcmp0 (field->key, "PRIORITY") == 0)
	    continue;
	else if (g_strcmp0 (field->key, "GLIB_OLD_LOG_API") == 0)
	    continue;

	// Assumes value is a string !
	else printf("\nKEY (%s) VALUE(%s) not handled\n",field->key,(const char *)field->value);
    }

    switch (log_level & G_LOG_LEVEL_MASK)
    {
    case G_LOG_LEVEL_ERROR:
	level = "ERROR  ";
	break;
    case G_LOG_LEVEL_CRITICAL:
	level = "CRITICAL";
	break;
    case G_LOG_LEVEL_WARNING:
	level = "WARNING";
	break;
    case G_LOG_LEVEL_MESSAGE:
	level = "MESSAGE";
	break;
    case G_LOG_LEVEL_INFO:
	level = "INFO   ";
	break;
    case G_LOG_LEVEL_DEBUG:
	level = "DEBUG  ";
	debugMessage = TRUE;
	break;
    default:
	level = "UNKNOWN";
	break;
    }

    if((!debugMessage) || (debugFlag))
    {
	size_t len = strlen(message);
	printf("%s:%s:%s:%s:%s",level,file,line,func,message);
	
	if((len > 0) && (message[len-1] != '\n')) printf("\n");
    }
    return G_LOG_WRITER_HANDLED;
}

void LoggingInit(gboolean debug)
{
     g_log_set_writer_func (log_writer,
			   NULL,NULL);
     debugFlag = debug;
}

void debugOn(void)
{
    debugCount += 1;
    debugFlag = TRUE;
}

void debugOff(void)
{
    debugCount -= 1;
    if(debugCount <= 0)
	debugFlag = FALSE;
}
    
