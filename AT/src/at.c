#include "at.h"
#include <string.h>


bool canBeScalar(Ret *r) {
  Type *t = &r->type;
  if (t->n >= 0)
    return false;
  if (t->tb == TB_VOID)
    return false;
  return true;
}

bool convTo(Type *src, Type *dst) {
  // the pointers (arrays) can be converted one to another, but in nothing else
  if (src->n >= 0) {
    if (dst->n >= 0)
      return true;
    return false;
  }
  if (dst->n >= 0)
    return false;
  switch (src->tb) {
  case TB_INT:
  case TB_DOUBLE:
  case TB_CHAR:
    switch (dst->tb) {
    case TB_INT:
    case TB_CHAR:
    case TB_DOUBLE:
      return true;
    default:
      return false;
    }
  // a struct can be converted only to itself
  case TB_STRUCT:
    if (dst->tb == TB_STRUCT && src->s == dst->s)
      return true;
    return false;
  default:
    return false;
  }
}

bool arithTypeTo(Type *t1, Type *t2, Type *dst) {
  // there are no arithmetic operations with pointers
  if (t1->n >= 0 || t2->n >= 0)
    return false;
  // the result of an arithmetic operation cannot be pointer or struct
  dst->s = NULL;
  dst->n = -1;
  switch (t1->tb) {
  case TB_INT:
    switch (t2->tb) {
    case TB_INT:
    case TB_CHAR:
      dst->tb = TB_INT;
      return true;
    case TB_DOUBLE:
      dst->tb = TB_DOUBLE;
      return true;
    default:
      return false;
    }
  case TB_DOUBLE:
    switch (t2->tb) {
    case TB_INT:
    case TB_DOUBLE:
    case TB_CHAR:
      dst->tb = TB_DOUBLE;
      return true;
    default:
      return false;
    }
  case TB_CHAR:
    switch (t2->tb) {
    case TB_INT:
    case TB_DOUBLE:
    case TB_CHAR:
      dst->tb = t2->tb;
      return true;
    default:
      return false;
    }
  default:
    return false;
  }
}

Symbol *findSymbolInList(Symbol *list, const char *name) {
  for (Symbol *s = list; s; s = s->next) {
    if (!strcmp(s->name, name))
      return s;
  }
  return NULL;
}
