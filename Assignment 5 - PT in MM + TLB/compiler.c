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

/* Per-instruction compile tracing (label/branch/offset details) */
static FILE *fd_trace = NULL;
static void ensure_trace_open(void){
    if(!fd_trace){
        fd_trace = fopen("trace.log", "a");
    }
}

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

    unsigned char encoded_offset = (unsigned char)offset;
    fprintf(fd_trace, "Branch: current = %d, target = %d, offset = %d, encoded offset = %d\n", curr_addr, target_addr, offset, encoded_offset);
    fprintf(optr, "%02X %02X %02X %02X\n", opcode, 0, 0, encoded_offset);
}

/* Reads the assembly source from 'source_filename', translates each
   instruction into machine code, and stores it in 'instruction_filename'.
   Can be called multiple times (once per program the OS loads), so all
   compiler state (labels) is reset at the start of every call. */
void compile(const char *source_filename, const char *instruction_filename){
    label_count = 0; // reset compiler state so this can be reused across programs
    ensure_trace_open();

    // Pass 1 - finding labels
    FILE *iptr = fopen(source_filename, "r");
    if(!iptr){
        printf("Error opening input file %s\n", source_filename);
        return;
    }
    char line[100];
    int curr_addr = 0;

    fprintf(fd_trace, "Compilation - first pass:\n");
    while(fgets(line, sizeof(line), iptr)){
        char* comment = strchr(line, '%');

        // ignore comments and blank spaces
        if(comment){
            *comment = '\0';
        }
        if(line[0] == '\0' || line[0] == '\n') continue;
        
        // check for labels
        if(line[0] == '.'){
            if(sscanf(line, "%s", labels[label_count].name) == 1){
                labels[label_count].address = curr_addr;
                fprintf(fd_trace, "Label found: %s -> address %d\n", labels[label_count].name, curr_addr);
                label_count++;
            }
            continue;
        }
        curr_addr += 4;
    }
    fclose(iptr);

    // Pass 2 - generating machine code
    iptr = fopen(source_filename, "r");
    FILE *optr = fopen(instruction_filename, "w");
    if(!iptr){
        printf("Error opening input file %s\n", source_filename);
        return;
    }
    if(!optr){
        printf("Error opening output file %s\n", instruction_filename);
        return;
    }
    int addr_reg, src_reg;
    int dest, src1, src2, val;
    char label[MAX_LABEL_LENGTH];
    curr_addr = 0;

    fprintf(fd_trace, "\nCompilation - second pass:\n");
    while(fgets(line, sizeof(line), iptr)){
        char* comment = strchr(line, '%');

        // ignore comments and blank spaces
        if(comment){
            *comment = '\0';
        }
        if(line[0] == '\0' || line[0] == '\n') continue;
        if(line[0] == '.'){
            continue;
        }

        // parse instructions
        if(sscanf(line, "Print x%d", &src2) == 1){ // print scalar register
            fprintf(optr, "%02X %02X %02X %02X\n", PRINT, 0, 0, src2);
        }
        else if(sscanf(line, "Print v%d", &src2) == 1){ // print vector register
            fprintf(optr, "%02X %02X %02X %02X\n", VPRINT, 0, 0, src2);
        }
        else if(sscanf(line, "x%d = [x%d]", &dest, &addr_reg) == 2){ // memory read from variable (register) address
            fprintf(optr, "%02X %02X %02X %02X\n", LOAD, dest, 0, addr_reg);
        }
        else if(sscanf(line, "v%d = [x%d]", &dest, &addr_reg) == 2){
            fprintf(optr, "%02X %02X %02X %02X\n", VLOAD, dest, 0, addr_reg);
        }
        else if(sscanf(line, "x%d = [%d]", &dest, &val) == 2){ // memory read from constant address
            fprintf(optr, "%02X %02X %02X %02X\n", LOAD_CONST, dest, 0, val);
        }
        else if(sscanf(line, "v%d = [%d]", &dest, &val) == 2){
            fprintf(optr, "%02X %02X %02X %02X\n", VLOAD_CONST, dest, 0, val);
        }
        else if(sscanf(line, "[x%d] = x%d", &addr_reg, &src_reg) == 2){ // memory write to variable (register) address
            fprintf(optr, "%02X %02X %02X %02X\n", STORE, addr_reg, 0, src_reg);
        }
        else if(sscanf(line, "[x%d] = v%d", &addr_reg, &src_reg) == 2){
            fprintf(optr, "%02X %02X %02X %02X\n", VSTORE, addr_reg, 0, src_reg);
        }
        else if(sscanf(line, "[%d] = x%d", &val, &src_reg) == 2){ // memory write to constant address
            fprintf(optr, "%02X %02X %02X %02X\n", STORE_CONST, val, 0, src_reg);
        }
        else if(sscanf(line, "[%d] = v%d", &val, &src_reg) == 2){
            fprintf(optr, "%02X %02X %02X %02X\n", VSTORE_CONST, val, 0, src_reg);
        }
        else if(sscanf(line, "x%d = x%d + x%d", &dest, &src1, &src2) == 3){ // addition to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", ADD, dest, src1, src2);
        }
        else if(sscanf(line, "v%d = v%d + v%d", &dest, &src1, &src2) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VADD, dest, src1, src2);
        }
        else if(sscanf(line, "v%d = v%d + x%d", &dest, &src1, &src2) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VADD_REG, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d + %d", &dest, &src1, &val) == 3){ // addition to constant
            fprintf(optr, "%02X %02X %02X %02X\n", ADD_CONST, dest, src1, val);
        }
        else if(sscanf(line, "v%d = v%d + %d", &dest, &src1, &val) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VADD_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d - x%d", &dest, &src1, &src2) == 3){ // subtraction to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", SUB, dest, src1, src2);
        }
        else if(sscanf(line, "v%d = v%d - v%d", &dest, &src1, &src2) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VSUB, dest, src1, src2);
        }
        else if(sscanf(line, "v%d = v%d - x%d", &dest, &src1, &src2) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VSUB_REG, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d - %d", &dest, &src1, &val) == 3){ // subtraction to constant
            fprintf(optr, "%02X %02X %02X %02X\n", SUB_CONST, dest, src1, val);
        }
        else if(sscanf(line, "v%d = v%d - %d", &dest, &src1, &val) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VSUB_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d * x%d", &dest, &src1, &src2) == 3){ // multiplication to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", MUL, dest, src1, src2);
        }
        else if(sscanf(line, "v%d = v%d * v%d", &dest, &src1, &src2) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VMUL, dest, src1, src2);
        }
        else if(sscanf(line, "v%d = v%d * x%d", &dest, &src1, &src2) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VMUL_REG, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d * %d", &dest, &src1, &val) == 3){ // multiplication to constant
            fprintf(optr, "%02X %02X %02X %02X\n", MUL_CONST, dest, src1, val);
        }
        else if(sscanf(line, "v%d = v%d * %d", &dest, &src1, &val) == 3){
            fprintf(optr, "%02X %02X %02X %02X\n", VMUL_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d / x%d", &dest, &src1, &src2) == 3){ // division to variable (register)
            fprintf(optr, "%02X %02X %02X %02X\n", DIV, dest, src1, src2);
        }
        else if(sscanf(line, "x%d = x%d / %d", &dest, &src1, &val) == 3){ // division to constant
            fprintf(optr, "%02X %02X %02X %02X\n", DIV_CONST, dest, src1, val);
        }
        else if(sscanf(line, "x%d = x%d", &dest, &src1) == 2){ // register-to-register assignment
            fprintf(optr, "%02X %02X %02X %02X\n", MOV, dest, 0, src1);
        }
        else if(sscanf(line, "x%d = %d", &dest, &val) == 2){ // constant-to-register assignment
            fprintf(optr, "%02X %02X %02X %02X\n", MOV_CONST, dest, 0, val);
        }
        else if(sscanf(line, "BEQ %s", label) == 1){
            generate_branch(optr, label, BEQ, curr_addr);
        }
        else if(sscanf(line, "BNE %s", label) == 1){
            generate_branch(optr, label, BNE, curr_addr);
        }
        else if(sscanf(line, "BCS %s", label) == 1){
            generate_branch(optr, label, BCS, curr_addr);
        }
        else if(sscanf(line, "BCC %s", label) == 1){
            generate_branch(optr, label, BCC, curr_addr);
        }
        else if(sscanf(line, "BMI %s", label) == 1){
            generate_branch(optr, label, BMI, curr_addr);
        }
        else if(sscanf(line, "BPL %s", label) == 1){
            generate_branch(optr, label, BPL, curr_addr);
        }
        else if(sscanf(line, "BVS %s", label) == 1){
            generate_branch(optr, label, BVS, curr_addr);
        }
        else if(sscanf(line, "BVC %s", label) == 1){
            generate_branch(optr, label, BVC, curr_addr);
        }
        else if(sscanf(line, "BHI %s", label) == 1){
            generate_branch(optr, label, BHI, curr_addr);
        }
        else if(sscanf(line, "BLS %s", label) == 1){
            generate_branch(optr, label, BLS, curr_addr);
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