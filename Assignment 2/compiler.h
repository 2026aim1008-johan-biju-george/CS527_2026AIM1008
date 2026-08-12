#include <stdio.h>
#ifndef COMPILER_H
#define COMPILER_H

#define HALT 0x00

// opcodes if second operand is variable
#define ADD 0x01
#define SUB 0x02
#define MUL 0x03
#define DIV 0x04
#define LOAD 0x05
#define STORE 0x06
#define MOV 0x07

// opcodes if second operand is constant
#define ADD_CONST 0x09
#define SUB_CONST 0x0A
#define MUL_CONST 0x0B
#define DIV_CONST 0x0C
#define LOAD_CONST 0x0D
#define STORE_CONST 0x0E
#define MOV_CONST 0x0F

// branch opcodes
#define BEQ 0x10
#define BNE 0x11
#define BCS 0x12
#define BCC 0x13
#define BMI 0x14
#define BPL 0x15
#define BVS 0x16
#define BVC 0x17
#define BHI 0x18
#define BLS 0x19
#define BGE 0x1A
#define BLT 0x1B
#define BGT 0x1C
#define BLE 0x1D
#define BAL 0x1E

int find_label(char *name);
void generate_branch(FILE *optr, char *label, int opcode, int curr_addr);
void compile(const char *filename);

#endif