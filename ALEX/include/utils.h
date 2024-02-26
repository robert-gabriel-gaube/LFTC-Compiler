// requires at least C11
// in Visual Studio it is set from Properties -> C/C++ -> C Language Standard
#pragma once

#include <stddef.h>
#include <stdnoreturn.h>

// prints to stderr a message prefixed with "error: " and exit the program
// the arguments are the same as for printf
noreturn void throwError(const char *format, ...);

// allocs memory using malloc
// if succeeds, it returns the allocated memory, else it prints an error message
// and exit the program
void *safeAlloc(size_t nBytes);

// loads a text file in a dynamically allocated memory and returns it
// on error, prints a message and exit the program
char *loadFile(const char *fileName);
