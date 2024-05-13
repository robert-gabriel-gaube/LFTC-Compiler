#pragma once

// stack based virtual machine

// the instructions of the virtual machine
// FORMAT: OP_<name>.<data_type>    // [argument] effect
//		OP_ - common prefix (operation code)
//		<name> - instruction name
//		<data_type> - if present, the data type on which the instruction acts
//			.i - int
//			.f - double
//			.c - char
//			.p - pointer
//		[argument] - if present, the instruction argument
//		effect - the effect of the instruction
typedef enum{
	OP_HALT	// ends the code execution
	,OP_PUSH_I		// [ct.i] puts on stack the constant ct.i
	,OP_CALL			// [instr] calls a VM function which starts with the given instruction
	,OP_CALL_EXT	// [native_addr] calls a host function (machine code) at the given address
	,OP_ENTER		// [nb_locals] creates a function frame with the given number of local variables
	,OP_RET				// [nb_params] returns from a function which has the given number of parameters and returns a value
	,OP_RET_VOID	// [nb_params] returns from a function which has the given number of parameters without returning a value
	,OP_CONV_I_F	// converts the value from stack from int to double
	,OP_JMP				// [instr] unconditional jump to the specified instruction
	,OP_JF				// [instr] jumps to the specified instruction if the value from stack is false
	,OP_JT				// [instr] jumps to the specified instruction if the value from stack is true
	,OP_FPLOAD		// [idx] puts on stack the value from FP[idx]
	,OP_FPSTORE		// [idx] puts in FP[idx] the value from stack
	,OP_ADD_I			// adds 2 int values from stack and puts the result on stack
	,OP_LESS_I			// compares 2 int values from stack and puts the result on stack as int
	,OP_LESS_D
	,OP_PUSH_D
	,OP_ADD_D
	}Opcode;

typedef struct Instr Instr;

// an universal value - used both as a stack cell and as an instruction argument
typedef union{
	int i;			// int and index values
	double f;		// float values
	void *p;		// pointers
	void(*extFnPtr)();		// pointer to an extern (host) function
	Instr *instr;		// pointer to an instruction
}Val;

// a VM instruction
struct Instr{
	Opcode op;		// opcode: OP_*
	Val arg;
	Instr *next;		// the link to the next instruction in list
};

// adds a new instruction to the end of list and sets its "op" field
// returns the newly added instruction
Instr *addInstr(Instr **list,Opcode op);

// add an instruction which has an argument of type int
Instr *addInstrWithInt(Instr **list,Opcode op,int argVal);

// add an instruction which has an argument of type double
Instr *addInstrWithDouble(Instr **list,Opcode op,double argVal);

// MV initialisation
void vmInit();

// executes the code starting with the given instruction (IP - Instruction Pointer)
void run(Instr *IP);

// generates a test program
Instr *genTestProgram();

// generates a test program
Instr *genTestProgramDouble();