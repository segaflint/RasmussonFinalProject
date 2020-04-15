/* Semantics.c
   Support and semantic action routines.

*/

#include <strings.h>
#include <stdlib.h>

#include "CodeGen.h"
#include "Semantics.h"
#include "SymTab.h"
#include "IOMngr.h"

extern SymTab *table;

/* Semantics support routines */

struct ExprRes *  doIntLit(char * digits)  {

   struct ExprRes *res;

  res = (struct ExprRes *) malloc(sizeof(struct ExprRes));
  res->Reg = AvailTmpReg();
  res->Instrs = GenInstr(NULL,"li",TmpRegName(res->Reg),digits,NULL);

  return res;
}

struct ExprRes *  doRval(char * name)  {

   struct ExprRes *res;

   if (!findName(table, name)) {
		writeIndicator(getCurrentColumnNum());
		writeMessage("Undeclared variable");
   }
  res = (struct ExprRes *) malloc(sizeof(struct ExprRes));
  res->Reg = AvailTmpReg();
  res->Instrs = GenInstr(NULL,"lw",TmpRegName(res->Reg),name,NULL);

  return res;
}

struct ExprRes *  doArith(struct ExprRes * Res1, struct ExprRes * Res2, char * inst)  {

  int reg;

  reg = AvailTmpReg();
  AppendSeq(Res1->Instrs,Res2->Instrs);
  AppendSeq(Res1->Instrs,GenInstr(NULL, inst,
                                       TmpRegName(reg),
                                       TmpRegName(Res1->Reg),
                                       TmpRegName(Res2->Reg)));
  ReleaseTmpReg(Res1->Reg);
  ReleaseTmpReg(Res2->Reg);
  Res1->Reg = reg;
  free(Res2);
  return Res1;
}

struct ExprRes * doMod(struct ExprRes * Res1, struct ExprRes * Res2) {
    //Res1->Reg holds the quotent, and Res2->Reg holds the divisor

    //x % y = x - (Floor(x / y) * y) so do:
    //1. z = x / y
    //2. a = z * y // after, free y and free z
    //3. result = x - a  //after, free x and a

    int reg;
    int reg1;

    //z = x / y
    //z => reg
    //x => Res1->reg
    //y => Res2->Reg
    reg = AvailTmpReg();
    AppendSeq(Res1->Instrs,Res2->Instrs); // add instructions for Expressions together
    AppendSeq(Res1->Instrs,GenInstr(NULL, "div", TmpRegName(reg),
        TmpRegName(Res1->Reg), TmpRegName(Res2->Reg)));

    //a = z * y
    //a => reg1
    //z => reg
    //y => Res2->Reg
    reg1 = AvailTmpReg();
    AppendSeq(Res1->Instrs,GenInstr(NULL, "mul", TmpRegName(reg1),
        TmpRegName(reg), TmpRegName(Res2->Reg)));

    //result = x - a
    //result => Res1->Reg
    // x => Res1->Reg
    // a => reg1
    AppendSeq(Res1->Instrs,GenInstr(NULL, "sub", TmpRegName(Res1->Reg),
        TmpRegName(Res1->Reg), TmpRegName(reg1)));

    ReleaseTmpReg(reg);
    ReleaseTmpReg(reg1);
    ReleaseTmpReg(Res2->Reg);
    free(Res2);
    return Res1;
}

struct ExprRes * doUnary(struct ExprRes * Res) {
    int reg;

    reg = AvailTmpReg();
    AppendSeq(Res->Instrs, GenInstr(NULL, "sub", TmpRegName(reg),
        "$zero", TmpRegName(Res->Reg)));

    ReleaseTmpReg(Res->Reg);
    Res->Reg = reg;
    return Res;
}

struct ExprRes * doExponent(struct ExprRes * Res1, struct ExprRes * Res2) {
    int counterReg; // "$t0" in the below example
    int sltResReg; // "$t3" in the below example
    int resultReg; // "$t4" in the below example
    char * loopLabel; //L1
    char * endLabel; //L2

