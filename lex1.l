%{
//#include "yaccExample.h"
#include "IOMngr.h"
#include "y.tab.h"

#define YY_INPUT(buf,result,max_size) \
	{ int c = getNextSourceChar(); \
          result = (c == EOF) ? YY_NULL : (buf[0] = c, 1); \
        }

%}


letter [A-Za-z]
digit [0-9]

%%
if					{return IF;}
else 				{return ELSE;}
int					{return Int;}
boolean			{return BOOL;}
for					{return FOR;}
while				{return WHILE;}
printLine   {return PrintLine;}
printSpaces {return PrintSpaces;}
print					{return Write;}
read 				{return READ;}
return 			{return RETURN;}
void 				{return VOID;}
{letter}({letter}|{digit})*	{return Ident;}
{digit}{digit}*		{return IntLit;}
\=\=				{return EQ;}
\>\=				{return GEQ;}
\>					{return GT;}
\<\=				{return LEQ;}
\<					{return LT;}
\!\=				{return NEQ;}
\=					{return '=';}
\+					{return '+';}
\-					{return '-';}
\*					{return '*';}
\%					{return '%';}
\/					{return '/';}
\!					{return '!';}
\&\&				{return BAND;}
\|\|				{return BOR;}
\^					{return '^';}
\;					{return ';';}
\{					{return '{';}
\}					{return '}';}
\(					{return '(';}
\)					{return ')';}
\,					{return ',';}
\: 					{return ':';}
\[					{return '[';}
\]					{return ']';}


[ ]				{}
\t				{}
\r				{}
\n				{}

.				{writeIndicator(getCurrentColumnNum());
				   writeMessage("Illegal Character in lex");}

%%


int yywrap () {
    return 1;
}
