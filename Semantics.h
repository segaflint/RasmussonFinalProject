/* Semantics.h
   The action and supporting routines for performing semantics processing.
*/

/* Semantic Records */
struct IdList {
  struct SymEntry * TheEntry;
  struct ExprRes * ArrayIndexRes;
  struct IdList * Next;
};

struct ExprRes {
  int Reg;
  struct InstrSeq * Instrs;
};

struct ExprResList {
	struct ExprRes *Expr;
  char * arrayName;
	struct ExprResList * Next;
};

struct BExprRes {
  char * Label;
  struct InstrSeq * Instrs;
};


/* Semantics Actions */

// Integer Expressions
extern struct ExprRes *  doIntLit(char * digits);
extern struct ExprRes *  doRval(char * name);
extern struct InstrSeq * doAssign(char * name,  struct ExprRes * Res1);
extern struct ExprRes *  doArith(struct ExprRes * Res1,  struct ExprRes * Res2, char * inst); // Arithmetic instructions
extern struct ExprRes *  doMod(struct ExprRes * Res1, struct ExprRes * Res2); // Modulus operation
extern struct ExprRes *  doUnary(struct ExprRes * Res);
extern struct ExprRes *  doExponent(struct ExprRes * Res1, struct ExprRes * Res2);
extern struct BExprRes * doBExprRel(struct ExprRes * Res1,  struct ExprRes * Res2, int relationalOperator);
extern struct BExprRes * doExprToBFactor(struct ExprRes * res);
extern struct BExprRes * doNot(struct BExprRes * Res);
extern struct BExprRes * doBAND(struct BExprRes * bRes1, struct BExprRes * bRes2);
extern struct BExprRes * doBOR(struct BExprRes * bRes1, struct BExprRes * bRes2);

/* Integer I/O*/
extern struct InstrSeq * doPrintExprList(struct ExprResList * exprList);
extern struct ExprResList * doAppendExprList(struct ExprResList * resList, struct ExprRes * res);
extern struct ExprResList * doExprToExprList(struct ExprRes * res1, struct ExprRes * res2);

extern struct InstrSeq * doPrintline();
extern struct InstrSeq * doPrintSpaces(struct ExprRes * res);
extern struct InstrSeq * doPrint(struct ExprRes * Expr);

// extern struct InstrSeq * doInputOnId(char * name);
extern struct InstrSeq * doInputOnList(struct IdList * list );

extern struct IdList * doArrayToIdList(char * name, struct ExprRes * indexRes );
extern struct IdList * doAppendIdentList(struct IdList * IdentList1, struct IdList * IdentList2);
extern struct IdList * doIdToIdList(char * name);

extern struct InstrSeq * doIf(struct BExprRes *bRes, struct InstrSeq * seq);
extern struct InstrSeq * doIfElse(struct BExprRes * bRes, struct InstrSeq * ifSeq, struct InstrSeq * elseSeq);
extern struct InstrSeq * doWhile(struct BExprRes * bRes, struct InstrSeq * seq);
extern struct InstrSeq * doFor(char * initVar, struct ExprRes * initExpr, struct BExprRes * bRes, char * updateVar, struct ExprRes * updateExprRes , struct InstrSeq * seq);

extern struct ExprRes * doArrayRval(char * name, struct ExprRes * res);
extern struct InstrSeq * doArrayAssign(char * name, struct ExprRes * arrayIndexRes, struct ExprRes * assignmentRes );

extern void defineAndAppendFunction(void * table, char * functionName, struct InstrSeq * codeBody);
extern struct InstrSeq * doReturnInt(struct ExprRes * res);
extern struct InstrSeq * doReturn();
extern void checkReturn();
extern struct InstrSeq * doVoidFunctionCall(char * name,  struct ExprResList * ExprList);
extern struct ExprRes * doIntFunctionCall(char * name, struct ExprResList * ExprList);

extern struct InstrSeq * saveRAAndSeq();
extern struct InstrSeq * restoreRAAndSeq();

struct InstrSeq * pushParameters(struct ExprResList * ExprList);

extern void insertScopedName(char * name);

extern void cleanUpFunction();

extern void	Finish(struct InstrSeq *Code);

void generateTableInstructions(struct InstrSeq * code);
void appendFinishedFunction(struct InstrSeq * code);
