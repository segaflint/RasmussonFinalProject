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
extern SymTab *intFunctionTable;
extern SymTab *voidFunctionTable;
extern SymTab *localTable;
// extern SymTab *functionParamsTable;

int errorFlag1 = 0;
int returnFlag = 0;
int funcContextFlag= 0;
int localVarCount = 0;
int functionParamsCount = 0;
char * finishFunctionLabel = "FinishFunction";

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

  res = (struct ExprRes *) malloc(sizeof(struct ExprRes));
  res->Reg = AvailTmpReg();

  if(funcContextFlag && findName(localTable, name)) {
    char offset[8];
    int varOffset = (int) getCurrentAttr(localTable);
    sprintf(offset, "%d($sp)", ((localVarCount - varOffset)-1)*4);

    res->Instrs = GenInstr(NULL,"lw",TmpRegName(res->Reg), offset, NULL);
  } else {
    if (!findName(table, name)) {
      writeIndicator(getCurrentColumnNum());
      writeMessage("Undeclared variable");
      errorFlag1++;
    }
    if(!getCurrentAttr(table)){ // attribute is null, so not an array
      res->Instrs = GenInstr(NULL,"lw",TmpRegName(res->Reg),name,NULL);
    } else  { //attribute not null, is an array
      res->Instrs = GenInstr(NULL,"la",TmpRegName(res->Reg),name,NULL);
    }
  }

  return res;
}

struct InstrSeq * doAssign(char *name, struct ExprRes * Expr) {

  struct InstrSeq *code;

  code = Expr->Instrs;
  if(funcContextFlag && findName(localTable, name)) { // local var
    char offset[8];
    int varOffset = (int) getCurrentAttr(localTable);

    sprintf(offset, "%d($sp)", ((localVarCount - varOffset)-1)*4);
    AppendSeq(code, GenInstr(NULL, "sw", TmpRegName(Expr->Reg), offset, NULL));
  } else {
    if (!findName(table, name)) {
		    writeIndicator(getCurrentColumnNum());
		    writeMessage("Undeclared variable");
        errorFlag1++;
    }
    AppendSeq(code,GenInstr(NULL,"sw",TmpRegName(Expr->Reg), name,NULL));
  }

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

struct BExprRes * doExprToBFactor(struct ExprRes * res){
  struct BExprRes * bRes;

 	bRes = (struct BExprRes *) malloc(sizeof(struct BExprRes));
	bRes->Label = GenLabel();
  AppendSeq(res->Instrs, GenInstr(NULL, "beq", TmpRegName(res->Reg), "0", bRes->Label));

  ReleaseTmpReg(res->Reg);
  bRes->Instrs = res->Instrs;
  free(res);

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

struct InstrSeq * doWhile(struct BExprRes * bRes, struct InstrSeq * seq) {
  struct InstrSeq *returnSeq;
  char * label;
  label = GenLabel();
  returnSeq = AppendSeq(GenInstr(label, NULL, NULL, NULL, NULL), bRes->Instrs);
  AppendSeq(returnSeq, seq);
  AppendSeq(returnSeq, GenInstr(NULL, "j", label, NULL, NULL));
  AppendSeq(returnSeq, GenInstr(bRes->Label, NULL, NULL, NULL, NULL));

  return returnSeq;
}

struct InstrSeq * doFor(char * initVar, struct ExprRes * initExpr, struct BExprRes * bRes, char * updateVar, struct ExprRes * updateExprRes , struct InstrSeq * seq){
  struct InstrSeq * returnSeq;
  char * label;

  label = GenLabel();
  returnSeq = AppendSeq(doAssign(initVar, initExpr), GenInstr(label, NULL, NULL, NULL, NULL));
  AppendSeq(returnSeq, bRes->Instrs);
  AppendSeq(returnSeq, seq);
  AppendSeq(returnSeq, doAssign(updateVar, updateExprRes));
  AppendSeq(returnSeq, GenInstr(NULL, "j", label, NULL, NULL));
  AppendSeq(returnSeq, GenInstr(bRes->Label, NULL, NULL, NULL, NULL));

