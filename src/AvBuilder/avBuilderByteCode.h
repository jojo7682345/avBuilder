#ifndef __AV_BUILDER_BYTE_CODE__
#define __AV_BUILDER_BYTE_CODE__

#include <AvUtils/avTypes.h>

enum Operations {
	OP_NOP = 0b00000, // NOP
	OP_LDA = 0b00001, // LDA [ivalue]   -- LOAD LDA                 -- puts the value into LDA
	OP_LDB = 0b00010, // LDB [ivalue]   -- LOAD LDB                 -- puts the value into LDB
	OP_SWP = 0b00011, // SWP            -- SWAP                     -- swaps LDA and LDB
	OP_SUM = 0b00100, // SUM            -- SUM                      -- LDA = LDA+LDB
	OP_SUB = 0b00101, // SUB            -- SUBTRACT                 -- LDA = LDA-LDB
	OP_MUL = 0b00110, // MUL            -- MULTIPLY                 -- LDA = LDA*LDB
	OP_DIV = 0b00111, // DIV            -- DIVIDE                   -- LDA = LDA/LDB
	OP_POP = 0b01000, // POP            -- POP FROM STACK           -- pops value from top of stack into LDA
	OP_PSH = 0b01001, // PSH            -- PUSH TO STACK            -- puts LDA on the top of the stack
	OP_INC = 0b01010, // INC            -- INCREMENT                -- increments LDA
	OP_DEC = 0b01011, // DEC            -- DECREMENT                -- decrements LDA
	OP_JMP = 0b01100, // JMP            -- JUMP                     -- jump to the instruction in LDA
	OP_BEQ = 0b01101, // BEQ            -- BRANCH IF EQUAL          -- skips the next instruction when LDA == LDB
	OP_BZC = 0b01110, // BZC            -- BRANCH ON ZERO           -- skips the next instruction when LDA is 0
	OB_DRA = 0b01111, // LPA            -- DEREFERENCE LDA          -- puts the value into LDA pointed to by LDA
	OB_DRB = 0b10000, // LPA            -- DEREFERENCE LDB          -- puts the value into LDB pointed to by LDB
	OB_ISP = 0b10001, // ISP            -- INCREMENT STACK POINTER  -- increments the value of the stack pointer by the value of LDA
	OB_DSP = 0b10010, // DSP            -- DECREMENT_STACK POINTER  -- decreases the value of the stack pointer by the value of LDA
	OP_SYS = 0b11111, // SYS [ivalue]   -- SYSCALL                  -- calls a syscall
};

enum Syscalls {
	SYSCALL_EXIT,
	SYSCALL_MALLOC,
};


#endif//__AV_BUILDER_BYTE_CODE__