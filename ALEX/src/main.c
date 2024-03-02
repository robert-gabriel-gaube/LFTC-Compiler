#include "lexer.h"
#include "utils.h"
#include <stdio.h>

int main() {
    char *file = loadFile("/home/robert/Desktop/Facultate/LFTC/LFTC-Compiler/ALEX/tests/testlex.c");
    Token *tokens = tokenize(file);
    showTokens(tokens);
    return 0;
}