  return returnSeq;
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

  // AppendSeq(code,GenInstr(NULL,"la","$a0","_nl",NULL));
  // AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

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

  // AppendSeq(code,GenInstr(NULL,"li","$v0","4",NULL));
  // AppendSeq(code,GenInstr(NULL,"la","$a0","_nl",NULL));
  // AppendSeq(code,GenInstr(NULL,"syscall",NULL,NULL,NULL));

  ReleaseTmpReg(Expr->Reg);
  free(Expr);

  return code;
}

// struct InstrSeq * doInputOnId(char * name) {
//   struct InstrSeq * code;
//
//   if (!findName(table, name)) {
//     writeIndicator(getCurrentColumnNum());
//     writeMessage("A variable in the read list is undeclared");
//     errorFlag1++;
//   }
//
//   code = GenInstr(NULL, "li", "$v0", "5", NULL);
//   AppendSeq(code, GenInstr(NULL, "syscall", NULL, NULL, NULL));
//   AppendSeq(code,GenInstr(NULL,"sw","$v0", name, NULL));
//
//   return code;
// }

struct InstrSeq * doInputOnList(struct IdList * list ) {
  struct InstrSeq * code;
  struct IdList * currentList;
  struct IdList * oldList;
  struct ExprRes * assignmentRes;

  currentList = list;
  code = GenInstr(NULL, NULL, NULL, NULL, NULL);

  while (currentList) {

    assignmentRes = (struct ExprRes *) malloc(sizeof(struct ExprRes));
    assignmentRes->Reg = AvailTmpReg();
    assignmentRes->Instrs = GenInstr(NULL, "li", "$v0", "5", NULL);
    AppendSeq(assignmentRes->Instrs, GenInstr(NULL, "syscall", NULL, NULL, NULL));
    AppendSeq(assignmentRes->Instrs, GenInstr(NULL, "addi", TmpRegName(assignmentRes->Reg), "$v0", "0"));

    if( funcContextFlag && findName(localTable, currentList->TheEntry->name)) { // context is local
      if(currentList->ArrayIndexRes) { // this is an array
        AppendSeq(code, doArrayAssign(getCurrentName(localTable), currentList->ArrayIndexRes, assignmentRes));
      } else { // local var
        AppendSeq(code, doAssign(getCurrentName(localTable), assignmentRes));
      }
    } else { // global context
      if(!findName(table, currentList->TheEntry->name)) {
        writeIndicator(getCurrentColumnNum());
        writeMessage("No array by such a name exists");
        errorFlag1++;
      }
      if(currentList->ArrayIndexRes) { // global array
        AppendSeq(code, doArrayAssign(getCurrentName(table), currentList->ArrayIndexRes, assignmentRes));
      } else { // global var
        AppendSeq(code, doAssign(getCurrentName(table), assignmentRes));
      }
    }

    oldList = currentList;
    currentList = currentList->Next;

    free(oldList);
  }

  return code;
}

struct IdList * doArrayToIdList(char * name, struct ExprRes * indexRes ){
  struct IdList * newIdList = (struct IdList * ) malloc(sizeof(struct IdList));

  if(funcContextFlag && findName(localTable, name)) { //name is found in locals
    newIdList->TheEntry = localTable->current; // attribute on local is a variables index in local variables

  } else {
    if(!findName(table, name)) {
      writeIndicator(getCurrentColumnNum());
      writeMessage("No array by such a name exists");
      errorFlag1++;
    }

    newIdList->TheEntry = table->current; //attribute on global is an array size
  }
  newIdList->Next = NULL;
  newIdList->ArrayIndexRes = indexRes;

  return newIdList;
}

struct IdList * doAppendIdentList(struct IdList * IdentList1, struct IdList * IdentList2) {
  struct IdList * currentList = IdentList1;

  while(currentList->Next) currentList = currentList->Next;

  currentList->Next = IdentList2;

  return currentList;
}

struct IdList * doIdToIdList(char * name){
  struct IdList * newIdList = (struct IdList * ) malloc(sizeof(struct IdList));

