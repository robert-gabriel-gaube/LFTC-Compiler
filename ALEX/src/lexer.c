#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "utils.h"

Token *tokens = NULL; // single linked list of tokens
Token *lastTk = NULL; // the last token in list

int line = 1; // the current line in the input file

// Adds a token to the end of the tokens list and returns it.
// Sets it's code and line.
Token *addToken(int code) {
  Token *tk = (Token *)safeAlloc(sizeof(Token));

  // Initialize token
  tk->code = code;
  tk->line = line;
  tk->next = NULL;

  // Place token in list
  if (lastTk) {
    lastTk->next = tk;
  } else {
    tokens = tk;
  }
  lastTk = tk;

  return tk;
}

char *extract(const char *begin, const char *end) {
  if (end < begin) {
    throwError("Not a valid string segment");
  }

  size_t size_of_segment = end - begin;
  char *segment = (char *)safeAlloc(size_of_segment * sizeof(char) + 1);
  strncpy(segment, begin, size_of_segment);
  segment[size_of_segment] = '\0';

  return segment;
}

void handleComment(const char **pch) {
  while (**pch != '\r' && **pch != '\n') {
    ++(*pch);
  }
  if ((*pch)[1] == '\n') {
    (*pch) += 2;
  } else {
    ++(*pch);
  }
  ++line;
}

void handle_equal(const char **ppch) {
  if ((*ppch)[1] == '=') {
    addToken(EQUAL);
    (*ppch) += 2;
  } else {
    addToken(ASSIGN);
    ++(*ppch);
  }
}

void handle_default(const char **ppch) {
  if (isdigit(**ppch)) {
    char *endLong, *endDouble;

    Token *tk = NULL;

    long number = strtol(*ppch, &endLong, 10);
    double numberDouble = strtod(*ppch, &endDouble);

    if (endLong == endDouble && endLong != *ppch) {
      tk = addToken(INT);
      tk->i = number;
      (*ppch) = endLong;
    } else if (endDouble != *ppch) {
      tk = addToken(DOUBLE);
      tk->d = numberDouble;
      (*ppch) = endDouble;
    }
  } else if (isalpha(**ppch) || **ppch == '_') {
    Token *tk = NULL;

    // Extract the portion of text from start to pch
    const char *start = (*ppch)++;
    while (isalnum(**ppch) || **ppch == '_') {
      ++(*ppch);
    }
    char *text = extract(start, *ppch);

    if (strcmp(text, "char") == 0) {
      addToken(TYPE_CHAR);
    } else if (strcmp(text, "double") == 0) {
      addToken(TYPE_DOUBLE);
    } else if (strcmp(text, "else") == 0) {
      addToken(ELSE);
    } else if (strcmp(text, "if") == 0) {
      addToken(IF);
    } else if (strcmp(text, "int") == 0) {
      addToken(TYPE_INT);
    } else if (strcmp(text, "return") == 0) {
      addToken(RETURN);
    } else if (strcmp(text, "struct") == 0) {
      addToken(STRUCT);
    } else if (strcmp(text, "void") == 0) {
      addToken(VOID);
    } else if (strcmp(text, "while") == 0) {
      addToken(WHILE);
    } else {
      tk = addToken(ID);
      tk->text = text;
    }
  } else {
    throwError("Invalid char: %c (%d)", **ppch, **ppch);
  }
}

void handleString(const char **ppch) {
  ++(*ppch);
  char *string = (char *)safeAlloc(100 * sizeof(char));
  int idx = 0;
  while (**ppch != '"') {
    string[idx] = **ppch;
    ++(*ppch);
    ++idx;
  }
  ++(*ppch);
  string[idx] = 0;
  Token *tk = addToken(STRING);
  tk->text = string;
}

