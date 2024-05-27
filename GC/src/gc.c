#include "gc.h"

void insertConvIfNeeded(Instr *before,Type *srcType,Type *dstType){
	switch(srcType->tb){
		case TB_INT:
			switch(dstType->tb){
				case TB_DOUBLE:
					insertInstr(before,OP_CONV_I_F);
					break;
				}
			break;
		case TB_DOUBLE:
			switch(dstType->tb){
				case TB_INT:
					insertInstr(before,OP_CONV_F_I);
					break;
				}
			break;
		}
	}

void addRVal(Instr **code,bool lval,Type *type){
	if(!lval)return;
	switch(type->tb){
		case TB_INT:
			addInstr(code,OP_LOAD_I);
			break;
		case TB_DOUBLE:
			addInstr(code,OP_LOAD_F);
			break;
		}
	}
