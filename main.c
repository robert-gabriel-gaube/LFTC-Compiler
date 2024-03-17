#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    if(argc != 2) {
        throwError("Usage ./ALEX <file_path>");
    }
    char *file_data = loadFile(argv[1]);
    Token *tokens = tokenize(file_data);
    puts("DONE WITH TOKENIZE");
    showTokens(tokens);
    parse(tokens);
    return 0;
}