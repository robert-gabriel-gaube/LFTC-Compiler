#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

Token *iTk = NULL;        // the iterator in the tokens list
Token *consumedTk = NULL; // the last consumed token

typedef Token* Guard;

Guard makeGuard() {
  return iTk;
}
void restoreGuard(Guard g) {
  iTk = g;
}

void tkerr(const char *fmt, ...) {
  fprintf(stderr, "error in line %d: ", iTk->line);

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
bool typeBase() {
  if (consume(TYPE_INT)) {
    return true;
  }
  if (consume(TYPE_DOUBLE)) {
    return true;
  }
  if (consume(TYPE_CHAR)) {
    return true;
  }
  if (consume(STRUCT)) {
    if (consume(ID)) {
      return true;
    }
  }
  return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl() {
  Guard guard = makeGuard();

  if(consume(LBRACKET)) {
    consume(INT);
    if(consume(RBRACKET)) {
      return true;
    }
  }

  restoreGuard(guard);
  return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef() {
  Guard guard = makeGuard();

  if(typeBase()) {
    if(consume(ID)) {
      arrayDecl();
      if(consume(SEMICOLON)) {
        return true;
      }
    }
  }

  restoreGuard(guard);
  return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef() {
  Guard guard = makeGuard();

  if(consume(STRUCT)) {
    if(consume(ID)) {
      if(consume(LACC)) {
        while(varDef());
        if(consume(RACC)) {
          if(consume(SEMICOLON)) {
            return true;
          }
        }
      }
    }
  }

  restoreGuard(guard);
  return false;
}
bool fnDef() {
  return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit() {
  for (;;) {
    if (structDef()) {
      puts("Struct found");
    } else if (fnDef()) {
      puts("Funct found");
    } else if (varDef()) {
      puts("Var def found");
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
  if (!unit())
    tkerr("syntax error");
}
