#include "memory.h"
#include "stdio.h"

/* Declaring Instruction memory of size 256 bytes and Data memory of size 4096 bytes */
char Instruction[256], Data[4096];

/* Loads the compiled instructions from 'program.byte'
   into Instruction memory and initializes Data memory
   using the contents of 'data.byte' */
void initialize(){
    FILE *iptr = fopen("program.byte", "r"); // instruction file pointer
    int val;
    int i = 0;

    // Loading Instruction memory
    while(i < 256 && fscanf(iptr, "%2x", &val) == 1){
        Instruction[i] = (char)val; // one byte of hexadecimal value is being read
        i++;
    }
    fclose(iptr);

    FILE *dptr = fopen("data.byte", "r"); // data file pointer
    i = 0;

    // Loading Data memory
    while(i < 4096 && fscanf(dptr, "%2x", &val) == 1){
        Data[i] = (char)val; // one byte of hexadecimal value is being read
        i++;
    }
    fclose(dptr);
}

/* Writes the updated contents of Data memory back
   to 'data.byte' after program execution */
void finalize(){
    FILE *dptr = fopen("data.byte", "w"); // data file pointer
    for(int i = 0; i < 4096; i += 4){
        fprintf(dptr, "%02X %02X %02X %02X\n", (unsigned char)Data[i], (unsigned char)Data[i + 1], 
            (unsigned char)Data[i + 2], (unsigned char)Data[i + 3]);
    }
    fclose(dptr);
}

/* Reads one 32-bit word from Data memory */
int read_word(int i){
    // checking for invalid address
    if(i < 0 || i > 4092){
        printf("Memory read error: invalid address %d\n", i);
        return 0;
    }

    unsigned int val;
    val = ((unsigned int)(unsigned char)Data[i] << 24) | ((unsigned int)(unsigned char)Data[i + 1] << 16) |
        ((unsigned int)(unsigned char)Data[i + 2] << 8)  | ((unsigned int)(unsigned char)Data[i + 3]);
    return (int)val;
}

/* Writes one 32-bit word into Data memory */
void write_word(int i, int val){
    // checking for invalid address
    if(i < 0 || i > 4092){
        printf("Memory read error: invalid address %d\n", i);
        return;
    }

    Data[i] = (char)((val >> 24) & 0xFF);
    Data[i + 1] = (char)((val >> 16) & 0xFF);
    Data[i + 2] = (char)((val >> 8) & 0xFF);
    Data[i + 3] = (char)(val & 0xFF);
}