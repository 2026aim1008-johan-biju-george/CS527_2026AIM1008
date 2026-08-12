#include "processor.h"
#include "memory.h"
#include "stdio.h"

int Register[256]; // register for storing values
int PC; // program counter to store address of next instruction
int opcode, dest, src1, src2; // instruction details
int end_of_simulation = 0; // flag for controlling simulation

/* Resets the processor state by initializing all registers to zero,
   resetting the PC and reactivating the simulation. */
void reset(){
    for(int i = 0; i < 256; i++){
        Register[i] = 0;
    }
    PC = 0;
    end_of_simulation = 0;
}

/* Fetches the next instruction from Instruction memory using the
   Program Counter (PC), extracts its opcode and operands, and
   advances the PC to the next instruction. */
void fetch(){
    opcode = Instruction[PC];
    dest = Instruction[PC+1];
    src1 = Instruction[PC+2];
    src2 = Instruction[PC+3];
    PC += 4;
    
    printf("Fetching instruction bytes %d-%d:\n", PC-4, PC-1);
    printf("Opcode: %d\n", opcode);
    printf("Destination: %d\n", dest);
    printf("Operand 1: %d\n", src1);
    printf("Operand 2: %d\n", src2);
}

// Decodes the fetched instruction
void decode(){
    printf("Decoding instruction bytes %d-%d:\n", PC-4, PC-1);
}

/* Executes the decoded instruction based on its opcode by performing
   arithmetic, memory access, data movement, or program termination. */
void execute(){
    printf("Executing instruction bytes %d-%d:\n", PC-4, PC-1);
    switch(opcode){
        case HALT: // opcode 0
           end_of_simulation = 1;
           printf("Program executed successfully.");
           break;
        case ADD: // opcode 1
           Register[dest] = Register[src1] + Register[src2];
           printf("Register[%d] <- Register[%d] + Register[%d] = %d + %d = %d\n", dest, src1, src2, Register[src1], Register[src2], Register[dest]);
           break;
        case SUB: // opcode 2
           Register[dest] = Register[src1] - Register[src2];
           printf("Register[%d] <- Register[%d] - Register[%d] = %d - %d = %d\n", dest, src1, src2, Register[src1], Register[src2], Register[dest]);
           break;
        case MUL: // opcode 3
           Register[dest] = Register[src1] * Register[src2];
           printf("Register[%d] <- Register[%d] * Register[%d] = %d * %d = %d\n", dest, src1, src2, Register[src1], Register[src2], Register[dest]);
           break;
        case DIV: // opcode 4
           if(Register[src2] != 0){
                Register[dest] = Register[src1] / Register[src2];
                printf("Register[%d] <- Register[%d] / Register[%d] = %d / %d = %d\n", dest, src1, src2, Register[src1], Register[src2], Register[dest]);
           }
           else{
                printf("Division by zero is not possible.\n");
                end_of_simulation = 1;
           }
           break;      
        case READ: // opcode 5 (register value at destination = data value at byte address)
           Register[dest] = Data[src1/4]; // assuming each data value is stored in 32-bits (4 bytes)
           printf("Register[%d] <- %d\n", dest, Data[src1/4]);
           break;
        case WRITE: // opcode 6 (data value at byte address = register value at destination)
           Data[src1/4] = Register[dest]; // assuming each data value is stored in 32-bits (4 bytes)
           printf("Data[%d] <- Register[%d] = %d\n", src1/4, dest, Data[src1/4]);
           break;
        case MOV: // opcode 7 (immediate assignment of value 'src1' to register)
           Register[dest] = src1;
           printf("Register[%d] <- %d\n", dest, src1);
           break;
        default: // invalid opcode
           printf("Invalid opcode: %d\n", opcode);
           end_of_simulation = 1;
           break;
    }
    printf("\n");
}