  if(funcContextFlag && findName(localTable, name)) { //name found in locals
    newIdList->TheEntry = localTable->current;
  } else  {
    if (!findName(table, name)) {
      writeIndicator(getCurrentColumnNum());
      writeMessage("A variable in the read list undeclared");
      errorFlag1++;
    }
    newIdList->TheEntry = table->current; //attribute on global is an array size

  }

  newIdList->Next = NULL;
  newIdList->ArrayIndexRes = NULL;
  // IdList1 = (struct IdList * ) malloc(sizeof(struct IdList));
  // IdList1->TheEntry = table->current;
  //
  // if(Id2 != NULL) {
  //   if (!findName(table, Id2)) {
  //     writeIndicator(getCurrentColumnNum());
  //     writeMessage("Variable 2 in the read list undeclared");
  //     errorFlag1++;
  //   }
  //   IdList2 = (struct IdList * ) malloc(sizeof(struct IdList));
  //   IdList2->TheEntry = table->current;
  //
  //   IdList1->Next = IdList2;
  //   IdList2->Next = NULL;
  // } else {
  //   IdList1->Next = NULL;
  // }

  return newIdList;
}

/* END INTEGER I/O */

/* BEGIN ARRAYS */
struct ExprRes * doArrayRval(char * name, struct ExprRes * res){
  int valueReg;
  int exprReg;
  char * strAddr = (char *) malloc(sizeof(char)*7);

  valueReg = AvailTmpReg();
  exprReg = res->Reg;

  if( funcContextFlag && findName(localTable, name)) {
    char offset[8];
    int varOffset = (int) getCurrentAttr(localTable);

    if(varOffset >= functionParamsCount) { // locally allocated variable array
      sprintf(offset, "%d", ((localVarCount - varOffset)-1)*4);
      AppendSeq(res->Instrs, GenInstr(NULL, "addi", TmpRegName(valueReg), "$sp", offset));
    } else { // globally allocated

      sprintf(offset, "%d($sp)", ((localVarCount - varOffset)-1)*4);
      AppendSeq(res->Instrs, GenInstr(NULL, "lw", TmpRegName(valueReg), offset ,NULL )); // get the address of whatever array from the stack
    }

  } else {
    if (!findName(table, name)) {
      writeIndicator(getCurrentColumnNum());
      writeMessage("Undeclared array variable");
      errorFlag1++;
    } else if ( getCurrentAttr(table) == NULL){
      writeIndicator(getCurrentColumnNum());
      writeMessage("This variable cannot be referenced as an array");
      errorFlag1++;
    } else {}

    AppendSeq(res->Instrs, GenInstr(NULL, "la", TmpRegName(valueReg), name, NULL));
  }

  sprintf(strAddr, "0(%s)",TmpRegName(valueReg));

  AppendSeq(res->Instrs, GenInstr(NULL, "sll", TmpRegName(exprReg), TmpRegName(exprReg), "2"));
  AppendSeq(res->Instrs, GenInstr(NULL, "add", TmpRegName(valueReg), TmpRegName(valueReg), TmpRegName(exprReg)));
  AppendSeq(res->Instrs, GenInstr(NULL, "lw", TmpRegName(valueReg), strAddr, NULL));

  ReleaseTmpReg(exprReg);
  res->Reg = valueReg;

  return res;
}

struct InstrSeq * doArrayAssign(char * name, struct ExprRes * arrayIndexRes, struct ExprRes * assignmentRes) {
  struct InstrSeq * code;
  int addressReg = AvailTmpReg();
  char * strAddr = (char *) malloc(sizeof(char) * 7);
  int stackAddress;

  code = arrayIndexRes->Instrs;
  AppendSeq(code, assignmentRes->Instrs);

