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

  // Alloc space for segment
  size_t size_of_segment = end - begin;
  char *segment = (char *)safeAlloc(size_of_segment * sizeof(char) + 1);

  // Copy segment
  strncpy(segment, begin, size_of_segment);
  segment[size_of_segment] = '\0';

  return segment;
}

const char *handleComment(const char *pch) {
  while (*pch != '\r' && *pch != '\n') {
    ++pch;
  }
  return pch;
}

const char *handle_possibly_double_char(const char *pch, char ch,
                                        TokenType if_yes, TokenType if_no) {
  if (pch[1] == ch) {
    addToken(if_yes);
    return pch + 2;
  } else {
    addToken(if_no);
    return pch + 1;
  }
}

const char *handle_mandatory_double_char(const char *pch, char ch,
                                         TokenType if_yes) {
  if (pch[1] == ch) {
    addToken(if_yes);
  } else {
    throwError("Invalid random alone '%c'", ch);
  }
  return pch + 2;
}

const char *handle_double_char(const char *pch) {
  if (*pch == '&') {
    return handle_mandatory_double_char(pch, *pch, AND);
  } else if (*pch == '|') {
    return handle_mandatory_double_char(pch, *pch, OR);
  } else {
    const char *operators = "!<>=";
    TokenType if_yes_values[] = {NOTEQ, LESSEQ, GREATEREQ, EQUAL};
    TokenType if_no_values[] = {NOT, LESS, GREATER, ASSIGN};

    size_t idx = strchr(operators, *pch) - operators;
    return handle_possibly_double_char(pch, *pch, if_yes_values[idx],
                                       if_no_values[idx]);
  }
}

const char *handle_char(const char *pch) {
  if (pch[2] == '\'') {
    Token *tk = addToken(CHAR);
    tk->c = pch[1];
  }
  return pch + 3;
}

const char *handle_slash(const char *pch) {
  if (pch[1] == '/') {
    return handleComment(pch);
  } else {
    addToken(DIV);
    return pch + 1;
  }
}

const char *handle_single_char(const char *pch) {
  const char *single_chars = ",;()[]{}+-*.";
  TokenType match_for_chars[] = {COMMA,    SEMICOLON, LPAR, RPAR,
                                 LBRACKET, RBRACKET,  LACC, RACC,
                                 ADD,      SUB,       MUL,  DOT};

  // Get token index
  const char *char_ptr = strchr(single_chars, *pch);
  size_t idx = char_ptr - single_chars;

  addToken(match_for_chars[idx]);
  return pch + 1;
}

const char *handle_number(const char *pch) {
  char *endLong, *endDouble;
  long number = strtol(pch, &endLong, 10);
  double numberDouble = strtod(pch, &endDouble);

  if (endLong == endDouble) {
    Token *tk = addToken(INT);
    tk->i = number;
    return endLong;
  } else if (endDouble != pch) {
    Token *tk = addToken(DOUBLE);
    tk->d = numberDouble;
    return endDouble;
  }
}

void handle_text(char *text) {
  const char *keywords[] = {"char",   "double", "int",  "else", "if",
                            "return", "struct", "void", "while"};
  TokenType tokens[] = {TYPE_CHAR, TYPE_DOUBLE, TYPE_INT, ELSE, IF,
                        RETURN,    STRUCT,      VOID,     WHILE};

  size_t number_of_keywords = 9;
  for(size_t idx = 0; idx < number_of_keywords; ++idx) {
    if(strcmp(text, keywords[idx])) {
      addToken(tokens[idx]);
      return;
    }
  }

  // If not keyword
  Token *tk = addToken(ID);
  tk->text = text;
}

const char *handle_id_or_keyword(const char *pch) {
  Token *tk = NULL;

  // Extract the portion of text from start to pch
  const char *start = pch++;
  while (isalnum(*pch) || *pch == '_') {
    ++pch;
  }
  char *text = extract(start, pch);

  handle_text(text);

  return pch;
}

const char *handle_default(const char *pch) {
  if (isdigit(*pch)) {
    return handle_number(pch);
  } else if (isalpha(*pch) || *pch == '_') {
    return handle_id_or_keyword(pch);
  } else {
    throwError("Invalid char: %c (%d)", *pch, *pch);
  }
}

const char *handle_string(const char *pch) {
  ++pch; // Jump over '"'
  size_t number_of_chars_in_string = strchr(pch, '"') - pch;

  Token *tk = addToken(STRING);
  tk->text = extract(pch, pch + number_of_chars_in_string);

  return pch + number_of_chars_in_string + 1;
}

Token *tokenize(const char *pch) {
  for (;;) {
    switch (*pch) {
    case ' ':
    case '\t':
      pch++;
      break;
    case '\r':
      pch += (pch[1] == '\n' ? 1 : 0);
      // fallthrough to \n
    case '\n':
      line++;
      pch++;
      break;
    case '\0':
      addToken(END);
      return tokens;
    case ',':
    case ';':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '+':
    case '-':
    case '*':
    case '.':
      pch = handle_single_char(pch);
      break;
    case '"':
      pch = handle_string(pch);
      break;
    case '\'':
      pch = handle_char(pch);
      break;
    case '/':
      pch = handle_slash(pch);
      break;
    case '&':
    case '|':
    case '!':
    case '<':
    case '>':
    case '=':
      pch = handle_double_char(pch);
      break;
    default:
      pch = handle_default(pch);
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
    if (tk->code == ID || tk->code == STRING) {
      printf(":%s", tk->text);
    } else if (tk->code == INT) {
      printf(":%d", tk->i);
    } else if (tk->code == DOUBLE) {
      printf(":%f", tk->d);
    } else if (tk->code == CHAR) {
      printf(":%c", tk->c);
    }
    printf("\n");
  }
}
