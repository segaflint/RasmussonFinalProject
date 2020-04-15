%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "IOMngr.h"
#include "SymTab.h"
#include "Semantics.h"
#include "CodeGen.h"

extern int yylex();	/* The next token function. */
extern char *yytext;   /* The matched token text.  */
extern int yyleng;      /* The token text length.   */
extern int yyparse();
extern int yyerror(char *);
void dumpTable();

extern SymTab *table;

%}


%union {
  long val;
  char * string;
  struct ExprRes * ExprRes;
  struct InstrSeq * InstrSeq;
  struct BExprRes * BExprRes;
}

%type <string> Id
%type <ExprRes> Expo
%type <ExprRes> Factor
%type <ExprRes> Term
%type <ExprRes> Expr
%type <InstrSeq> StmtSeq
%type <InstrSeq> Stmt
%type <BExprRes> BExpr
%type <BExprRes> BTerm

%token Ident
%token IntLit
%token Int
%token Write
%token IF
%token FOR
%token WHILE
%token EQ
%token LEQ
%token GEQ
%token LT
%token GT
%token NEQ
%token ANDOP
%token OROP

%%

Prog			:	Declarations StmtSeq				{Finish($2); } ;
Declarations	:	Dec Declarations					{ };
Declarations	:						  		{ };
Dec			:	Int Ident {enterName(table, yytext); }';'	{};
StmtSeq 		:	Stmt StmtSeq			{$$ = AppendSeq($1, $2); } ;
StmtSeq		:											{$$ = NULL;} ;
Stmt			:	Write Expr ';'			{$$ = doPrint($2); };
Stmt			:	Id '=' Expr ';'			{$$ = doAssign($1, $3);} ;
Stmt			:	IF '(' BExpr ')' '{' StmtSeq '}'	{$$ = doIf($3, $6);};
BExpr     : '!' BExpr           {$$ = doNot($2);};
BExpr     : BTerm               {$$ = $1;};
BTerm		  :	Expr EQ Expr				{$$ = doBExprRel($1, $3, 1);};
BTerm     : Expr LEQ Expr       {$$ = doBExprRel($1, $3, 2);};
BTerm     : Expr LT Expr        {$$ = doBExprRel($1, $3, 3);};
BTerm     : Expr GEQ Expr       {$$ = doBExprRel($1, $3, 4);};
BTerm     : Expr GT Expr        {$$ = doBExprRel($1, $3, 5);};
BTerm     : Expr NEQ Expr       {$$ = doBExprRel($1, $3, 6);};
BTerm     : '(' BExpr ')'       {$$ = $2;};
Expr			:	Expr '+' Term				{$$ = doArith($1, $3, "add");};
Expr      : Expr '-' Term       {$$ = doArith($1, $3, "sub");};
Expr			:	Term						    {$$ = $1;};
Term      : Term '/' Factor     { $$ = doArith($1, $3, "div"); };
Term      : Term '%' Factor     { $$ = doMod($1, $3);};
Term		  :	Term '*' Factor			{ $$ = doArith($1, $3, "mul"); };
Term		  :	Factor						  { $$ = $1;};
Factor    : Expo '^' Factor     { $$ = doExponent($1, $3);};
Factor    : Expo                { $$ = $1;};
Expo      : '-' Expo            { $$ = doUnary($2);};
Expo      : '(' Expr ')'        { $$ = $2;};
Expo   		:	IntLit							{ $$ = doIntLit(yytext); };
Expo  		:	Ident								{ $$ = doRval(yytext); };
Id			  : Ident								{ $$ = strdup(yytext);}

%%

int yyerror(char *s)  {
  writeIndicator(getCurrentColumnNum());
  writeMessage("Illegal Character in YACC");
  return 1;
}