  if(funcContextFlag && findName(localTable, name)) {
    char offset[8];
    int varOffset = (int) getCurrentAttr(localTable);

    if(varOffset >= functionParamsCount) { // locally allocated variable array
      sprintf(offset, "%d", ((localVarCount - varOffset)-1)*4);
      AppendSeq(code, GenInstr(NULL, "addi", TmpRegName(addressReg), "$sp", offset));
    } else { // globally allocated

      sprintf(offset, "%d($sp)", ((localVarCount - varOffset)-1)*4);
      AppendSeq(code, GenInstr(NULL, "lw", TmpRegName(addressReg), offset ,NULL )); // get the address of whatever array from the stack
    }

  } else {
    if (!findName(table, name)) {
      writeIndicator(getCurrentColumnNum());
      writeMessage("Undeclared array variable");
      errorFlag1++;
    } else if ( getCurrentAttr(table) == NULL){
      writeIndicator(getCurrentColumnNum());
      writeMessage("This variable cannot be referenced as an array");
      errorFlag1++;
    } else {}

    AppendSeq(code, GenInstr(NULL, "la", TmpRegName(addressReg), name ,NULL ));
  }


  sprintf(strAddr, "0(%s)", TmpRegName(addressReg));

  AppendSeq(code, GenInstr(NULL, "sll", TmpRegName(arrayIndexRes->Reg), TmpRegName(arrayIndexRes->Reg), "2"));
  AppendSeq(code, GenInstr(NULL, "add", TmpRegName(addressReg), TmpRegName(addressReg), TmpRegName(arrayIndexRes->Reg)));
  AppendSeq(code, GenInstr(NULL, "sw", TmpRegName(assignmentRes->Reg), strAddr, NULL));

  ReleaseTmpReg(addressReg);
  ReleaseTmpReg(arrayIndexRes->Reg);
  ReleaseTmpReg(assignmentRes->Reg);
  free(arrayIndexRes);
  free(assignmentRes);

  return code;
}

void defineAndAppendFunction(void * table, char * functionName, struct InstrSeq * codeBody ){
  struct InstrSeq * code = (struct InstrSeq *) malloc(sizeof(struct InstrSeq));
  int pureLocalsCount;
  char offset[8];
  char shrinkAmount[8];

  findName((SymTab *)table, functionName);
  pureLocalsCount = localVarCount - functionParamsCount;
  sprintf(offset, "%d", -(pureLocalsCount*4));
  AppendSeq(code, GenInstr(NULL, "addi", "$sp", "$sp", offset)); // decrease stack pointer size of num locals and one more for $ra
  for(int i = 0; i < pureLocalsCount; i++) {// for the count of , initialize them to zero
    sprintf(offset, "%d($sp)", i*4);
    AppendSeq(code, GenInstr(NULL, "sw", "$zero", offset, NULL));
  }

  AppendSeq(code, codeBody);

  sprintf(shrinkAmount, "%d", (localVarCount*4));
  AppendSeq(code, GenInstr( NULL, "addi", "$sp", "$sp", shrinkAmount));
  AppendSeq(code, GenInstr(NULL, "j", finishFunctionLabel, NULL, NULL));

  setCurrentAttr((SymTab *)table, (void*) code);

}

struct InstrSeq * doReturnInt(struct ExprRes * res){
  char shrinkAmount[8];

  AppendSeq(res->Instrs, GenInstr(NULL, "addi", "$v0", TmpRegName(res->Reg), "0"));
  sprintf(shrinkAmount, "%d", (localVarCount*4)); // shrink stack from locals, params, and $ra
  AppendSeq(res->Instrs, GenInstr( NULL, "addi", "$sp", "$sp", shrinkAmount));
  AppendSeq(res->Instrs, GenInstr(NULL, "j", finishFunctionLabel, NULL, NULL));


  ReleaseTmpReg(res->Reg);
  returnFlag--;
  return res->Instrs;
}

struct InstrSeq * doReturn(){
  struct InstrSeq * code;
  char shrinkAmount[8];
  sprintf(shrinkAmount, "%d", (localVarCount*4));
  code = GenInstr( NULL, "addi", "$sp", "$sp", shrinkAmount);
  AppendSeq(code, GenInstr(NULL, "j", finishFunctionLabel, NULL, NULL));

  return code;
}

void checkReturn() {
  if(returnFlag > 0){
    writeIndicator(getCurrentColumnNum());
		writeMessage("return statement expected");
    errorFlag1++;
  }
  returnFlag = 0;
}

struct InstrSeq * doVoidFunctionCall(char * name, struct ExprResList * ExprList) {
  struct InstrSeq * code;

