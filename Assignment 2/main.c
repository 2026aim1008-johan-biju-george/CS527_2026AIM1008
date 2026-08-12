#include "compiler.h"
#include "memory.h"
#include "processor.h"
#include "stdio.h"

int main(int argc, char **argv){

    if(argc != 2){
        printf("Pass the input file as argument.\n");
        return 1;
    }

    /* Compiler part */
    compile(argv[1]); // compile all instructions

    /* Memory part */
    initialize(); // initialize both instruction and data memory

    /* Processor part */
    reset(); // reset program counter, end_of_simulation flag, and register values

    while(!end_of_simulation){
        fetch(); // fetch opcode, destination, operand details from instruction memory
        decode(); // decode instruction
        execute(); // do some operations, given the opcode
    }
    
    /* Memory part */
    finalize(); // write back the results into data memory
    
    return 0;
}