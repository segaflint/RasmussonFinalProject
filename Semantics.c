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

int errorFlag1 = 0;


/* Semantics support routines */

/* BEGIN INTEGER EXPRESSIONS */

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
    errorFlag1++;
   }
  res = (struct ExprRes *) malloc(sizeof(struct ExprRes));
  res->Reg = AvailTmpReg();
  res->Instrs = GenInstr(NULL,"lw",TmpRegName(res->Reg),name,NULL);

  return res;
}

struct InstrSeq * doAssign(char *name, struct ExprRes * Expr) {

  struct InstrSeq *code;


   if (!findName(table, name)) {
		writeIndicator(getCurrentColumnNum());
		writeMessage("Undeclared variable");
    errorFlag1++;
   }

  code = Expr->Instrs;

  AppendSeq(code,GenInstr(NULL,"sw",TmpRegName(Expr->Reg), name,NULL));

  ReleaseTmpReg(Expr->Reg);
  free(Expr);

  return code;
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

struct InstrSeq * doIfElse(struct BExprRes * bRes, struct InstrSeq * ifSeq, struct InstrSeq * elseSeq) {
  struct InstrSeq * code;
  char* endLabel;
  endLabel = GenLabel();
  code = AppendSeq(bRes->Instrs, ifSeq); // boolean clause and if body
  AppendSeq(code, GenInstr(NULL, "j", endLabel, NULL, NULL));
  AppendSeq(code, GenInstr(bRes->Label, NULL, NULL, NULL, NULL)); // run else
  AppendSeq(code, elseSeq);
  AppendSeq(code, GenInstr(endLabel, NULL, NULL, NULL, NULL));

  return code;
}

struct BExprRes * doNot(struct BExprRes * bRes) {
  char * label = GenLabel();
  AppendSeq(bRes->Instrs, GenInstr(NULL, "j", label, NULL, NULL));
  AppendSeq(bRes->Instrs, GenInstr(bRes->Label, NULL, NULL, NULL, NULL));
  bRes->Label = label;
  return bRes;
}

struct BExprRes * doBAND(struct BExprRes * bRes1, struct BExprRes * bRes2) {
  char * checkbRes2Label = GenLabel();

  AppendSeq(bRes1->Instrs, GenInstr(NULL, "j", checkbRes2Label, NULL, NULL)); // jump to second check
  AppendSeq(bRes1->Instrs, GenInstr(bRes1->Label, "j", bRes2->Label, NULL, NULL)); // jump out of code, 1st condition failed so we will go to end.
  AppendSeq(bRes1->Instrs, GenInstr(checkbRes2Label, NULL, NULL, NULL, NULL));
  AppendSeq(bRes1->Instrs, bRes2->Instrs); // second check

  bRes1->Label = bRes2->Label;
  free(bRes2);
  return bRes1;

}

struct BExprRes * doBOR(struct BExprRes * bRes1, struct BExprRes * bRes2) {
  char * bodyLabel = GenLabel();

  AppendSeq(bRes1->Instrs, GenInstr(NULL, "j", bodyLabel, NULL, NULL)); // first condition passed; move to code body
  AppendSeq(bRes1->Instrs, GenInstr(bRes1->Label, NULL, NULL, NULL, NULL)); // first condition failed; check second
  AppendSeq(bRes1->Instrs, bRes2->Instrs); // second conditional
  AppendSeq(bRes1->Instrs, GenInstr(bodyLabel, NULL, NULL, NULL, NULL)); // reached the body; either first was true and jumped here,
  // or first was false, second was true and fell through
  bRes1->Label = bRes2->Label;
  free(bRes2);
  return bRes1;
}

/* END INTEGER EXPRESSIONS */

/* BEGIN INTEGER I/O */

struct InstrSeq * doPrintExprList(struct ExprResList * exprList){
  struct InstrSeq * code;
  struct ExprResList * currentList;
  struct ExprResList * oldList;

  currentList = exprList;
  code = GenInstr(NULL, NULL, NULL, NULL, NULL);
  while(currentList != NULL) {

    AppendSeq(code, currentList->Expr->Instrs);
    AppendSeq(code,GenInstr(NULL,"li","$v0","1",NULL));
    AppendSeq(code,GenInstr(NULL,"move","$a0",TmpRegName(currentList->Expr->Reg), NULL));
    AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

    AppendSeq(code,GenInstr(NULL,"li","$v0","4",NULL));
    AppendSeq(code,GenInstr(NULL,"la","$a0","_space",NULL));
    AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

    oldList = currentList;
    currentList = currentList->Next;

    ReleaseTmpReg(oldList->Expr->Reg);
    free(oldList->Expr);
    free(oldList);
  }

  AppendSeq(code,GenInstr(NULL,"la","$a0","_nl",NULL));
  AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

  return code;
}

struct ExprResList * doAppendExprList(struct ExprResList * resList, struct ExprRes * res){
  struct ExprResList * newResList;
  struct ExprResList * currentList;
  newResList = (struct ExprResList * ) malloc(sizeof(struct ExprResList));

  newResList->Expr = res;
  newResList->Next = NULL;
  currentList = resList;
  while(currentList->Next) currentList = currentList->Next;
  currentList->Next = newResList;

  return resList;
}

struct ExprResList * doExprToExprList(struct ExprRes * res1, struct ExprRes * res2){
  struct ExprResList * resList1;
  struct ExprResList * resList2;

