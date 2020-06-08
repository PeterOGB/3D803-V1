/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

/** \typedef Token
 * A Token is a space delimited string in a message 
 */
typedef struct token
{
    const char *string;   /**< The string to match */
    int value;      /**< The value to be returned by #tokenToValue */ 
    int (*handler)(int n); /**< The function to be called when the token found */
} Token;


gboolean parseFile(const gchar *filename,Token *Tokens,char *comment);
char *getField(int index);
int getFieldCount(void);
int parse(Token *Tokens, int index);


void updateConfigFile(const char *fileName,GString *userPath,GString *configString);
gboolean readConfigFile(const char *fileName,GString *userPath,Token *savedStateTokens);