    counterReg = AvailTmpReg();
    sltResReg = AvailTmpReg();
    resultReg = AvailTmpReg();
    loopLabel = GenLabel();
    endLabel = GenLabel();
    AppendSeq(Res1->Instrs, Res2->Instrs);
    // "$t0" is counterReg
    // "$t1" is the Res2->Reg (exponent)
    // "$t2" is the Res1->Reg (original base number of the expression)
    // addi $t0, $zero, 0  # i = 0 // needs to be 1 because the first instance has the initial two multiplied
    AppendSeq(Res1->Instrs, GenInstr(NULL, "addi", TmpRegName(counterReg), "$zero", "0"));
    // addi $t4, $zero, 1 # t4 = 1
    AppendSeq(Res1->Instrs, GenInstr(NULL, "addi", TmpRegName(resultReg), "$zero", "1"));

    // L1 slt  $t3, $t0, $t1  # i < t1 //   slt $d, $s, $t  If $s is less than $t, $d is set to one. It gets zero otherwise.
    AppendSeq(Res1->Instrs, GenInstr(loopLabel, "slt", TmpRegName(sltResReg),
        TmpRegName(counterReg), TmpRegName(Res2->Reg)));
    //    beq $t3,$zero, L2 # exit loop
    AppendSeq(Res1->Instrs, GenInstr(NULL, "beq", TmpRegName(sltResReg), "$zero", endLabel ));
    //    mul  $t4 $t4 $t2    # work, multiply
    AppendSeq(Res1->Instrs, GenInstr(NULL, "mul", TmpRegName(resultReg),
        TmpRegName(resultReg), TmpRegName(Res1->Reg)));
    //    addi $t0, $t0, 1    # i++
    AppendSeq(Res1->Instrs, GenInstr(NULL, "addi", TmpRegName(counterReg), TmpRegName(counterReg), "1"));
    //    j L1 # loop
    AppendSeq(Res1->Instrs, GenInstr(NULL, "j", loopLabel, NULL, NULL));
    // L2:
    AppendSeq(Res1->Instrs, GenInstr(endLabel, NULL, NULL, NULL, NULL));

    //Free all Registers and results that are no longer used
    ReleaseTmpReg(Res1->Reg);
    ReleaseTmpReg(Res2->Reg);
    ReleaseTmpReg(counterReg);
    ReleaseTmpReg(sltResReg);
    free(Res2);
    Res1->Reg = resultReg;

    return Res1;
}

struct InstrSeq * doPrint(struct ExprRes * Expr) {

  struct InstrSeq *code;

  code = Expr->Instrs;

    AppendSeq(code,GenInstr(NULL,"li","$v0","1",NULL));
    AppendSeq(code,GenInstr(NULL,"move","$a0",TmpRegName(Expr->Reg),NULL));
    AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

    AppendSeq(code,GenInstr(NULL,"li","$v0","4",NULL));
    AppendSeq(code,GenInstr(NULL,"la","$a0","_nl",NULL));
   AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

    ReleaseTmpReg(Expr->Reg);
    free(Expr);

  return code;
}

struct InstrSeq * doAssign(char *name, struct ExprRes * Expr) {

  struct InstrSeq *code;


   if (!findName(table, name)) {
		writeIndicator(getCurrentColumnNum());
		writeMessage("Undeclared variable");
   }

  code = Expr->Instrs;

  AppendSeq(code,GenInstr(NULL,"sw",TmpRegName(Expr->Reg), name,NULL));

  ReleaseTmpReg(Expr->Reg);
  free(Expr);

  return code;
}

struct BExprRes * doBExprRel(struct ExprRes * Res1,  struct ExprRes * Res2, int relationalOperator) {
	struct BExprRes * bRes;
  int reg;
	AppendSeq(Res1->Instrs, Res2->Instrs);
 	bRes = (struct BExprRes *) malloc(sizeof(struct BExprRes));
	bRes->Label = GenLabel();

