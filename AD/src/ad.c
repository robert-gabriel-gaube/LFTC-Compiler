#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "ad.h"

Domain *symTable=NULL;

int typeBaseSize(Type *t){
	switch(t->tb){
		case TB_INT:return sizeof(int);
		case TB_DOUBLE:return sizeof(double);
		case TB_CHAR:return sizeof(char);
		case TB_VOID:return 0;
		default:{		// TB_STRUCT
			int size=0;
			for(Symbol *m=t->s->structMembers;m;m=m->next){
				size+=typeSize(&m->type);
				}
			return size;
			}
		}
	}

int typeSize(Type *t){
	if(t->n<0)return typeBaseSize(t);
	if(t->n==0)return sizeof(void*);
	return t->n*typeBaseSize(t);
	}

// free from memory a list of symbols
void freeSymbols(Symbol *list){
	for(Symbol *next;list;list=next){
		next=list->next;
		freeSymbol(list);
		}
	}

Symbol *newSymbol(const char *name,SymKind kind){
	Symbol *s=(Symbol*)safeAlloc(sizeof(Symbol));
	// sets all the fields to 0/NULL
	memset(s,0,sizeof(Symbol));
	s->name=name;
	s->kind=kind;
	return s;
	}

Symbol *dupSymbol(Symbol *symbol){
	Symbol *s=(Symbol*)safeAlloc(sizeof(Symbol));
	*s=*symbol;
	s->next=NULL;
	return s;
	}

// s->next is already NULL from newSymbol
Symbol *addSymbolToList(Symbol **list,Symbol *s){
	Symbol *iter=*list;
	if(iter){
		while(iter->next)iter=iter->next;
		iter->next=s;
		}else{
		*list=s;
		}
	return s;
	}

int symbolsLen(Symbol *list){
	int n=0;
	for(;list;list=list->next)n++;
	return n;
	}

void freeSymbol(Symbol *s){
	switch(s->kind){
		case SK_VAR:
			if(!s->owner)free(s->varMem);
			break;
		case SK_FN:
			freeSymbols(s->fn.params);
			freeSymbols(s->fn.locals);
			break;
		case SK_STRUCT:
			freeSymbols(s->structMembers);
			break;
		}
	free(s);
	}

Domain *pushDomain(){
	Domain *d=(Domain*)safeAlloc(sizeof(Domain));
	d->symbols=NULL;
	d->parent=symTable;
	symTable=d;
	return d;
	}

void dropDomain(){
	Domain *d=symTable;
	symTable=d->parent;
	freeSymbols(d->symbols);
	free(d);
	}

void showNamedType(Type *t,const char *name){
	switch(t->tb){
		case TB_INT:printf("int");break;
		case TB_DOUBLE:printf("double");break;
		case TB_CHAR:printf("char");break;
		case TB_VOID:printf("void");break;
		default:		// TB_STRUCT
			printf("struct %s",t->s->name);
		}
	if(name)printf(" %s",name);
	if(t->n==0)printf("[]");
	else if(t->n>0)printf("[%d]",t->n);
	}

void showSymbol(Symbol *s){
	switch(s->kind){
			case SK_VAR:
				showNamedType(&s->type,s->name);
				if(s->owner){
					printf(";\t// size=%d, idx=%d\n",typeSize(&s->type),s->varIdx);
					}else{
					printf(";\t// size=%d, mem=%p\n",typeSize(&s->type),s->varMem);
					}
				break;
			case SK_PARAM:{
				showNamedType(&s->type,s->name);
				printf(" /*size=%d, idx=%d*/",typeSize(&s->type),s->paramIdx);
				}break;
			case SK_FN:{
				showNamedType(&s->type,s->name);
				printf("(");
				bool next=false;
				for(Symbol *param=s->fn.params;param;param=param->next){
					if(next)printf(", ");
					showSymbol(param);
					next=true;
					}
				printf("){\n");
				for(Symbol *local=s->fn.locals;local;local=local->next){
					printf("\t");
					showSymbol(local);
					}
				printf("\t}\n");
				}break;
			case SK_STRUCT:{
				printf("struct %s{\n",s->name);
				for(Symbol *m=s->structMembers;m;m=m->next){
					printf("\t");
					showSymbol(m);
					}
				printf("\t};\t// size=%d\n",typeSize(&s->type));
				}break;
		}
	}

void showDomain(Domain *d,const char *name){
	printf("// domain: %s\n",name);
	for(Symbol *s=d->symbols;s;s=s->next){
		showSymbol(s);
		}
	puts("\n");
	}

Symbol *findSymbolInDomain(Domain *d,const char *name){
	for(Symbol *s=d->symbols;s;s=s->next){
		if(!strcmp(s->name,name))return s;
		}
	return NULL;
	}

Symbol *findSymbol(const char *name){
	for(Domain *d=symTable;d;d=d->parent){
		Symbol *s=findSymbolInDomain(d,name);
		if(s)return s;
		}
	return NULL;
	}

Symbol *addSymbolToDomain(Domain *d,Symbol *s){
	return addSymbolToList(&d->symbols,s);
	}

Symbol *addExtFn(const char *name,void(*extFnPtr)(),Type ret){
	Symbol *fn=newSymbol(name,SK_FN);
	fn->fn.extFnPtr=extFnPtr;
	fn->type=ret;
	addSymbolToDomain(symTable,fn);
	return fn;
	}

Symbol *addFnParam(Symbol *fn,const char *name,Type type){
	Symbol *param=newSymbol(name,SK_PARAM);
	param->type=type;
	param->paramIdx=symbolsLen(fn->fn.params);
	addSymbolToList(&fn->fn.params,dupSymbol(param));
	return param;
	}