Token *tokenize(const char *pch) {
  for (;;) {
    switch (*pch) {
    case ' ':
    case '\t':
      pch++;
      break;
    case '\r': // handles different kinds of newlines (Windows: \r\n, Linux:
               // \n, MacOS, OS X: \r or \n)
      pch += (*pch == '\n' ? 1 : 0);
      // fallthrough to \n
    case '\n':
      line++;
      pch++;
      break;
    case '\0':
      addToken(END);
      return tokens;
    case ',':
      addToken(COMMA);
      pch++;
      break;
    case ';':
      addToken(SEMICOLON);
      pch++;
      break;
    case '(':
      addToken(LPAR);
      pch++;
      break;
    case ')':
      addToken(RPAR);
      pch++;
      break;
    case '[':
      addToken(LBRACKET);
      pch++;
      break;
    case ']':
      addToken(RBRACKET);
      pch++;
      break;
    case '{':
      addToken(LACC);
      pch++;
      break;
    case '}':
      addToken(RACC);
      pch++;
      break;
    case '+':
      addToken(ADD);
      pch++;
      break;
    case '-':
      addToken(SUB);
      pch++;
      break;
    case '*':
      addToken(MUL);
      pch++;
      break;
    case '"':
      handleString(&pch);
      break;
    case '\'':
      if (pch[1] != '\'' && pch[3] == '\'') {
        Token *tk = addToken(CHAR);
        tk->c = pch[2];
      }
      pch += 3;
      break;
    case '/':
      if (pch[1] == '/') {
        handleComment(&pch);
      } else {
        addToken(DIV);
        pch++;
      }
      break;
    case '.':
      addToken(DOT);
      pch++;
      break;
    case '&':
      if (pch[1] == '&') {
        addToken(AND);
      } else {
        throwError("Invalid random alone '&'");
      }
      pch += 2;
      break;
    case '|':
      if (pch[1] == '|') {
        addToken(OR);
      } else {
        throwError("Invalid random alone '|'");
      }
      pch += 2;
      break;
    case '!':
      if (pch[1] == '=') {
        addToken(NOTEQ);
        pch += 2;
      } else {
        addToken(NOT);
        pch++;
      }
      break;
    case '<':
      if (pch[1] == '=') {
        addToken(LESSEQ);
        pch += 2;
      } else {
        addToken(LESS);
        pch++;
      }
      break;
    case '>':
      if (pch[1] == '=') {
        addToken(GREATEREQ);
        pch += 2;
      } else {
        addToken(GREATER);
        pch++;
      }
      break;
    case '=':
      handle_equal(&pch);
      break;
    default:
      handle_default(&pch);
    }
  }
}
const char *getTokenString(int token) {
  switch (token) {
  case ID:
    return "ID";
  case TYPE_CHAR:
    return "TYPE_CHAR";
  case TYPE_DOUBLE:
    return "TYPE_DOUBLE";
  case ELSE:
    return "ELSE";
  case IF:
    return "IF";
  case TYPE_INT:
    return "TYPE_INT";
  case RETURN:
    return "RETURN";
  case STRUCT:
    return "STRUCT";
  case VOID:
    return "VOID";
  case WHILE:
    return "WHILE";
  case SEMICOLON:
    return "SEMICOLON";
  case LPAR:
    return "LPAR";
  case RPAR:
    return "RPAR";
  case LBRACKET:
    return "LBRACKET";
  case RBRACKET:
    return "RBRACKET";
  case LACC:
    return "LACC";
  case RACC:
    return "RACC";
  case COMMA:
    return "COMMA";
  case END:
    return "END";
  case ADD:
    return "ADD";
  case SUB:
    return "SUB";
  case MUL:
    return "MUL";
  case DIV:
    return "DIV";
  case DOT:
    return "DOT";
  case AND:
    return "AND";
  case OR:
    return "OR";
  case NOT:
    return "NOT";
  case ASSIGN:
    return "ASSIGN";
  case EQUAL:
    return "EQUAL";
  case NOTEQ:
    return "NOTEQ";
  case LESS:
    return "LESS";
  case LESSEQ:
    return "LESSEQ";
  case GREATER:
    return "GREATER";
  case GREATEREQ:
    return "GREATEREQ";
  case INT:
    return "INT";
  case DOUBLE:
    return "DOUBLE";
  case CHAR:
    return "CHAR";
  case STRING:
    return "STRING";
  default:
    return "UNKNOWN";
  }
}
void showTokens(const Token *tokens) {
  for (const Token *tk = tokens; tk; tk = tk->next) {
    const char *label = getTokenString(tk->code);
    printf("%d\t%s", tk->line, label);
    if (strcmp(label, "ID") == 0 || strcmp(label, "STRING") == 0) {
      printf(":%s", tk->text);
    } else if (strcmp(label, "INT") == 0) {
      printf(":%d", tk->i);
    } else if (strcmp(label, "DOUBLE") == 0) {
      printf(":%f", tk->d);
    } else if (strcmp(label, "CHAR") == 0) {
      printf(":%c", tk->c);
    }
    printf("\n");
  }
}