  if (!findName(voidFunctionTable, name)) {
   writeIndicator(getCurrentColumnNum());
   writeMessage("No such void function exists");
   errorFlag1++;
  }

  code = saveRAAndSeq();
  AppendSeq(code, pushParameters(ExprList));
  AppendSeq(code, GenInstr(NULL, "jal", name, NULL, NULL));
  AppendSeq(code, restoreRAAndSeq());
  return code;
}

struct ExprRes * doIntFunctionCall(char * name, struct ExprResList * ExprList) {
  struct ExprRes * res = (struct ExprRes *) malloc(sizeof(struct ExprRes));
  int reg;

  if (!findName(intFunctionTable, name)) {
   writeIndicator(getCurrentColumnNum());
   writeMessage("No such int function exists");
   errorFlag1++;
  }

  // res->Instrs = GenInstr(NULL, "addi", "$sp", "$sp", "-4");
  // AppendSeq(res->Instrs, GenInstr(NULL, "sw", "$ra", "0($sp)", NULL));
  // AppendSeq(res->Instrs, SaveSeq());
  res->Instrs = saveRAAndSeq();
  AppendSeq(res->Instrs, pushParameters(ExprList));
  AppendSeq(res->Instrs, GenInstr(NULL, "jal", name, NULL, NULL));
  AppendSeq(res->Instrs, restoreRAAndSeq());
  // AppendSeq(res->Instrs, RestoreSeq());
  reg = AvailTmpReg();
  AppendSeq(res->Instrs, GenInstr(NULL, "addi", TmpRegName(reg), "$v0", "0"));
  // AppendSeq(res->Instrs, GenInstr(NULL, "lw", "$ra", "0($sp)", NULL));
  // AppendSeq(res->Instrs, GenInstr(NULL, "addi", "$sp", "$sp", "4"));

  res->Reg = reg;

  return res;
}

struct InstrSeq * saveRAAndSeq() {
  struct InstrSeq * code;
  code = GenInstr(NULL, "addi", "$sp", "$sp", "-4");
  AppendSeq(code, GenInstr(NULL, "sw", "$ra", "0($sp)", NULL));
  AppendSeq(code, SaveSeq());

  return code;
}

struct InstrSeq * restoreRAAndSeq() {
  struct InstrSeq * code;
  code = RestoreSeq();
  AppendSeq(code, GenInstr(NULL, "lw", "$ra", "0($sp)", NULL));
  AppendSeq(code, GenInstr(NULL, "addi", "$sp", "$sp", "4"));

  return code;
}

struct InstrSeq * pushParameters(struct ExprResList * ExprList) {
  struct InstrSeq * code = NULL;
  if(ExprList) {
    struct ExprResList* currentExprRes = ExprList;
    struct ExprResList* oldExprResList;
    int reg;

    code = GenInstr(NULL, NULL, NULL, NULL, NULL);

    while(currentExprRes) {

      AppendSeq(code, currentExprRes->Expr->Instrs);
      AppendSeq(code, GenInstr(NULL, "addi", "$sp", "$sp", "-4"));
      AppendSeq(code, GenInstr(NULL, "sw", TmpRegName(currentExprRes->Expr->Reg), "0($sp)", NULL));

      ReleaseTmpReg(currentExprRes->Expr->Reg);
      free(currentExprRes->Expr);

      oldExprResList = currentExprRes;
      currentExprRes = ExprList->Next;
      free(oldExprResList);
    }
  }

  return code;
}

struct ExprResList * doAppendExprListToExprList(struct ExprResList * list1, struct ExprResList * list2) {
  list1->Next = list2;
  return list1;
}

struct ExprResList * doOneExprToExprList(struct ExprRes * res) {
  struct ExprResList * resList = (struct ExprResList *) malloc(sizeof(struct ExprResList));

  resList->Expr = res;
  resList->arrayName = NULL;
  resList->Next = NULL;

