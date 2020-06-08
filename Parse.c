/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/
/************************** Message and File parsing code ***********/

#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <glib.h>
#include <libiberty/libiberty.h>
#include <strings.h>
#include <string.h>
#include "Parse.h"

#define PARSE_DEBUG 0
static char *unparsedMessage= NULL;

static Token *currentToken= NULL;

static int fieldCount;
static char **fields= NULL;
void postParse(void);
int preParse(char *line);
/**
 * Uses buildargv from libiberty to chop up the recieved message into fields.
 * @param line The recieved message.
 */

int preParse(char *line)
{
	char **targs;

	if (fields != NULL)
	{
		g_warning("preParse: postParse not called, freeing previous fields\n");
		postParse();
	}

#if PARSE_DEBUG
	printf("About to parse (%s)\n",line);
#endif
	if ((fields = buildargv(line)) == NULL)
	{
		g_warning("buildargv failed to parse!\n\n");
		return FALSE;
	}
	else
	{
		fieldCount = 0;
		for (targs = fields; *targs != NULL; targs++)
		{
			fieldCount += 1;
#if PARSE_DEBUG
			printf ("preParse \t\"%s\"\n", *targs);
#endif
		}
#if PARSE_DEBUG
		printf ("\n");
#endif
	}
	return TRUE;
}

/**
 * frees up the fields array.
 */

void postParse(void)
{
#if PARSE_DEBUG
	char **targs;

	for (targs = fields; *targs != NULL; targs++)
	{
		printf ("postParse1\t\"%s\"\n", *targs);
	}
	printf("\n");
#endif

	if (fields != NULL)
	{
		freeargv(fields);
		fields = NULL;
	}
	else
	{
		g_warning("postParse: fields = NULL\n");
	}
}

/**
 * Act on a field in the message.
 * When a match is found the handler is called with the index as the parameter.
 * If there is no handler for a token, then just the value is returned.
 * @param Tokens An array of #Token to check against.
 * @param index Selects the field to parse.
 */

int parse(Token *Tokens, int index)
{
	int m, ret;
	char *tok;
#if PARSE_DEBUG
		printf("%s Tokens->string now (%s) %p\n",__FUNCTION__,Tokens->string,Tokens);
#endif
	if ((index >= fieldCount) || (index < 0))
	{
		g_warning("parse: index (%d) out or range [0..%d]\n", index, fieldCount-1);
		return FALSE;
	}

	if (fields == NULL)
	{
		g_warning("parse: parse called before preParse\n");
		return FALSE;
	}

	tok = fields[index];
	if (tok == NULL)
	{
		g_warning("empty message\n");
		return (-1);
	}
	if (Tokens == NULL)
	{
		m = sscanf(tok, "%d", &ret);
		if (m != 1)
		{
			g_warning("parse: failed to convert %s to an integer\n", tok);
			return FALSE;
		}
		return (ret);
	}

	// THIS HAS CHANGED !!!
	// Old behaviour
	/* Use a NULL sting for the end of the table which means the
	 handler can be called as a default if no match has been found.
	 */

	// New behaviour
	/* Use a NULL sting for the end of the table which with a 
	 handler can be called  at the end of the file.
	 */

	currentToken = NULL;
	while (Tokens->string != NULL)
	{
#if PARSE_DEBUG
	    printf("comparing (%s) and (%s) %p\n",tok,Tokens->string,Tokens);
#endif
	    if (!strcmp(tok, Tokens->string))
	    {
#if PARSE_DEBUG
		printf("MATCH for %s\n",tok);
#endif
		/* If the handler is NULL, just return the vlaue field.
		   This removes the need for "tokenToValue"  06/11/05 */
		currentToken = Tokens;

#if PARSE_DEBUG
		printf("currentToken set to %p string=%s\n",currentToken,currentToken->string);
#endif

		if (Tokens->handler != NULL)
		    ret = (Tokens->handler)(index);
		else
		    ret = Tokens->value;
		return (ret);
	    }
	    Tokens++;
#if PARSE_DEBUG
	    printf("Tokens->string now (%s) %p\n",Tokens->string,Tokens);
#endif
	}
	/* No match found, so call the default if one has been defined */
	//if (Tokens->handler != NULL)
	//{
	//	ret = (Tokens->handler)(index);
	//	return (ret);
	//}
	
	{
	    char name[1000];
	    int self;
	    //ssize_t n;
	    bzero(name, 1000);
	    self = open("/proc/self/cmdline", O_RDONLY);
	    //n =
	    read(self,name,999);
	    close(self);

	    g_warning("parse:(%s) No Match for %s\n",
				name, tok);
	}
	return FALSE;
}

