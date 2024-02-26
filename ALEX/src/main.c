#include "lexer.h"
#include "utils.h"

int main() {
    char *file = loadFile("/home/robert/Desktop/Facultate/LFTC/ALEX/tests/testlex.c");
    Token *tokens = tokenize(file);
    showTokens(tokens);
    return 0;
}