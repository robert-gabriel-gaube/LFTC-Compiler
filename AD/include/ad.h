#pragma once

#include "vm.h"

// the domain analysis

struct Symbol;typedef struct Symbol Symbol;

typedef enum{		// base type
	TB_INT,TB_DOUBLE,TB_CHAR,TB_VOID,TB_STRUCT
	}TypeBase;

typedef struct{		// the type of a symbol
	TypeBase tb;
	Symbol *s;		// for TB_STRUCT, the struct's symbol

	// n - the dimension for an array
	//		n<0 - no array
	//		n==0 - array without specified dimension: int v[]
	//		n>0 - array with specified dimension: double v[10]
	int n;
	}Type;

// returns the size of type t in bytes
int typeSize(Type *t);

typedef enum{		// symbol's kind
	SK_VAR,SK_PARAM,SK_FN,SK_STRUCT
	}SymKind;

struct Symbol{
	const char *name;		// symbol's name. The symbol doesn't own this pointer, but it is allocated somewhere else (ex: in Token)
	SymKind kind;
	Type type;

	// owner:
	//		- NULL for global symbols
	//		- a struct for variables defined in that struct
	//		- a function for parameters/variables local to that function
	Symbol *owner;
	Symbol *next;		// the link to the next symbol in list
	union{		// specific data fo each kind of symbol
		// the index in fn.locals for local vars
		// the index in struct for struct members
		int varIdx;
		// the variable memory for global vars (dynamically allocated)
		void *varMem;
		// the index in fn.params for parameters
		int paramIdx;
		// the members of a struct
		Symbol *structMembers;
		struct{
			Symbol *params;		// the parameters of a function
			Symbol *locals;		// all local vars of a function, including the ones from its inner domains
			void(*extFnPtr)();		// !=NULL for extern functions
			Instr *instr;		// used if extFnPtr==NULL
			}fn;
		};
	};

// dynamically allocation of a new symbol
Symbol *newSymbol(const char *name,SymKind kind);
// duplicates the given symbol
Symbol *dupSymbol(Symbol *symbol);
// adds the symbol the the end of the list
// list - the address of the list where to add the symbol
Symbol *addSymbolToList(Symbol **list,Symbol *s);
// the number of the symbols in list
int symbolsLen(Symbol *list);
// frees the memory of a symbol
void freeSymbol(Symbol *s);

typedef struct _Domain{
	struct _Domain *parent;		// the parent domain
	Symbol *symbols;		// the symbols from this domain (single linked list)
	}Domain;

// the current domain (the top of the domains's stack)
extern Domain *symTable;

// adds a domain to the top of the domains's stack
Domain *pushDomain();
// deletes the domain from the top of the domains's stack
void dropDomain();
// shows the content of the given domain
void showDomain(Domain *d,const char *name);
// search a symbol with the given name in the specified domain and returns it
// if no symbol find, returns NULL
Symbol *findSymbolInDomain(Domain *d,const char *name);
// searches a symbol in all domains, starting with the current one
Symbol *findSymbol(const char *name);
// adds a symbol to the current domain
Symbol *addSymbolToDomain(Domain *d,Symbol *s);

// add in ST an extern function with the given name, address and return type
Symbol *addExtFn(const char *name,void(*extFnPtr)(),Type ret);

// add to fn a parameter with the given name and type
// it doesn't verify for parameter redefinition
// returns the added parameter
Symbol *addFnParam(Symbol *fn,const char *name,Type type);