/**
 * Get a pointer to a field in the message.
 * @param index Selects the field.  First one is 0. Use #getFieldCount() to find upper limit.
 */
char *getField(int index)
{
	if ((index >= fieldCount) || (index < 0))
	{
		g_warning("parse: index (%d) out or range [0..%d]\n", index, fieldCount-1);
		return NULL;
	}
	return (fields[index]);
}

/** Get the number of fields in the message */
int getFieldCount(void)
{
	return (fieldCount);
}

/**
 * Parse a file using the same techinques as used on messages.
 * @param filename
 * @param Tokens The top level tokens for the parse tree.
 * @param comment The start of line char for a comment.
 * Returns TRUE if no errors else FALSE;
 */

gboolean parseFile(const gchar *filename,Token *Tokens,char *comment)
{
    GIOChannel *file;
    gchar *message;
    gsize length,term;
    GError *error = NULL;
    GIOStatus status;
    char *cp;

    g_debug("%s called %s\n",__FUNCTION__,filename);

    if((file = g_io_channel_new_file(filename,"r",&error)) == NULL)
    {
	g_warning("failed to open file %s\n",filename);
	return FALSE;
    }
    
    while((status = g_io_channel_read_line(file,&message,&length,&term,&error)) == G_IO_STATUS_NORMAL)
    {
	if(message != NULL)
	{
	    if(term != 0)
	    {
		if((cp = strchr(message,'\n')))
		{
		    *cp = ' ';
		}

		unparsedMessage = message;

		if( (comment == NULL) || (*comment != *message))
		{
		    preParse(message);
		    parse(Tokens,0);
		    postParse();
		}
	    }
	    g_free(message);
	    unparsedMessage = NULL;
	}
    }
    if(status == G_IO_STATUS_EOF)
    {
	g_debug("eof file=%p\n",file);
	g_io_channel_shutdown(file,FALSE,NULL);
	g_io_channel_unref(file);

	// Find end of Tokens table (should be more "robust")
	while (Tokens->string != NULL) Tokens++;
	if(Tokens->handler != NULL) (Tokens->handler)(-1);
	
	return TRUE;
    }
    else
    {
	g_warning("Error reading file %s\n",filename);
	g_io_channel_shutdown(file,FALSE,NULL);
	g_io_channel_unref(file);
	return FALSE;
    }
}


void updateConfigFile(const char *fileName,GString *userPath,GString *configString)
{
    GString *oldState,*newState,*State;

    GIOChannel *file;
        
    oldState = g_string_new(userPath->str);
    newState = g_string_new(userPath->str);
    State = g_string_new(userPath->str);

    g_string_append_printf(oldState,"%s.old",fileName);
    g_string_append_printf(newState,"%s.new",fileName);
    g_string_append_printf(State,"%s",fileName);

    file = g_io_channel_new_file(newState->str,"w",NULL);
    if(file != NULL)
    {
        g_io_channel_write_chars(file,configString->str,-1,NULL,NULL);

        g_io_channel_shutdown(file,TRUE,NULL);
        g_io_channel_unref (file);
        
        unlink(oldState->str);
        rename(State->str,oldState->str);
        rename(newState->str,State->str);
    }

    g_string_free(State,TRUE);
    g_string_free(newState,TRUE);
    g_string_free(oldState,TRUE);
}


gboolean readConfigFile(const char *fileName,GString *userPath,Token *savedStateTokens)
{
    GString *configFileName;
    GIOChannel *file;
    GError *error = NULL;
    GIOStatus status;
    gchar *message;
    gsize length,term;
    char *cp;

    configFileName = g_string_new(userPath->str);
    g_string_append_printf(configFileName,"%s",fileName);

    if((file = g_io_channel_new_file(configFileName->str,"r",&error)) == NULL)
    {
	g_warning("failed to open file %s due to %s\n",configFileName->str,error->message);
	return FALSE;
    }

    while((status = g_io_channel_read_line(file,&message,&length,&term,&error)) 
	  == G_IO_STATUS_NORMAL)
    {
	if(message != NULL)
	{
	    if(term != 0)
	    {
		if((cp = strchr(message,'\n')))
		{
		    *cp = ' ';
		}

		unparsedMessage = message;

		if(message[0] != '#')
		{
		    preParse(message);
		    parse(savedStateTokens,0);
		    postParse();
		}
	    }
	    g_free(message);
	    unparsedMessage = NULL;
	}
    }

    g_io_channel_shutdown(file,FALSE,NULL);
    g_io_channel_unref(file);
    if(status == G_IO_STATUS_EOF)
    {
	return TRUE;
    }
    else
    {
	g_warning("Error reading file %s\n",configFileName->str);
	return FALSE;
    }
}

