#define MAX_LABELS 100
#define MAX_LABEL_LENGTH 50

#include "compiler.h"
#include <stdio.h>
#include <string.h>

typedef struct{
    char name[MAX_LABEL_LENGTH];
    int address;
} Label;

Label labels[MAX_LABELS];
int label_count = 0;

/* Function to parse the label and return its address */
int find_label(char *name){
    for(int i = 0; i < label_count; i++){
        if(strcmp(labels[i].name, name) == 0){
            return labels[i].address;
        }
    }
    return -1;
}

/* Generates bytecode for branch instruction */
void generate_branch(FILE *optr, char *label, int opcode, int curr_addr){
    int target_addr = find_label(label);
    int offset = target_addr - curr_addr;

    unsigned char encoded_offset = (unsigned char)offset; // for converting to 8-bits representation (in hex format)
    printf("Branch: current = %d, target = %d, offset = %d, encoded offset = %d\n", curr_addr, target_addr, offset, encoded_offset);
    fprintf(optr, "%02X %02X %02X %02X\n", opcode, 0, 0, encoded_offset);
}

/*
 * Reads the source program from 'program.txt',
 * translates each instruction into machine code,
 * and stores it in 'program.byte'.
 */
void compile(const char *filename){
    // Pass 1 - finding labels
    FILE *iptr = fopen(filename, "r"); // input file pointer
    if(!iptr){
        printf("Error opening input file\n");
        return;
    }
    char line[100];
    int curr_addr = 0;

    printf("Compilation - first pass:\n");
    while(fgets(line, sizeof(line), iptr)){
        char* comment = strchr(line, '%');

        // remove comments
        if(comment){
            *comment = '\0';
        }

        // ignore empty lines
        if(line[0] == '\0' || line[0] == '\n') continue;

        // Check whether the line is label or not
        if(line[0] == '.'){
            if(sscanf(line, "%s", labels[label_count].name) == 1){
                labels[label_count].address = curr_addr;
                printf("Label found: %s -> address %d\n", labels[label_count].name, curr_addr);
                label_count++;
            }
            continue;
        }
        curr_addr += 4; // 4 bytes for each non-label instruction
    }
    fclose(iptr);
    
    // Pass 2 - generating machine code
    iptr = fopen(filename, "r");
    FILE *optr = fopen("program.byte", "w");
    if(!iptr){
        printf("Error opening input file\n");
        return;
    }
    if(!optr){
        printf("Error opening output file\n");
        return;
    }
    int addr_reg, src_reg;
    int dest, src1, src2, val;
    char label[MAX_LABEL_LENGTH];
    curr_addr = 0;

    printf("\nCompilation - second pass:\n");
    while(fgets(line, sizeof(line), iptr)){
        char* comment = strchr(line, '%');

        // remove comments
        if(comment){
            *comment = '\0';
        }

        // ignore empty lines
        if(line[0] == '\0' || line[0] == '\n') continue;

        // ignore labels
        if(line[0] == '.'){
            continue;
        }
        
        // generating hexadecimal bytecode for non-branch instructions
        if(sscanf(line, "x%d = [x%d]", &dest, &addr_reg) == 2){ // memory read from variable (register) address
            fprintf(optr, "%02X %02X %02X %02X\n", LOAD, dest, addr_reg, 0);
        }
        else if(sscanf(line, "x%d = [%d]", &dest, &val) == 2){ // memory read from constant address
            fprintf(optr, "%02X %02X %02X %02X\n", LOAD_CONST, dest, val, 0);
        }
        else if(sscanf(line, "[x%d] = x%d", &addr_reg, &src_reg) == 2){ // memory write to variable (register) address
            fprintf(optr, "%02X %02X %02X %02X\n", STORE, addr_reg, src_reg, 0);
        }
        else if(sscanf(line, "[%d] = x%d", &val, &src_reg) == 2){ // memory write to constant address
            fprintf(optr, "%02X %02X %02X %02X\n", STORE_CONST, val, src_reg, 0);
        }
        else if(sscanf(line, "x%d = x%d + x%d", &dest, &src1, &src2) == 3){ // addition to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", ADD, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d + %d", &dest, &src1, &val) == 3){ // addition to constant
            fprintf(optr, "%02X %02X %02X %02X\n", ADD_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d - x%d", &dest, &src1, &src2) == 3){ // subtraction to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", SUB, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d - %d", &dest, &src1, &val) == 3){ // subtraction to constant
            fprintf(optr, "%02X %02X %02X %02X\n", SUB_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d * x%d", &dest, &src1, &src2) == 3){ // multiplication to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", MUL, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d * %d", &dest, &src1, &val) == 3){ // multiplication to constant
            fprintf(optr, "%02X %02X %02X %02X\n", MUL_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d / x%d", &dest, &src1, &src2) == 3){ // division to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", DIV, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d / %d", &dest, &src1, &val) == 3){ // division to constant
            fprintf(optr, "%02X %02X %02X %02X\n", DIV_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d", &dest, &src1) == 2){  // register-to-register assignment
            fprintf(optr, "%02X %02X %02X %02X\n", MOV, dest, src1, 0);
        }
        else if(sscanf(line, "x%d = %d", &dest, &val) == 2){ // constant-to-register assignment
            fprintf(optr, "%02X %02X %02X %02X\n", MOV_CONST, dest, val, 0);
        }

        // generating hexadecimal bytecode for branch instructions
        else if(sscanf(line, "BEQ %s", label) == 1){
            generate_branch(optr, label, BEQ, curr_addr);
        } 
        else if(sscanf(line, "BNE %s", label) == 1){
            generate_branch(optr, label, BNE, curr_addr);
        }
        else if(sscanf(line, "BGT %s", label) == 1){
            generate_branch(optr, label, BGT, curr_addr);
        } 
        else if(sscanf(line, "BLT %s", label) == 1){
            generate_branch(optr, label, BLT, curr_addr);
        } 
        else if(sscanf(line, "BGE %s", label) == 1){
            generate_branch(optr, label, BGE, curr_addr);
        } 
        else if(sscanf(line, "BLE %s", label) == 1){
            generate_branch(optr, label, BLE, curr_addr);
        } 
        else if(sscanf(line, "BAL %s", label) == 1){
            generate_branch(optr, label, BAL, curr_addr);
        } 
        else{
            printf("Invalid instruction: %s", line);
            break;
        }
        curr_addr += 4;
    }
    fprintf(optr, "%02X 00 00 00\n", HALT);
    fclose(iptr);
    fclose(optr);
    printf("Compilation successful.\n\n");
}