  resList1 = (struct ExprResList * ) malloc(sizeof(struct ExprResList));
  resList2 = (struct ExprResList * ) malloc(sizeof(struct ExprResList));
  resList1->Expr = res1;
  resList1->Next = resList2;
  resList2->Expr = res2;
  resList2->Next = NULL;

  return resList1;
}

struct InstrSeq * doPrintline(){
  struct InstrSeq * code;
  code = GenInstr(NULL,"li","$v0","4",NULL);
  AppendSeq(code,GenInstr(NULL,"la","$a0","_nl",NULL));
  AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

  return code;
}

struct InstrSeq * doPrintSpaces(struct ExprRes * res){
  //res->Reg holds the number of spaces to print

  struct InstrSeq * code;
  char * loopLabel;
  char * finishedLabel;
  int iteratorReg;
  int tempReg;

  code = res->Instrs;
  loopLabel = GenLabel();
  finishedLabel = GenLabel();
  iteratorReg = AvailTmpReg();
  tempReg = AvailTmpReg();

  AppendSeq(code,GenInstr(NULL,"li","$v0","4",NULL));
  AppendSeq(code,GenInstr(NULL,"la","$a0","_space",NULL)); //set up space to be printed
  AppendSeq(code,GenInstr(NULL,"li",TmpRegName(iteratorReg), "0", NULL)); // i = 0

  AppendSeq(code, GenInstr(loopLabel, "slt", TmpRegName(tempReg), TmpRegName(iteratorReg), TmpRegName(res->Reg) ));
  AppendSeq(code, GenInstr(NULL, "bne", TmpRegName(tempReg), "1", finishedLabel));
  AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));//print space
  AppendSeq(code, GenInstr(NULL, "addi", TmpRegName(iteratorReg), TmpRegName(iteratorReg), "1")); //i++
  AppendSeq(code, GenInstr(NULL, "j", loopLabel, NULL, NULL)); // jump to loop beginning
  AppendSeq(code, GenInstr(finishedLabel, NULL, NULL, NULL, NULL)); //out of loop

  ReleaseTmpReg(iteratorReg);
  ReleaseTmpReg(tempReg);
  free(res);

  return code;

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

struct InstrSeq * doInputOnId(char * name) {
  struct InstrSeq * code;

  if (!findName(table, name)) {
    writeIndicator(getCurrentColumnNum());
    writeMessage("A variable in the read list is undeclared");
    errorFlag1++;
  }

  code = GenInstr(NULL, "li", "$v0", "5", NULL);
  AppendSeq(code, GenInstr(NULL, "syscall", NULL, NULL, NULL));
  AppendSeq(code,GenInstr(NULL,"sw","$v0", name, NULL));

  return code;
}

struct InstrSeq * doInputOnList(struct IdList * IdList ) {
  struct InstrSeq * code;
  struct IdList * currentList;
  struct IdList * oldList;

  currentList = IdList;
  code = GenInstr(NULL, NULL, NULL, NULL, NULL);

  while (currentList) {
    AppendSeq(code, GenInstr(NULL, "li", "$v0", "5", NULL));
    AppendSeq(code, GenInstr(NULL, "syscall", NULL, NULL, NULL));
    AppendSeq(code,GenInstr(NULL,"sw","$v0", currentList->TheEntry->name, NULL));

    oldList = currentList;
    currentList = currentList->Next;

    free(oldList);
  }

  return code;
}

struct IdList * doAppendIdentList(struct IdList * IdentList, char * variableName) {
  struct IdList * newIdList;
  struct IdList * currentList;
  newIdList = (struct IdList * ) malloc(sizeof(struct IdList));

  currentList = IdentList;
  while(currentList->Next) currentList = currentList->Next;

  if (!findName(table, variableName)) {
    writeIndicator(getCurrentColumnNum());
    writeMessage("A variable in the read list is undeclared");
    errorFlag1++;
  }
  newIdList = (struct IdList * ) malloc(sizeof(struct IdList));
  newIdList->TheEntry = table->current;
  newIdList->Next = NULL;
  currentList->Next = newIdList;

  return IdentList;
}

struct IdList * doIdToIdList(char * Id1, char * Id2){
  struct IdList * IdList1;
  struct IdList * IdList2;

  if (!findName(table, Id1)) {
    writeIndicator(getCurrentColumnNum());
    writeMessage("Variable 1 in the read list undeclared");
    errorFlag1++;
  }
  IdList1 = (struct IdList * ) malloc(sizeof(struct IdList));
  IdList1->TheEntry = table->current;

  if(Id2 != NULL) {
    if (!findName(table, Id2)) {
      writeIndicator(getCurrentColumnNum());
      writeMessage("Variable 2 in the read list undeclared");
      errorFlag1++;
    }
    IdList2 = (struct IdList * ) malloc(sizeof(struct IdList));
    IdList2->TheEntry = table->current;

    IdList1->Next = IdList2;
    IdList2->Next = NULL;
  } else {
    IdList1->Next = NULL;
  }

  return IdList1;
}

/* END INTEGER I/O */

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
  AppendSeq(code, GenInstr("_space", ".asciiz","\" \"", NULL, NULL));
  AppendSeq(code,GenInstr("_nl",".asciiz","\"\\n\"",NULL,NULL));

 hasMore = startIterator(table);
 while (hasMore) {
	AppendSeq(code,GenInstr((char *) getCurrentName(table),".word","0",NULL,NULL));
    hasMore = nextEntry(table);
 }

  WriteSeq(code);

  return;
}
