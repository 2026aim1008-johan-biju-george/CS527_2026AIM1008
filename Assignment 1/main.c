#include "compiler.h"
#include "memory.h"
#include "processor.h"
#include "stdio.h"

int main(){
    /* Compiler part */
    compile(); // compile all instructions

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