  switch (relationalOperator) {
    case 1: //EQ
      AppendSeq(Res1->Instrs, GenInstr(NULL, "bne", TmpRegName(Res1->Reg), TmpRegName(Res2->Reg), bRes->Label));
      break;
    case 4: //GEQ
      reg = AvailTmpReg();
      AppendSeq(Res1->Instrs, GenInstr(NULL, "slt", TmpRegName(reg), TmpRegName(Res1->Reg), TmpRegName(Res2->Reg))); //Check Res1 < Res2
      AppendSeq(Res1->Instrs, GenInstr(NULL, "beq", TmpRegName(reg), "1", bRes->Label));
      ReleaseTmpReg(reg);
      break;
    case 2: //LEQ
      reg = AvailTmpReg();
      AppendSeq(Res1->Instrs, GenInstr(NULL, "slt", TmpRegName(reg), TmpRegName(Res2->Reg), TmpRegName(Res1->Reg))); //Check Res2 < Res1
      AppendSeq(Res1->Instrs, GenInstr(NULL, "beq", TmpRegName(reg), "1", bRes->Label));
      ReleaseTmpReg(reg);
      break;
    case 5: //GT
      reg = AvailTmpReg();
      AppendSeq(Res1->Instrs, GenInstr(NULL, "slt", TmpRegName(reg), TmpRegName(Res2->Reg), TmpRegName(Res1->Reg))); //Check Res2 < Res1
      AppendSeq(Res1->Instrs, GenInstr(NULL, "beq", TmpRegName(reg), "0", bRes->Label));
      ReleaseTmpReg(reg);
      break;
    case 3: //LT
      reg = AvailTmpReg();
      AppendSeq(Res1->Instrs, GenInstr(NULL, "slt", TmpRegName(reg), TmpRegName(Res1->Reg), TmpRegName(Res2->Reg))); //Check Res1 < Res2
      AppendSeq(Res1->Instrs, GenInstr(NULL, "beq", TmpRegName(reg), "0", bRes->Label));
      ReleaseTmpReg(reg);
      break;
    case 6: //NEQ
      AppendSeq(Res1->Instrs, GenInstr(NULL, "beq", TmpRegName(Res1->Reg), TmpRegName(Res2->Reg), bRes->Label));
      break;
  }
	bRes->Instrs = Res1->Instrs;
	ReleaseTmpReg(Res1->Reg);
  ReleaseTmpReg(Res2->Reg);
	free(Res1);
	free(Res2);
	return bRes;
}

struct InstrSeq * doIf(struct BExprRes * bRes, struct InstrSeq * seq) {
	struct InstrSeq * seq2;
	seq2 = AppendSeq(bRes->Instrs, seq);
	AppendSeq(seq2, GenInstr(bRes->Label, NULL, NULL, NULL, NULL));
	free(bRes);
	return seq2;
}

struct BExprRes * doNot(struct BExprRes * bRes) {
  char * label = GenLabel();
  AppendSeq(bRes->Instrs, GenInstr(NULL, "j", label, NULL, NULL));
  AppendSeq(bRes->Instrs, GenInstr(bRes->Label, NULL, NULL, NULL, NULL));
  bRes->Label = label;
  return bRes;
}

/*

extern struct InstrSeq * doIf(struct ExprRes *res1, struct ExprRes *res2, struct InstrSeq * seq) {
	struct InstrSeq *seq2;
	char * label;
	label = GenLabel();
	AppendSeq(res1->Instrs, res2->Instrs);
	AppendSeq(res1->Instrs, GenInstr(NULL, "bne", TmpRegName(res1->Reg), TmpRegName(res2->Reg), label));
	seq2 = AppendSeq(res1->Instrs, seq);
	AppendSeq(seq2, GenInstr(label, NULL, NULL, NULL, NULL));
	ReleaseTmpReg(res1->Reg);
  	ReleaseTmpReg(res2->Reg);
	free(res1);
	free(res2);
	return seq2;
}

*/
void
Finish(struct InstrSeq *Code)
{ struct InstrSeq *code;
  //struct SymEntry *entry;
    int hasMore;
  struct Attr * attr;


  code = GenInstr(NULL,".text",NULL,NULL,NULL);
  //AppendSeq(code,GenInstr(NULL,".align","2",NULL,NULL));
  AppendSeq(code,GenInstr(NULL,".globl","main",NULL,NULL));
  AppendSeq(code, GenInstr("main",NULL,NULL,NULL,NULL));
  AppendSeq(code,Code);
  AppendSeq(code, GenInstr(NULL, "li", "$v0", "10", NULL));
  AppendSeq(code, GenInstr(NULL,"syscall",NULL,NULL,NULL));
  AppendSeq(code,GenInstr(NULL,".data",NULL,NULL,NULL));
  AppendSeq(code,GenInstr(NULL,".align","4",NULL,NULL));
  AppendSeq(code,GenInstr("_nl",".asciiz","\"\\n\"",NULL,NULL));

 hasMore = startIterator(table);
 while (hasMore) {
	AppendSeq(code,GenInstr((char *) getCurrentName(table),".word","0",NULL,NULL));
    hasMore = nextEntry(table);
 }

  WriteSeq(code);

  return;
}
