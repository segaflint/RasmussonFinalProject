/* Semantics.h
   The action and supporting routines for performing semantics processing.
*/

/* Semantic Records */
struct IdList {
  struct SymEntry * TheEntry;
  struct IdList * Next;
};

struct ExprRes {
  int Reg;
  struct InstrSeq * Instrs;
};

struct ExprResList {
	struct ExprRes *Expr;
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
extern struct InstrSeq *  doAssign(char * name,  struct ExprRes * Res1);
extern struct ExprRes *  doArith(struct ExprRes * Res1,  struct ExprRes * Res2, char * inst); // Arithmetic instructions
extern struct ExprRes * doMod(struct ExprRes * Res1, struct ExprRes * Res2); // Modulus operation
extern struct ExprRes * doUnary(struct ExprRes * Res);
extern struct ExprRes * doExponent(struct ExprRes * Res1, struct ExprRes * Res2);
extern struct InstrSeq *  doPrint(struct ExprRes * Expr);
extern struct BExprRes * doBExprRel(struct ExprRes * Res1,  struct ExprRes * Res2, int relationalOperator);
extern struct InstrSeq * doIf(struct BExprRes *bRes, struct InstrSeq * seq);
extern struct BExprRes * doNot(struct BExprRes * Res);
extern struct BExprRes * doBAND(struct BExprRes * bRes1, struct BExprRes * bRes2);
extern struct BExprRes * doBOR(struct BExprRes * bRes1, struct BExprRes * bRes2);
extern struct InstrSeq * doPrintline();



extern void	Finish(struct InstrSeq *Code);
