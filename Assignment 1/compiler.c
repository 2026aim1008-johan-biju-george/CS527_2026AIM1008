#include "compiler.h"
#include "stdio.h"

/*
 * Reads the source program from 'program.txt',
 * translates each instruction into machine code,
 * and stores it in 'program.byte'.
 */
void compile(){
    FILE *iptr = fopen("program.txt", "r"); // input file pointer
    FILE *optr = fopen("program.byte", "w"); // output file pointer

    char line[100];
    int reg, addr;
    int dest, src1, src2, val;
    
    while(fgets(line, sizeof(line), iptr)){
        // Read op
        if (sscanf(line, "Read x%d, %d", &reg, &addr) == 2) {
            fprintf(optr, "%d %d %d %d\n", READ, reg, addr, 0);
        }
        // Write op
        else if(sscanf(line, "Write x%d, %d", &reg, &addr) == 2) {
            fprintf(optr, "%d %d %d %d\n", WRITE, reg, addr, 0);
        }
        // Add op
        else if(sscanf(line, "x%d = x%d + x%d", &dest, &src1, &src2) == 3) {
            fprintf(optr, "%d %d %d %d\n", ADD, dest, src1, src2);
        }
        // Sub op
        else if(sscanf(line, "x%d = x%d - x%d", &dest, &src1, &src2) == 3) {
            fprintf(optr, "%d %d %d %d\n", SUB, dest, src1, src2);
        }
        // Mul op
        else if(sscanf(line, "x%d = x%d * x%d", &dest, &src1, &src2) == 3) {
            fprintf(optr, "%d %d %d %d\n", MUL, dest, src1, src2);
        }
        // Div op
        else if(sscanf(line, "x%d = x%d / x%d", &dest, &src1, &src2) == 3) {
            fprintf(optr, "%d %d %d %d\n", DIV, dest, src1, src2);
        }
        // Data assignment op
        else if(sscanf(line, "x%d = %d", &dest, &val) == 2) {
            fprintf(optr, "%d %d %d %d\n", MOV, dest, val, 0);
        } 
        else{
            printf("Invalid instruction: %s", line);
        }
    }
    printf("\n");
    fprintf(optr, "%d 0 0 0\n", HALT); // instruction bytecode specifying the end of program
    fclose(iptr);
    fclose(optr);
}