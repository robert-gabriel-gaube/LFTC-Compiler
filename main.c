#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"
#include "vm.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    if(argc != 2) {
        throwError("Usage ./ALEX <file_path>");
    }
    char *file_data = loadFile(argv[1]);
    Token *tokens = tokenize(file_data);
    // showTokens(tokens);
    pushDomain();
    vmInit();
    parse(tokens);
    // showDomain(symTable, "Global");
    Instr* testCode = genTestProgramDouble();
    run(testCode);
    dropDomain();
    return 0;
}