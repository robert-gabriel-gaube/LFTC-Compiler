#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad.h"
#include "at.h"
#include "gc.h"
#include "parser.h"
#include "utils.h"
#include "vm.h"

Token *iTk = NULL;        // the iterator in the tokens list
Token *consumedTk = NULL; // the last consumed token

Symbol *owner = NULL;


typedef struct {
  Instr *startInstr;
  Token *guardedToken;
} Guard;

Guard makeGuard() {
  Guard guard;

  guard.startInstr=owner?lastInstr(owner->fn.instr):NULL;
  guard.guardedToken = iTk;

  return guard; 
}
void restoreGuard(Guard g) { 
  iTk = g.guardedToken;
   if(owner) {
    delInstrAfter(g.startInstr);
   }
}

int inStruct = 0;

void tkerr(const char *fmt, ...) {
  if (consumedTk == NULL) {
    fprintf(stderr, "error in line %d: ", iTk->line);
  } else {
    fprintf(stderr, "error in line %d: ", consumedTk->line);
  }

  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);

  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

bool consume(int code) {
  if (iTk->code == code) {
    consumedTk = iTk;
    iTk = iTk->next;
    return true;
  }
  return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t) {
  t->n = -1;
  if (consume(TYPE_INT)) {
    t->tb = TB_INT;
    return true;
  }
  if (consume(TYPE_DOUBLE)) {
    t->tb = TB_DOUBLE;
    return true;
  }
  if (consume(TYPE_CHAR)) {
    t->tb = TB_CHAR;
    return true;
  }
  if (consume(STRUCT)) {
    if (consume(ID)) {
      t->tb = TB_STRUCT;
      t->s = findSymbol(consumedTk->text);
      if (!t->s) {
        tkerr("Undefined structure: %s", consumedTk->text);
      }
      return true;
    } else {
      tkerr("Missing struct name in type definition");
    }
  }
  return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t) {
  Guard guard = makeGuard();

  if (consume(LBRACKET)) {
    if (consume(INT)) {
      t->n = consumedTk->i;
    } else {
      t->n = 0;
    }
    if (consume(RBRACKET)) {
      PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found arrayDecl");
      return true;
    }
  }

  restoreGuard(guard);
  return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef() {
  Guard guard = makeGuard();
  Type t;

  if (typeBase(&t)) {
    if (consume(ID)) {
      Token *tkName = consumedTk;
      if (arrayDecl(&t)) {
        PRINT_DEBUG(MEDIUM_VERBOSITY,
                    "[AD] Found var definition with array of size n = %d", t.n);
        if (t.n == 0) {
          tkerr("A vector variable must have a specified dimension");
        }
      }
      if (consume(SEMICOLON)) {
        PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found varDef");
        Symbol *var = findSymbolInDomain(symTable, tkName->text);
        if (var) {
          tkerr("Symbol redefinition: %s", tkName->text);
        }
        var = newSymbol(tkName->text, SK_VAR);
        var->type = t;
        var->owner = owner;
        addSymbolToDomain(symTable, var);
        if (owner) {
          switch (owner->kind) {
          case SK_FN:
            var->varIdx = symbolsLen(owner->fn.locals);
            addSymbolToList(&owner->fn.locals, dupSymbol(var));
            break;
          case SK_STRUCT:
            var->varIdx = typeSize(&owner->type);
            addSymbolToList(&owner->structMembers, dupSymbol(var));
            break;
          }
        } else {
          var->varMem = safeAlloc(typeSize(&t));
        }
        return true;
      } else {
        tkerr("Missing ';' after variable definition");
      }
    } else {
      tkerr("Missing varriable name");
    }
  } else if (inStruct) {
    if (consume(ID)) {
      arrayDecl(&t);
      if (consume(SEMICOLON)) {
        tkerr("Missing type in variable definition inside struct");
      }
    }
  }

  restoreGuard(guard);
  return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef() {
  Guard guard = makeGuard();
  inStruct = 1;

  if (consume(STRUCT)) {
    if (consume(ID)) {
      Token *tkName = consumedTk;
      if (consume(LACC)) {
        Symbol *s = findSymbolInDomain(symTable, tkName->text);
        if (s) {
          tkerr("symbol redefinition: %s", tkName->text);
        }
        s = addSymbolToDomain(symTable, newSymbol(tkName->text, SK_STRUCT));
        s->type.tb = TB_STRUCT;
        s->type.s = s;
        s->type.n = -1;
        pushDomain();
        owner = s;
        while (varDef())
          ;
        if (consume(RACC)) {
          if (consume(SEMICOLON)) {
            PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found structDef");
            inStruct = 0;
            owner = NULL;
            // showDomain(symTable, tkName->text);
            dropDomain();
            return true;
          } else {
            tkerr("Missing ';' in struct definiton");
          }
        } else {
          tkerr("Missing '}' in struct definition");
        }
      }
    } else {
      tkerr("Missing struct name in definition");
    }
  }

  inStruct = 0;

  restoreGuard(guard);
  return false;
}

bool expr();

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//              | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Ret *r) {
  Guard guard = makeGuard();

  // Function call or simple ID
  if (consume(ID)) {
    Token *tkName = consumedTk;
    Symbol *s = findSymbol(tkName->text);
    if (!s) {
      tkerr("undefined id: %s", tkName->text);
    }
    if (consume(LPAR)) {
      if (s->kind != SK_FN)
        tkerr("only a function can be called");
      Ret rArg;
      Symbol *param = s->fn.params;
      if (expr(&rArg)) {
        if (!param) {
          tkerr("too many arguments in function call");
        }
        if (!convTo(&rArg.type, &param->type)) {
          tkerr("in call, cannot convert the argument type to the parameter "
                "type");
        }
        addRVal(&owner->fn.instr, rArg.lval, &rArg.type);
        insertConvIfNeeded(lastInstr(owner->fn.instr), &rArg.type,
                           &param->type);
        param = param->next;
        while (consume(COMMA)) {
          if (expr(&rArg)) {
            if (!param) {
              tkerr("too many arguments in function call");
            }
            if (!convTo(&rArg.type, &param->type)) {
              tkerr("in call, cannot convert the argument type to the "
                    "parameter type");
            }
            addRVal(&owner->fn.instr, rArg.lval, &rArg.type);
            insertConvIfNeeded(lastInstr(owner->fn.instr), &rArg.type,
                               &param->type);
            param = param->next;
          } else {
            tkerr("Expected expression after ','");
          }
        }
      }
      if (consume(RPAR)) {
        PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - function call");
        if (param) {
          tkerr("too few arguments in function call");
        }
        if (s->fn.extFnPtr) {
          addInstr(&owner->fn.instr, OP_CALL_EXT)->arg.extFnPtr =
              s->fn.extFnPtr;
        } else {
          addInstr(&owner->fn.instr, OP_CALL)->arg.instr = s->fn.instr;
        }
        *r = (Ret){s->type, false, true};
        return true;
      } else {
        tkerr("Missing ')' in function call");
      }
    }
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - simple ID");
    if (s->kind == SK_FN) {
      tkerr("a function can only be called");
    }
    if (s->kind == SK_VAR) {
      if (s->owner == NULL) { // global variables
        addInstr(&owner->fn.instr, OP_ADDR)->arg.p = s->varMem;
      } else { // local variables
        switch (s->type.tb) {
        case TB_INT:
          addInstrWithInt(&owner->fn.instr, OP_FPADDR_I, s->varIdx + 1);
          break;
        case TB_DOUBLE:
          addInstrWithInt(&owner->fn.instr, OP_FPADDR_F, s->varIdx + 1);
          break;
        }
      }
    }
    if (s->kind == SK_PARAM) {
      switch (s->type.tb) {
      case TB_INT:
        addInstrWithInt(&owner->fn.instr, OP_FPADDR_I,
                        s->paramIdx - symbolsLen(s->owner->fn.params) - 1);
        break;
      case TB_DOUBLE:
        addInstrWithInt(&owner->fn.instr, OP_FPADDR_F,
                        s->paramIdx - symbolsLen(s->owner->fn.params) - 1);
        break;
      }
    }
    *r = (Ret){s->type, true, s->type.n >= 0};
    return true;
  }

  restoreGuard(guard);

  // Simple atom
  if (consume(INT)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - atom INT");
    addInstrWithInt(&owner->fn.instr, OP_PUSH_I, consumedTk->i);
    *r = (Ret){{TB_INT, NULL, -1}, false, true};
    return true;
  } else if (consume(DOUBLE)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - atom DOUBLE");
    addInstrWithDouble(&owner->fn.instr, OP_PUSH_F, consumedTk->d);
    *r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
    return true;
  } else if (consume(CHAR)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - atom CHAR");
    *r = (Ret){{TB_CHAR, NULL, -1}, false, true};
    return true;
  } else if (consume(STRING)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - atom STRING");
    *r = (Ret){{TB_CHAR, NULL, 0}, false, true};
    return true;
  }

  // Expression with parantheses
  if (consume(LPAR)) {
    if (expr(r)) {
      if (consume(RPAR)) {
        PRINT_DEBUG(HIGH_VERBOSITY,
                    "[ADSR] Found primaryExpr - expression with ()");
        return true;
      } else {
        tkerr("Missing ')' at the end of expression");
      }
    }
  }

  restoreGuard(guard);
  return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim
//                  | DOT ID exprPostfixPrim
//                  | e
bool exprPostfixPrim(Ret *r) {
  Guard guard = makeGuard();

  // Array indexing
  if (consume(LBRACKET)) {
    Ret idx;
    if (expr(&idx)) {
      if (consume(RBRACKET)) {
        PRINT_DEBUG(HIGH_VERBOSITY,
                    "[ADSR] Found exprPostfixPrim - array indexing");
        if (r->type.n < 0) {
          tkerr("only an array can be indexed");
        }
        Type tInt = {TB_INT, NULL, -1};
        if (!convTo(&idx.type, &tInt)) {
          tkerr("the index is not convertible to int");
        }
        r->type.n = -1;
        r->lval = true;
        r->ct = false;
        return exprPostfixPrim(r);
      } else {
        tkerr("Missing ']' in array indexing");
      }
    } else {
      tkerr("Missing value inside array indexing");
    }
  }

  restoreGuard(guard);

  // Struct field access
  if (consume(DOT)) {
    if (consume(ID)) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprPostfixPrim - struct field access");
      Token *tkName = consumedTk;
      if (r->type.tb != TB_STRUCT) {
        tkerr("a field can only be selected from a struct");
      }
      Symbol *s = findSymbolInList(r->type.s->structMembers, tkName->text);
      if (!s) {
        tkerr("the structure %s does not have a field %s", r->type.s->name,
              tkName->text);
      }
      *r = (Ret){s->type, true, s->type.n >= 0};
      return exprPostfixPrim(r);
    } else {
      tkerr("Struct field access with no field name specified");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprPostfixPrim - epsilon");
  return true; // e
}

// exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix(Ret *r) {
  Guard guard = makeGuard();

  if (exprPrimary(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Inside exprPrimary");
    return exprPostfixPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Ret *r) {
  Guard guard = makeGuard();

  if (consume(SUB) || consume(NOT)) {
    if (exprUnary(r)) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprUnary - (SUB | NOT) exprUnary()");
      if (!canBeScalar(r)) {
        tkerr("unary - or ! must have a scalar operand");
      }
      r->lval = false;
      r->ct = true;
      return true;
    } else {
      tkerr("Missing expression after sub or not");
    }
  }

  restoreGuard(guard);

  if (exprPostfix(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprUnary - exprPostfix");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Ret *r) {
  Guard guard = makeGuard();
  Type arraySubscriptType;

  if (consume(LPAR)) {
    Type t;
    Ret op;
    if (typeBase(&t)) {
      if (arrayDecl(&t)) {
        PRINT_DEBUG(MEDIUM_VERBOSITY,
                    "[AD] Found array subscript in exprCast with n = %d", t.n);
      }
      if (consume(RPAR)) {
        if (exprCast(&op)) {
          PRINT_DEBUG(HIGH_VERBOSITY,
                      "[ADSR] Found exprCast - cast expression");
          if (t.tb == TB_STRUCT) {
            tkerr("cannot convert to a struct type");
          }
          if (op.type.tb == TB_STRUCT) {
            tkerr("cannot convert a struct");
          }
          if (op.type.n >= 0 && t.n < 0) {
            tkerr("an array can be converted only to another array");
          }
          if (op.type.n < 0 && t.n >= 0) {
            tkerr("a scalar can be converted only to another scalar");
          }
          *r = (Ret){t, false, true};
          return true;
        } else {
          tkerr("Missing casting expression after ')'");
        }
      }
    }
  }

  restoreGuard(guard);

  if (exprUnary(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprCast - exprUnary");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// epxrMulPrim: ( MUL | DIV ) exprCast exprMulPrim | e
bool exprMulPrim(Ret *r) {
  Guard guard = makeGuard();
  Token *op = NULL;

  if (consume(MUL) || consume(DIV)) {
    op = consumedTk;

    Instr *lastLeft = lastInstr(owner->fn.instr);
    addRVal(&owner->fn.instr, r->lval, &r->type);

    Ret right;
    if (exprCast(&right)) {
      PRINT_DEBUG(
          HIGH_VERBOSITY,
          "[ADSR] Found exprMulPrim - ( MUL | DIV ) exprCast exprMulPrim ");
      Type tDst;
      if (!arithTypeTo(&r->type, &right.type, &tDst)) {
        tkerr("invalid operand type for * or /");
      }
      addRVal(&owner->fn.instr, right.lval, &right.type);
      insertConvIfNeeded(lastLeft, &r->type, &tDst);
      insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
      switch (op->code) {
      case MUL:
        switch (tDst.tb) {
        case TB_INT:
          addInstr(&owner->fn.instr, OP_MUL_I);
          break;
        case TB_DOUBLE:
          addInstr(&owner->fn.instr, OP_MUL_F);
          break;
        }
        break;
      case DIV:
        switch (tDst.tb) {
        case TB_INT:
          addInstr(&owner->fn.instr, OP_DIV_I);
          break;
        case TB_DOUBLE:
          addInstr(&owner->fn.instr, OP_DIV_F);
          break;
        }
        break;
      }

      *r = (Ret){tDst, false, true};
      return exprMulPrim(r);
    } else {
      char message[50] = "Missing expression after ";
      switch (consumedTk->code) {
      case MUL:
        tkerr(strcat(message, "*"));
        break;
      case DIV:
        tkerr(strcat(message, "/"));
        break;
      }
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprMulPrim - epsilon");
  return true; // e
}

// exprMul: exprCast exprMulPrim
bool exprMul(Ret *r) {
  Guard guard = makeGuard();

  if (exprCast(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Inside exprMul");
    return exprMulPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | e
bool exprAddPrim(Ret *r) {
  Guard guard = makeGuard();
  Token *op = NULL;

  if (consume(ADD) || consume(SUB)) {
    op = consumedTk;

    Instr *lastLeft = lastInstr(owner->fn.instr);
    addRVal(&owner->fn.instr, r->lval, &r->type);
    Ret right;
    if (exprMul(&right)) {
      PRINT_DEBUG(
          HIGH_VERBOSITY,
          "[ADSR] Found exprAddPrim - ( ADD | SUB ) exprMul exprAddPrim");
      Type tDst;
      if (!arithTypeTo(&r->type, &right.type, &tDst)) {
        tkerr("invalid operand type for + or -");
      }
      addRVal(&owner->fn.instr, right.lval, &right.type);
      insertConvIfNeeded(lastLeft, &r->type, &tDst);
      insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
      switch (op->code) {
      case ADD:
        switch (tDst.tb) {
        case TB_INT:
          addInstr(&owner->fn.instr, OP_ADD_I);
          break;
        case TB_DOUBLE:
          addInstr(&owner->fn.instr, OP_ADD_F);
          break;
        }
        break;
      case SUB:
        switch (tDst.tb) {
        case TB_INT:
          addInstr(&owner->fn.instr, OP_SUB_I);
          break;
        case TB_DOUBLE:
          addInstr(&owner->fn.instr, OP_SUB_F);
          break;
        }
        break;
      }
      *r = (Ret){tDst, false, true};
      return exprAddPrim(r);
    } else {
      char message[50] = "Missing expression after ";
      switch (consumedTk->code) {
      case ADD:
        tkerr(strcat(message, "+"));
        break;
      case SUB:
        tkerr(strcat(message, "-"));
        break;
      }
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAddPrim - epsilon");
  return true; // e
}

// exprAdd: exprMul exprAddPrim
bool exprAdd(Ret *r) {
  Guard guard = makeGuard();

  if (exprMul(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAdd");
    return exprAddPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprRelPrim: ( LESS | LESSEQ | GREATER | GREATEREQ) exprAdd exprRelPrim
//              | e
bool exprRelPrim(Ret *r) {
  Guard guard = makeGuard();
  Token *op = NULL;

  if (consume(LESS) || consume(LESSEQ) || consume(GREATER) ||
      consume(GREATEREQ)) {
    op = consumedTk;

    Instr *lastLeft = lastInstr(owner->fn.instr);
    addRVal(&owner->fn.instr, r->lval, &r->type);

    Ret right;
    if (exprAdd(&right)) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprRelPrim - (rel op) exprAdd exprRelPrim");
      Type tDst;
      if (!arithTypeTo(&r->type, &right.type, &tDst)) {
        tkerr("invalid operand type for <, <=, >, >=");
      }

      addRVal(&owner->fn.instr, right.lval, &right.type);
      insertConvIfNeeded(lastLeft, &r->type, &tDst);
      insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
      switch (op->code) {
      case LESS:
        switch (tDst.tb) {
        case TB_INT:
          addInstr(&owner->fn.instr, OP_LESS_I);
          break;
        case TB_DOUBLE:
          addInstr(&owner->fn.instr, OP_LESS_F);
          break;
        }
        break;
      }

      *r = (Ret){{TB_INT, NULL, -1}, false, true};
      return exprRelPrim(r);
    } else {
      char message[50] = "Missing expression after ";
      switch (consumedTk->code) {
      case LESS:
        tkerr(strcat(message, "<"));
        break;
      case LESSEQ:
        tkerr(strcat(message, "<="));
        break;
      case GREATER:
        tkerr(strcat(message, ">"));
        break;
      case GREATEREQ:
        tkerr(strcat(message, ">="));
        break;
      }
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprRelPrim - epsilon");
  return true; // e
}

// exprRel: exprAdd exprRelPrim
bool exprRel(Ret *r) {
  Guard guard = makeGuard();

  if (exprAdd(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprRel");
    return exprRelPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim
//             | e
bool exprEqPrim(Ret *r) {
  Guard guard = makeGuard();

  if (consume(EQUAL) || consume(NOTEQ)) {
    Ret right;
    if (exprRel(&right)) {
      PRINT_DEBUG(
          HIGH_VERBOSITY,
          "[ADSR] Found exprEqPrim - ( EQUAL | NOTEQ ) exprRel exprEqPrim");
      Type tDst;
      if (!arithTypeTo(&r->type, &right.type, &tDst)) {
        tkerr("invalid operand type for == or !=");
      }
      *r = (Ret){{TB_INT, NULL, -1}, false, true};
      return exprEqPrim(r);
    } else {
      tkerr("Missing expression after equal or noteq");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprEqPrim - epsilon");
  return true; // e
}

// exprEq: exprRel exprEqPrim
bool exprEq(Ret *r) {
  Guard guard = makeGuard();

  if (exprRel(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprEq");
    return exprEqPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprAndPrim: AND exprEq exprAndPrim
//              | e
bool exprAndPrim(Ret *r) {
  Guard guard = makeGuard();

  if (consume(AND)) {
    Ret right;
    if (exprEq(&right)) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprAndPrim - AND exprEq exprAndPrim");
      Type tDst;
      if (!arithTypeTo(&r->type, &right.type, &tDst)) {
        tkerr("invalid operand type for &&");
      }
      *r = (Ret){{TB_INT, NULL, -1}, false, true};
      return exprAndPrim(r);
    } else {
      tkerr("Missing expression after and");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAndPrim - epsilon");
  return true; // e
}

// exprAnd: exprEq exprAndPrim
bool exprAnd(Ret *r) {
  Guard guard = makeGuard();

  if (exprEq(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAnd");
    return exprAndPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | e
bool exprOrPrim(Ret *r) {
  Guard guard = makeGuard();

  if (consume(OR)) {
    Ret right;
    if (exprAnd(&right)) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprOrPrim - OR exprAnd exprOrPrim");
      Type tDst;
      if (!arithTypeTo(&r->type, &right.type, &tDst)) {
        tkerr("invalid operand type for ||");
      }
      *r = (Ret){{TB_INT, NULL, -1}, false, true};
      return exprOrPrim(r);
    } else {
      tkerr("Missing expression after or");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprOrPrim - epsilon");
  return true; // e
}

// exprOr: exprAnd exprOrPrim
bool exprOr(Ret *r) {
  Guard guard = makeGuard();

  if (exprAnd(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprOr");
    return exprOrPrim(r);
  }

  restoreGuard(guard);
  return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r) {
  Guard guard = makeGuard();

  Ret rDst;
  if (exprUnary(&rDst)) {
    if (consume(ASSIGN)) {
      if (exprAssign(r)) {
        PRINT_DEBUG(HIGH_VERBOSITY,
                    "[ADSR] Found exprAssign - exprUnary ASSIGN exprAssign");

        addRVal(&owner->fn.instr, r->lval, &r->type);
        insertConvIfNeeded(lastInstr(owner->fn.instr), &r->type, &rDst.type);
        switch (rDst.type.tb) {
        case TB_INT:
          addInstr(&owner->fn.instr, OP_STORE_I);
          break;
        case TB_DOUBLE:
          addInstr(&owner->fn.instr, OP_STORE_F);
          break;
        }

        if (!rDst.lval) {
          tkerr("the assign destination must be a left-value");
        }
        if (rDst.ct) {
          tkerr("the assign destination cannot be constant");
        }
        if (!canBeScalar(&rDst)) {
          tkerr("the assign destination must be scalar");
        }
        if (!canBeScalar(r)) {
          tkerr("the assign source must be scalar");
        }
        if (!convTo(&r->type, &rDst.type)) {
          tkerr("the assign source cannot be converted to destination");
        }
        r->lval = false;
        r->ct = true;
        return true;
      } else {
        tkerr("Missing or invalid expression after assign");
      }
    }
  }

  restoreGuard(guard);

  if (exprOr(r)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAssign - exprOr");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// expr: exprAssign
bool expr(Ret *r) { return exprAssign(r); }

bool stm();
// stmCompound: LACC ( varDef | stm )* RACC
bool stmCompound(bool newDomain) {
  Guard guard = makeGuard();

  if (consume(LACC)) {
    if (newDomain) {
      pushDomain();
    }
    while (varDef() || stm())
      ;
    if (consume(RACC)) {
      PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stmCompound");
      if (newDomain) {
        // showDomain(symTable, "compound statement");
        dropDomain();
      }
      return true;
    } else {
      tkerr("Not a valid instruction or missing '}' after instructions");
    }
  }

  restoreGuard(guard);
  return false;
}

// stm: stmCompound
//     | IF LPAR expr RPAR stm ( ELSE stm )?
//     | WHILE LPAR expr RPAR stm
//     | RETURN expr? SEMICOLON
//     | expr? SEMICOLON
bool stm() {
  Guard guard = makeGuard();

  Ret rCond, rExpr;

  if (stmCompound(true)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - compound statement");
    return true;
  }

  restoreGuard(guard);

  // IF structure
  if (consume(IF)) {
    if (consume(LPAR)) {
      if (expr(&rCond)) {
        if (!canBeScalar(&rCond)) {
          tkerr("the if condition must be a scalar value");
        }
        if (consume(RPAR)) {
          addRVal(&owner->fn.instr, rCond.lval, &rCond.type);
          Type intType = {TB_INT, NULL, -1};
          insertConvIfNeeded(lastInstr(owner->fn.instr), &rCond.type, &intType);
          Instr *ifJF = addInstr(&owner->fn.instr, OP_JF);
          if (stm()) {
            if (consume(ELSE)) {
              Instr *ifJMP = addInstr(&owner->fn.instr, OP_JMP);
              ifJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
              if (stm()) {
                ifJMP->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
              } else {
                tkerr("Missing statement inside else");
              }
            } else {
              ifJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
            }
            PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - if statement");
            return true;
          } else {
            tkerr("Missing statement inside if");
          }
        } else {
          tkerr("if condition not correct or missing ')' after if condition");
        }
      } else {
        tkerr("Missing or invalid if condition");
      }
    } else {
      tkerr("Missing '(' before if condition");
    }
  }

  // WHILE structure
  if (consume(WHILE)) {
    Instr *beforeWhileCond = lastInstr(owner->fn.instr);
    if (consume(LPAR)) {
      if (expr(&rCond)) {
        if (!canBeScalar(&rCond)) {
          tkerr("the while condition must be a scalar value");
        }
        if (consume(RPAR)) {
          addRVal(&owner->fn.instr, rCond.lval, &rCond.type);
          Type intType = {TB_INT, NULL, -1};
          insertConvIfNeeded(lastInstr(owner->fn.instr), &rCond.type, &intType);
          Instr *whileJF = addInstr(&owner->fn.instr, OP_JF);

          if (stm()) {
            addInstr(&owner->fn.instr, OP_JMP)->arg.instr =
                beforeWhileCond->next;
            whileJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
            PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - while statement");
            return true;
          } else {
            tkerr("Missing statement inside while");
          }
        } else {
          tkerr("while condition not correct or missing ')' after while "
                "condition");
        }
      } else {
        tkerr("Missing or invalid while condition");
      }
    } else {
      tkerr("Missing '(' before while condition");
    }
  }

  // RETURN structure RETURN expr? SEMICOLON
  if (consume(RETURN)) {
    if (expr(&rExpr)) {
      addRVal(&owner->fn.instr, rExpr.lval, &rExpr.type);
      insertConvIfNeeded(lastInstr(owner->fn.instr), &rExpr.type, &owner->type);
      addInstrWithInt(&owner->fn.instr, OP_RET, symbolsLen(owner->fn.params));

      if (owner->type.tb == TB_VOID)
        tkerr("a void function cannot return a value");
      if (!canBeScalar(&rExpr))
        tkerr("the return value must be a scalar value");
      if (!convTo(&rExpr.type, &owner->type))
        tkerr("cannot convert the return expression type to the function "
              "return type");
    } else {
      addInstr(&owner->fn.instr, OP_RET_VOID);
      if (owner->type.tb != TB_VOID) {
        tkerr("a non-void function must return a value");
      }
    }
    if (consume(SEMICOLON)) {
      PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - return statement");
      return true;
    } else {
      tkerr("Missing ';' after return statement");
    }
  }

  // Simple statement
  if (expr(&rExpr)) {
    if (rExpr.type.tb != TB_VOID) {
      addInstr(&owner->fn.instr, OP_DROP);
    }
    if (consume(SEMICOLON)) {
      PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - simple statement");
      return true;
    } else {
      tkerr("Missing semicolon after expression");
    }
  }

  if (consume(SEMICOLON)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - simple statement");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam() {
  Guard guard = makeGuard();
  Type t;

  if (typeBase(&t)) {
    if (consume(ID)) {
      Token *tkName = consumedTk;
      if (arrayDecl(&t)) {
        PRINT_DEBUG(MEDIUM_VERBOSITY, "[AD] Found fnParam array with n = %d",
                    t.n);
        t.n = 0;
      }
      Symbol *param = findSymbolInDomain(symTable, tkName->text);
      if (param) {
        tkerr("Symbol redefinition: %s", tkName->text);
      }
      param = newSymbol(tkName->text, SK_PARAM);
      param->type = t;
      param->owner = owner;
      param->paramIdx = symbolsLen(owner->fn.params);
      addSymbolToDomain(symTable, param);
      addSymbolToList(&owner->fn.params, dupSymbol(param));
      PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found fnParam");
      return true;
    } else {
      tkerr("Missing function parameter name");
    }
  } else {
    if (consume(ID)) {
      arrayDecl(&t);
      tkerr("Missing function parameter type");
    }
  }

  restoreGuard(guard);
  return false;
}
// fnDef: ( typeBase | VOID ) ID
//         LPAR ( fnParam ( COMMA fnParam )* )? RPAR
//             stmCompound
bool fnDef() {
  Guard guard = makeGuard();
  Type t;

  if (typeBase(&t) || consume(VOID)) {
    if (consumedTk->code == VOID) {
      t.tb = TB_VOID;
    }
    if (consume(ID)) {
      Token *tkName = consumedTk;
      if (consume(LPAR)) {
        Symbol *fn = findSymbolInDomain(symTable, tkName->text);
        if (fn) {
          tkerr("Symbol redefinition: %s", tkName->text);
        }
        fn = newSymbol(tkName->text, SK_FN);
        fn->type = t;
        addSymbolToDomain(symTable, fn);
        owner = fn;
        pushDomain();
        if (fnParam()) {
          while (consume(COMMA)) {
            if (!fnParam()) {
              tkerr("Missing function parameter after ','");
            }
          }
        }
        if (consume(RPAR)) {
          addInstr(&fn->fn.instr, OP_ENTER);
          if (stmCompound(false)) {
            PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found fnDef");
            fn->fn.instr->arg.i = symbolsLen(fn->fn.locals);
            if (fn->type.tb == TB_VOID) {
              addInstrWithInt(&fn->fn.instr, OP_RET_VOID,
                              symbolsLen(fn->fn.params));
            }
            dropDomain();
            owner = NULL;
            return true;
          } else {
            tkerr("Not a valid set of instruction");
          }
        } else {
          tkerr("Function parameters not correctly defined or missing ')' in "
                "function definition");
        }
      }
    } else if (!arrayDecl(&t) && !consume(SEMICOLON)) {
      tkerr("Missing function name or '{' after struct definition");
    }
  }

  restoreGuard(guard);
  return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit() {
  for (;;) {
    if (structDef()) {
      PRINT_DEBUG(LOW_VERBOSITY, "FOUND STRUCT DEF");
    } else if (fnDef()) {
      PRINT_DEBUG(LOW_VERBOSITY, "FOUND FUNC DEF");
    } else if (varDef()) {
      PRINT_DEBUG(LOW_VERBOSITY, "FOUND VAR DEF");
    } else
      break;
  }
  if (consume(END)) {
    return true;
  }
  return false;
}

void parse(Token *tokens) {
  iTk = tokens;
  pushDomain();
  if (!unit()) {
    tkerr("syntax error");
  }
}
