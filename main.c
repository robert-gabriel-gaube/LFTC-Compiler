#include "ad.h"
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    throwError("Usage ./ALEX <file_path>");
  }
  char *file_data = loadFile(argv[1]);
  Token *tokens = tokenize(file_data);
  // showTokens(tokens);
  pushDomain();
  vmInit();
  parse(tokens);

  Symbol *symMain = findSymbolInDomain(symTable, "main");
  if (!symMain) {
    throwError("Missing main function\n");
  }
  Instr *entryCode = NULL;
  addInstr(&entryCode, OP_CALL)->arg.instr = symMain->fn.instr;
  addInstr(&entryCode, OP_HALT);
  run(entryCode);

  // showDomain(symTable, "Global");
  dropDomain();
  return 0;
}