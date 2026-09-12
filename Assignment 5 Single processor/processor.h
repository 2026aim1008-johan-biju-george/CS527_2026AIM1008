#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "memory.h"

#define HALT 0x00

// opcodes if second operand is variable
#define ADD 0x01
#define SUB 0x02
#define MUL 0x03
#define DIV 0x04
#define LOAD 0x05
#define STORE 0x06
#define MOV 0x07
#define PRINT 0x08

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

// opcodes for vector instructions
#define VADD 0x21
#define VSUB 0x22
#define VMUL 0x23
#define VLOAD 0x25
#define VSTORE 0x26
#define VADD_CONST 0x29
#define VSUB_CONST 0x2A
#define VMUL_CONST 0x2B
#define VLOAD_CONST 0x2C
#define VSTORE_CONST 0x2E
#define VADD_REG 0x2F
#define VSUB_REG 0x30
#define VMUL_REG 0x31
#define VPRINT 0x20

extern int Register[NP][256];
extern int VectorRegister[NP][32][8];
extern int PC[NP];
extern int end_of_simulation[NP];

typedef struct {
    int reg[256]; // integer registers
    int vreg[32][8]; // vector registers
    int PC; // program counter
    int N, Z, C, V; // condition flags
    int end_of_simulation; // "this task hit HALT"
} Context;

void processor_init(void);

void fetch(void);
void decode(void);
void execute(void);

void save_context(Context *ctx); // CPU → ctx
void load_context(Context *ctx); // ctx → CPU

void process_instructions(int instruction_count);  // no proc_id

#endif