  return resList;
}

struct ExprResList * doArrayNameToExprList(char * name) {
  struct ExprResList * resList = (struct ExprResList *) malloc(sizeof(struct ExprResList));

  resList->Expr = NULL;
  resList->arrayName = name;
  resList->Next = NULL;

  return resList;
}

void insertScopedName(char * name) {
  enterName(localTable, name);
  setCurrentAttr(localTable, (void *) localVarCount);
  localVarCount++;

}

void insertArrayScopedName(char * name, int arraySize) {
  enterName(localTable, name);
  if(arraySize < 1) {
    writeIndicator(getCurrentColumnNum());
    writeMessage("Cannot declare an array with a size smaller than 1");
    errorFlag1++;
  }

  for (int i = 0; i < arraySize; i++){
    setCurrentAttr(localTable, (void *) localVarCount);
    localVarCount++;
  }
}

void cleanUpFunction() {
  functionParamsCount = 0;
  localVarCount = 0;
  destroySymTab(localTable);
  localTable = NULL;
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
Finish(struct InstrSeq *Code){
  struct InstrSeq *code;
  //struct SymEntry *entry;
  int hasMore;
  int arraySize;
  int arraySpace;
  char * arraySpaceString = (char *) malloc(sizeof(char)*11);
  struct Attr * attr;

  code = GenInstr(NULL,".text",NULL,NULL,NULL);
  //AppendSeq(code,GenInstr(NULL,".align","2",NULL,NULL));
  AppendSeq(code,GenInstr(NULL,".globl","main",NULL,NULL));
  AppendSeq(code, GenInstr("main",NULL,NULL,NULL,NULL));
  AppendSeq(code,Code);
  AppendSeq(code, GenInstr(NULL, "li", "$v0", "10", NULL));
  AppendSeq(code, GenInstr(NULL,"syscall",NULL,NULL,NULL));

  generateTableInstructions(code);

  appendFinishedFunction(code);

  AppendSeq(code,GenInstr(NULL,".data",NULL,NULL,NULL));
  AppendSeq(code,GenInstr(NULL,".align","4",NULL,NULL));
  AppendSeq(code, GenInstr("_space", ".asciiz","\" \"", NULL, NULL));
  AppendSeq(code,GenInstr("_nl",".asciiz","\"\\n\"",NULL,NULL));

  hasMore = startIterator(table);

  while (hasMore) {
    if(!getCurrentAttr(table)) { // table has an attribute that is NULL; this is an int variable
      AppendSeq(code,GenInstr((char *) getCurrentName(table),".word","0",NULL,NULL));

    } else { // table has an attribute that isnt NULL; array
      arraySize = (int) getCurrentAttr(table);
      arraySpace = arraySize * 4;
      sprintf(arraySpaceString, "%d", arraySpace);
      AppendSeq(code, GenInstr((char *) getCurrentName(table), ".space", arraySpaceString, NULL, NULL ));
    }
    hasMore = nextEntry(table);
  }

  if(errorFlag1) {
    printf("You have existing errors, please see listing file\n" );
  } else  {
    WriteSeq(code);

  }

  return;
}

void generateTableInstructions(struct InstrSeq * code) {
  int hasMore;
  hasMore = startIterator(intFunctionTable);

  while (hasMore) {
    AppendSeq(code, GenInstr( getCurrentName(intFunctionTable), NULL, NULL, NULL, NULL ));
    AppendSeq(code, (struct InstrSeq *) getCurrentAttr(intFunctionTable));

    hasMore = nextEntry(intFunctionTable);
  }

  hasMore = startIterator(voidFunctionTable);

  while(hasMore) {
    AppendSeq(code, GenInstr(getCurrentName(voidFunctionTable), NULL, NULL, NULL, NULL));
    AppendSeq(code, (struct InstrSeq *) getCurrentAttr(voidFunctionTable));

    hasMore = nextEntry(voidFunctionTable);
  }
}

void appendFinishedFunction(struct InstrSeq * code){
  AppendSeq(code, GenInstr(finishFunctionLabel, NULL, NULL, NULL, NULL));
  AppendSeq(code, GenInstr(NULL, "jr", "$ra", NULL, NULL));
}
