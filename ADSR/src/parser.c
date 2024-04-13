#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad.h"
#include "parser.h"
#include "utils.h"

Token *iTk = NULL;        // the iterator in the tokens list
Token *consumedTk = NULL; // the last consumed token

Symbol *owner = NULL;

typedef Token *Guard;

Guard makeGuard() { return iTk; }
void restoreGuard(Guard g) { iTk = g; }

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
            //showDomain(symTable, tkName->text);
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
bool exprPrimary() {
  Guard guard = makeGuard();

  // Function call
  if (consume(ID)) {
    if (consume(LPAR)) {
      if (expr()) {
        while (consume(COMMA)) {
          if (!expr()) {
            tkerr("Expected expression after ','");
          }
        }
      }
      if (!consume(RPAR)) {
        tkerr("Missing ')' in function call");
      }
    }
    PRINT_DEBUG(HIGH_VERBOSITY,
                "[ADSR] Found primaryExpr - function call or simple ID");
    return true;
  }

  restoreGuard(guard);

  // Simple atom
  if (consume(INT) || consume(DOUBLE) || consume(CHAR) || consume(STRING)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found primaryExpr - atom");
    return true;
  }

  // Expression with parantheses
  if (consume(LPAR)) {
    if (expr()) {
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
bool exprPostfixPrim() {
  Guard guard = makeGuard();

  // Array indexing
  if (consume(LBRACKET)) {
    if (expr()) {
      if (consume(RBRACKET)) {
        PRINT_DEBUG(HIGH_VERBOSITY,
                    "[ADSR] Found exprPostfixPrim - array indexing");
        return exprPostfixPrim();
      } else {
        tkerr("Missing ']' in array indexing");
      }
    }
  }

  restoreGuard(guard);

  // Struct field access
  if (consume(DOT)) {
    if (consume(ID)) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprPostfixPrim - struct field access");
      return exprPostfixPrim();
    } else {
      tkerr("Struct field access with no field name specified");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprPostfixPrim - epsilon");
  return true; // e
}

// exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix() {
  Guard guard = makeGuard();

  if (exprPrimary()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Inside exprPrimary");
    return exprPostfixPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary() {
  Guard guard = makeGuard();

  if (consume(SUB) || consume(NOT)) {
    if (exprUnary()) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprUnary - (SUB | NOT) exprUnary()");
      return true;
    } else {
      tkerr("Missing expression after sub or not");
    }
  }

  restoreGuard(guard);

  if (exprPostfix()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprUnary - exprPostfix");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast() {
  Guard guard = makeGuard();
  Type arraySubscriptType;

  if (consume(LPAR)) {
    Type t;
    if (typeBase(&t)) {
      if (arrayDecl(&t)) {
        PRINT_DEBUG(MEDIUM_VERBOSITY,
                    "[AD] Found array subscript in exprCast with n = %d", t.n);
      }
      if (consume(RPAR)) {
        if (exprCast()) {
          PRINT_DEBUG(HIGH_VERBOSITY,
                      "[ADSR] Found exprCast - cast expression");
          return true;
        } else {
          tkerr("Missing casting expression after ')'");
        }
      }
    }
  }

  restoreGuard(guard);

  if (exprUnary()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprCast - exprUnary");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// epxrMulPrim: ( MUL | DIV ) exprCast exprMulPrim | e
bool exprMulPrim() {
  Guard guard = makeGuard();

  if (consume(MUL) || consume(DIV)) {
    if (exprCast()) {
      PRINT_DEBUG(
          HIGH_VERBOSITY,
          "[ADSR] Found exprMulPrim - ( MUL | DIV ) exprCast exprMulPrim ");
      return exprMulPrim();
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
bool exprMul() {
  Guard guard = makeGuard();

  if (exprCast()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Inside exprMul");
    return exprMulPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | e
bool exprAddPrim() {
  Guard guard = makeGuard();

  if (consume(ADD) || consume(SUB)) {
    if (exprMul()) {
      PRINT_DEBUG(
          HIGH_VERBOSITY,
          "[ADSR] Found exprAddPrim - ( ADD | SUB ) exprMul exprAddPrim");
      return exprAddPrim();
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
bool exprAdd() {
  Guard guard = makeGuard();

  if (exprMul()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAdd");
    return exprAddPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprRelPrim: ( LESS | LESSEQ | GREATER | GREATEREQ) exprAdd exprRelPrim
//              | e
bool exprRelPrim() {
  Guard guard = makeGuard();

  if (consume(LESS) || consume(LESSEQ) || consume(GREATER) ||
      consume(GREATEREQ)) {
    if (exprAdd()) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprRelPrim - (rel op) exprAdd exprRelPrim");
      return exprAddPrim();
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
bool exprRel() {
  Guard guard = makeGuard();

  if (exprAdd()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprRel");
    return exprRelPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim
//             | e
bool exprEqPrim() {
  Guard guard = makeGuard();

  if (consume(EQUAL) || consume(NOTEQ)) {
    if (exprRel()) {
      PRINT_DEBUG(
          HIGH_VERBOSITY,
          "[ADSR] Found exprEqPrim - ( EQUAL | NOTEQ ) exprRel exprEqPrim");
      return exprEqPrim();
    } else {
      tkerr("Missing expression after equal or noteq");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprEqPrim - epsilon");
  return true; // e
}

// exprEq: exprRel exprEqPrim
bool exprEq() {
  Guard guard = makeGuard();

  if (exprRel()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprEq");
    return exprEqPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprAndPrim: AND exprEq exprAndPrim
//              | e
bool exprAndPrim() {
  Guard guard = makeGuard();

  if (consume(AND)) {
    if (exprEq()) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprAndPrim - AND exprEq exprAndPrim");
      return exprAndPrim();
    } else {
      tkerr("Missing expression after and");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAndPrim - epsilon");
  return true; // e
}

// exprAnd: exprEq exprAndPrim
bool exprAnd() {
  Guard guard = makeGuard();

  if (exprEq()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAnd");
    return exprAndPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | e
bool exprOrPrim() {
  Guard guard = makeGuard();

  if (consume(OR)) {
    if (exprAnd()) {
      PRINT_DEBUG(HIGH_VERBOSITY,
                  "[ADSR] Found exprOrPrim - OR exprAnd exprOrPrim");
      return exprOrPrim();
    } else {
      tkerr("Missing expression after or");
    }
  }

  restoreGuard(guard);
  PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprOrPrim - epsilon");
  return true; // e
}

// exprOr: exprAnd exprOrPrim
bool exprOr() {
  Guard guard = makeGuard();

  if (exprAnd()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprOr");
    return exprOrPrim();
  }

  restoreGuard(guard);
  return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign() {
  Guard guard = makeGuard();

  if (exprUnary()) {
    if (consume(ASSIGN)) {
      if (exprAssign()) {
        PRINT_DEBUG(HIGH_VERBOSITY,
                    "[ADSR] Found exprAssign - exprUnary ASSIGN exprAssign");
        return true;
      } else {
        tkerr("Missing or invalid expression after assign");
      }
    }
  }

  restoreGuard(guard);

  if (exprOr()) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found exprAssign - exprOr");
    return true;
  }

  restoreGuard(guard);
  return false;
}

// expr: exprAssign
bool expr() { return exprAssign(); }

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
        //showDomain(symTable, "compound statement");
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

  if (stmCompound(true)) {
    PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - compound statement");
    return true;
  }

  restoreGuard(guard);

  // IF structure
  if (consume(IF)) {
    if (consume(LPAR)) {
      if (expr()) {
        if (consume(RPAR)) {
          if (stm()) {
            if (consume(ELSE)) {
              if (!stm()) {
                tkerr("Missing statement inside else");
              }
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
    if (consume(LPAR)) {
      if (expr()) {
        if (consume(RPAR)) {
          if (stm()) {
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
    expr();
    if (consume(SEMICOLON)) {
      PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found stm - return statement");
      return true;
    } else {
      tkerr("Missing ';' after return statement");
    }
  }

  // Simple statement
  if (expr()) {
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
          if (stmCompound(false)) {
            PRINT_DEBUG(HIGH_VERBOSITY, "[ADSR] Found fnDef");
            //showDomain(symTable, tkName->text);
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
