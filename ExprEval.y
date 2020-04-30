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
extern SymTab *intFunctionTable;
extern SymTab *voidFunctionTable;
extern SymTab *localTable;
extern SymTab *functionParamsTable;
extern int returnFlag;
extern int funcContextFlag;
extern int noParamsFlag;
extern int noLocalsFlag;
extern int functionParamsCount;

%}


%union {
  long val;
  char * string;
  struct ExprRes * ExprRes;
  struct InstrSeq * InstrSeq;
  struct BExprRes * BExprRes;
  struct ExprResList * ExprResList;
  struct IdList * IdList;
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
%type <BExprRes> BFactor
%type <ExprResList> ExprList
%type <IdList> IdentList
%type <IdList> ParamList
%type <IdList> LocalsList

%token Ident
%token IntLit
%token Int
%token Write
%token IF
%token ELSE
%token FOR
%token WHILE
%token EQ
%token LEQ
%token GEQ
%token LT
%token GT
%token NEQ
%token BAND
%token BOR
%token PrintSpaces
%token PrintLine
%token READ
%token BOOL
%token RETURN
%token VOID

%%

Prog			:	Declarations StmtSeq				{Finish($2); } ;
Declarations	:	Dec Declarations					{ };
Declarations	:						  		{ };
Dec     : IntDec                {};
Dec     : FunctionDec           {};
IntDec			:	Int Id {enterName(table, $2); }';'	{};
IntDec     : Int Id {enterName(table, $2); }'[' IntLit {setCurrentAttr(table, (void *) atoi(yytext));}']' ';' {};
FunctionDec     : Int Id {enterName(intFunctionTable, $2);enterName(functionParamsTable, $2); localTable = createSymTab(10);} '(' ParamList ')' LocalsList '{' {returnFlag++; funcContextFlag++;} StmtSeq {funcContextFlag--;}'}'{checkReturn(); defineAndAppendFunction((void *)intFunctionTable, $2, $10); setCurrentAttr(functionParamsTable, (void *) functionParamsCount); cleanUpFunction();};
FunctionDec     : VOID Id {enterName(voidFunctionTable, $2); enterName(functionParamsTable, $2); localTable = createSymTab(10);} '(' ParamList ')' LocalsList '{' {funcContextFlag++;} StmtSeq {funcContextFlag--;}'}'{ defineAndAppendFunction((void *) voidFunctionTable, $2, $10); setCurrentAttr(functionParamsTable, (void *) functionParamsCount); cleanUpFunction();};
ParamList : Param ',' ParamList {};
ParamList : Param               {};
ParamList :                     {};
Param     : Int Id              {insertScopedName($2); functionParamsCount++;};
LocalsList: LocalDec LocalsList {};
LocalsList:                     {};
LocalDec  : Int Id ';'          {insertScopedName($2);};
StmtSeq 	:	Stmt StmtSeq			  {$$ = AppendSeq($1, $2); } ;
StmtSeq		:											{$$ = NULL;} ;
Stmt      : Id '(' ')' ';'      {$$ = doVoidFunctionCall($1);};
Stmt      : RETURN Expr ';'     {$$ = doReturnInt($2);};
Stmt      : RETURN ';'          {$$ = doReturn();};
Stmt			:	Write Expr ';'			{$$ = doPrint($2); };
Stmt      : Write '(' ExprList ')' ';' {$$ = doPrintExprList($3);};
Stmt      : PrintSpaces '(' Expr ')' ';'  {$$ = doPrintSpaces($3);};
Stmt      : PrintLine ';'       {$$ = doPrintline();};
Stmt      : READ '(' IdentList ')' ';' {$$ = doInputOnList($3);};
Stmt      : READ '(' Id ')' ';' {$$ = doInputOnId($3);};
Stmt			:	Id '=' Expr ';'			{$$ = doAssign($1, $3);} ;
Stmt      : Id '[' Expr ']' '=' Expr ';'  {$$ = doArrayAssign($1, $3, $6);};
Stmt			:	IF '(' BExpr ')' '{' StmtSeq '}' ELSE '{' StmtSeq '}' {$$ = doIfElse($3, $6, $10);};
Stmt			:	IF '(' BExpr ')' '{' StmtSeq '}'	{$$ = doIf($3, $6);};
Stmt      : WHILE '(' BExpr ')' '{' StmtSeq '}' {$$ = doWhile($3, $6);};
Stmt      : FOR '(' Id '=' Expr ';' BExpr ';' Id '=' Expr ')' '{' StmtSeq '}' {$$ = doFor($3, $5, $7, $9, $11, $14);};
BExpr     : BExpr BOR BTerm     {$$ = doBOR($1, $3);};
BExpr     : BTerm               {$$ = $1;};
BTerm     : BTerm BAND BFactor  {$$ = doBAND($1, $3);};
BTerm     : BFactor             {$$ = $1;};
BFactor   : '!' BFactor         {$$ = doNot($2);};
BFactor	  :	Expr EQ Expr				{$$ = doBExprRel($1, $3, 1);};
BFactor   : Expr LEQ Expr       {$$ = doBExprRel($1, $3, 2);};
BFactor   : Expr LT Expr        {$$ = doBExprRel($1, $3, 3);};
BFactor   : Expr GEQ Expr       {$$ = doBExprRel($1, $3, 4);};
BFactor   : Expr GT Expr        {$$ = doBExprRel($1, $3, 5);};
BFactor   : Expr NEQ Expr       {$$ = doBExprRel($1, $3, 6);};
BFactor   : BOOL ':' Expr       {$$ = doExprToBFactor($3);};
BFactor   : '(' BExpr ')'       {$$ = $2;};
IdentList : IdentList ',' Id    {$$ = doAppendIdentList($1, $3);};
IdentList : Id ',' Id           {$$ = doIdToIdList($1, $3);};
ExprList  : ExprList ',' Expr   {$$ = doAppendExprList($1, $3);};
ExprList  : Expr ',' Expr       {$$ = doExprToExprList($1, $3);};
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
Expo  		:	Id								  { $$ = doRval($1); };
Expo      : Id '[' Expr ']'     { $$ = doArrayRval($1, $3);};
Expo      : Id '(' ')'          {$$ = doIntFunctionCall($1);};
Id			  : Ident								{ $$ = strdup(yytext);}

%%

int yyerror(char *s)  {
  writeIndicator(getCurrentColumnNum());
  writeMessage("Illegal Character in YACC");
  return 1;
}
