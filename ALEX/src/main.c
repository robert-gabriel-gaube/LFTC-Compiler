#include "lexer.h"
#include "utils.h"
#include <stdio.h>

int main() {
    char *file = loadFile("/home/rgaube/Personal/C/LFTC-Compiler/ALEX/tests/testlex.c");
    Token *tokens = tokenize(file);
    showTokens(tokens);
    return 0;
}