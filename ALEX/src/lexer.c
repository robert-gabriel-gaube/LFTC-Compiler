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
  if (isalpha(**ppch) || **ppch == '_') {
    Token *tk = NULL;

    // Extract the portion of text from start to pch
    const char *start = (*ppch)++;
    while (isalnum(**ppch) || **ppch == '_') {
      ++(*ppch);
    }
    char *text = extract(start, *ppch);

    if (strcmp(text, "char") == 0) {
      addToken(TYPE_CHAR);
      free(text);
    } else {
      tk = addToken(ID);
      tk->text = text;
    }
  } else {
    throwError("Invalid char: %c (%d)", **ppch, **ppch);
  }
}

Token *tokenize(const char *pch) {
  for (;;) {
    switch (*pch) {
    case ' ':
    case '\t':
      pch++;
      break;
    case '\r': // handles different kinds of newlines (Windows: \r\n, Linux: \n,
               // MacOS, OS X: \r or \n)
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
    case '=':
      handle_equal(&pch);
      break;
    default:
      handle_default(&pch);
    }
  }
}

void showTokens(const Token *tokens) {
  for (const Token *tk = tokens; tk; tk = tk->next) {
    printf("%d\n", tk->code);
  }
}
