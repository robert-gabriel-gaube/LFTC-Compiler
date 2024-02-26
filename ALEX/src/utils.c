#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void throwError(const char *format, ...) {
  fprintf(stderr, "[ERROR]: ");

  // Print argument list
  va_list va;
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);

  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

void *safeAlloc(size_t nBytes) {
  void *p = malloc(nBytes);
  if (!p) {
    throwError("Not enough memory");
  }
  return p;
}

char *loadFile(const char *fileName) {
  // Opening file
  FILE *fis = fopen(fileName, "rb");
  if (!fis) {
    throwError("Unable to open %s", fileName);
  }

  // Getting file size
  fseek(fis, 0, SEEK_END);
  size_t file_size = (size_t)ftell(fis);
  fseek(fis, 0, SEEK_SET);

  // Read file
  char *buf = (char *)safeAlloc(file_size + 1);
  size_t chars_read = fread(buf, sizeof(char), file_size, fis);

  fclose(fis);
  if (file_size != chars_read) {
    free(buf);
    throwError("Cannot read all the content of %s", fileName);
  }

  buf[file_size] = '\0';
  return buf;
}
