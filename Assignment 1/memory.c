#include "memory.h"
#include "stdio.h"

/* Declaring Instruction and Data memory of size 256 byte */
unsigned char Instruction[256], Data[256];

/* Loads the compiled instructions from 'program.byte'
   into Instruction memory and initializes Data memory
   using the contents of 'data.byte'. */
void initialize(){
    FILE *iptr = fopen("program.byte", "r"); // instruction file pointer
    int val;
    int i = 0;
    // Populating Instruction memory
    while(i < 256 && fscanf(iptr, "%d", &val) == 1){
        Instruction[i] = val;
        i++;
    }
    fclose(iptr);

    FILE *dptr = fopen("data.byte", "r"); // data file pointer
    i = 0;
    // Populating Data memory
    while(i < 256 && fscanf(dptr, "%d", &val) == 1){
        Data[i] = val;
        i++;
    }
    fclose(dptr);
}

/* Writes the updated contents of Data memory back
   to 'data.byte' after program execution. */
void finalize(){
    FILE *dptr = fopen("data.byte", "w"); // data file pointer
    for(int i = 0; i < 256; i++){
        fprintf(dptr, "%d\n", Data[i]);
    }
    fclose(dptr);
}