#include "processor.h"
#include "memory.h"
#include "stdio.h"
#include "unistd.h"
#include "os.h"

/* single CPU register file */
int Register[NP][256]; // per-processor integer register file
int VectorRegister[NP][32][8]; // per-processor vector register file
int PC[NP]; // per-processor program counter
int end_of_simulation[NP]; // per-processor simulation-done flag
int instruction_PC;
int opcode, dest, src1, src2;
int N, Z, C, V;

/* Shared execution log to print the variable values mentioned in processor's 'print' instruction */
static FILE *fd_execution = NULL;

/* Shared per-instruction trace log to print fetch/decode/execute details */
static FILE *fd_trace = NULL;

void processor_init(void){
   if(!fd_execution) fd_execution = fopen("execution.log", "a");
   if(!fd_trace) fd_trace = fopen("trace.log", "a");
}

/* Fetches the next instruction for current memory slot */
void fetch(){
   instruction_PC = PC[0];

   int addr0 = getPhysicalAddress(current_mem_slot, 1, PC[0]);
   int addr1 = getPhysicalAddress(current_mem_slot, 1, PC[0] + 1);
   int addr2 = getPhysicalAddress(current_mem_slot, 1, PC[0] + 2);
   int addr3 = getPhysicalAddress(current_mem_slot, 1, PC[0] + 3);
   
   opcode = memory[addr0];
   dest = memory[addr1];
   src1 = memory[addr2];
   src2 = memory[addr3];

   PC[0] += 4;

   fprintf(fd_trace, "Fetching instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);
   fprintf(fd_trace, "Opcode: %d | ", opcode);
   fprintf(fd_trace, "Destination: %d | ", dest);
   fprintf(fd_trace, "Operand 1: %d | ", src1);
   fprintf(fd_trace, "Operand 2: %d\n", src2);
}

/* Decodes fetched instruction */
void decode(){
   fprintf(fd_trace, "Decoding instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);
}

/* Updates Z N C V flags after addition operation */
void update_add_flags(int operand1, int operand2, int result){
   Z = (result == 0);
   N = ((unsigned int)result >> 31) & 1;

   unsigned int unsigned_operand1 = (unsigned int)operand1;
   unsigned int unsigned_operand2 = (unsigned int)operand2;
   unsigned int unsigned_result = (unsigned int)result;
   C = (unsigned_result < unsigned_operand1) || (unsigned_result < unsigned_operand2);

   V = ((operand1 >= 0 && operand2 >= 0 && result < 0) || (operand1 < 0 && operand2 < 0 && result >= 0));
}

/* Updates Z N C V flags after subtraction operation */
void update_sub_flags(int operand1, int operand2, int result){
    Z = (result == 0);
    N = ((unsigned int)result >> 31) & 1;
    C = ((unsigned int)operand1 > (unsigned int)operand2);
    V = ((operand1 >= 0 && operand2 < 0 && result < 0) || (operand1 < 0 && operand2 >= 0 && result >= 0));
}

int branch_taken(int opcode){
   switch(opcode){
      case BEQ: return Z == 1;
      case BNE: return Z == 0;
      case BCS: return C == 1;
      case BCC: return C == 0;
      case BMI: return N == 1;
      case BPL: return N == 0;
      case BVS: return V == 1;
      case BVC: return V == 0;
      case BHI: return C == 1 && Z == 0;
      case BLS: return C == 0 || Z == 1;
      case BGE: return N == V;
      case BLT: return N != V;
      case BGT: return Z == 0 && N == V;
      case BLE: return Z == 1 || N != V;
      case BAL: return 1;
      default: return 0;
   }
}

/* Executes the decoded instruction for processor id.
   For memory read/write and data-movement instructions, operand1 is 0, and
   the actual register/value/address involved is carried in operand2. 
   This applies to LOAD, STORE, MOV and their _CONST and vector counterparts, 
   and to PRINT. */
void execute(){
   fprintf(fd_trace, "Executing instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);
   switch(opcode){
      case HALT: // opcode 0
         end_of_simulation[0] = 1;
         fprintf(fd_trace, "Program executed successfully.");
         break;


      // addition
      case ADD:{ // opcode 1
         int operand1 = Register[0][src1];
         int operand2 = Register[0][src2];
         int res = operand1 + operand2;
         Register[0][dest] = res;
         update_add_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d + X%d = %d + %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VADD:{ // opcode 33
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = VectorRegister[0][src2][i];
            int res = operand1 + operand2;
            VectorRegister[0][dest][i] = res;
            update_add_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d + V%d%d = %d + %d = %d\n", dest, i, src1, i, src2, i, operand1, operand2, res);
         }
         break;
      }

      case ADD_CONST:{ // opcode 9
         int operand1 = Register[0][src1];
         int operand2 = src2;
         int res = operand1 + operand2;
         Register[0][dest] = res;
         update_add_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d + %d = %d + %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VADD_CONST:{ // opcode 41
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = src2;
            int res = operand1 + operand2;
            VectorRegister[0][dest][i] = res;
            update_add_flags(operand1, operand2, res); // FIX (was update_sub_flags)
            fprintf(fd_trace, "V%d%d <- V%d%d + %d = %d + %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }

      case VADD_REG:{ // opcode 47
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = Register[0][src2];
            int res = operand1 + operand2;
            VectorRegister[0][dest][i] = res;
            update_add_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d + X%d = %d + %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }


      // subtraction
      case SUB:{ // opcode 2
         int operand1 = Register[0][src1];
         int operand2 = Register[0][src2];
         int res = operand1 - operand2;
         Register[0][dest] = res;
         update_sub_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d - X%d = %d - %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VSUB:{ // opcode 34
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = VectorRegister[0][src2][i];
            int res = operand1 - operand2;
            VectorRegister[0][dest][i] = res;
            update_sub_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d - V%d%d = %d - %d = %d\n", dest, i, src1, i, src2, i, operand1, operand2, res);
         }
         break;
      }

      case SUB_CONST:{ // opcode 10
         int operand1 = Register[0][src1];
         int operand2 = src2;
         int res = operand1 - operand2;
         Register[0][dest] = res;
         update_sub_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d - %d = %d - %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VSUB_CONST:{ // opcode 42
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = src2;
            int res = operand1 - operand2;
            VectorRegister[0][dest][i] = res;
            update_sub_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d - %d = %d - %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }

      case VSUB_REG:{ // opcode 48
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = Register[0][src2];
            int res = operand1 - operand2;
            VectorRegister[0][dest][i] = res;
            update_sub_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d - X%d = %d - %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }


      // multiplication
      case MUL:{ // opcode 3
         int operand1 = Register[0][src1];
         int operand2 = Register[0][src2];
         int res = operand1 * operand2;
         Register[0][dest] = res;
         fprintf(fd_trace, "X%d <- X%d * X%d = %d * %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }

      case VMUL:{ // opcode 35
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = VectorRegister[0][src2][i];
            int res = operand1 * operand2;
            VectorRegister[0][dest][i] = res;
            fprintf(fd_trace, "V%d%d <- V%d%d * V%d%d = %d * %d = %d\n", dest, i, src1, i, src2, i, operand1, operand2, res);
         }
         break;
      }

      case MUL_CONST:{ // opcode 11
         int operand1 = Register[0][src1];
         int operand2 = src2;
         int res = operand1 * operand2;
         Register[0][dest] = res;
         fprintf(fd_trace, "X%d <- X%d * %d = %d * %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }

      case VMUL_CONST:{ // opcode 43
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = src2;
            int res = operand1 * operand2;
            VectorRegister[0][dest][i] = res;
            // FIX: removed erroneous update_sub_flags() call - MUL never sets flags
            fprintf(fd_trace, "V%d%d <- V%d%d * %d = %d * %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }

      case VMUL_REG:{ // opcode 49
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[0][src1][i];
            int operand2 = Register[0][src2];
            int res = operand1 * operand2;
            VectorRegister[0][dest][i] = res;
            fprintf(fd_trace, "V%d%d <- V%d%d * X%d = %d * %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }


      // division
      case DIV:{ // opcode 4
         int operand1 = Register[0][src1];
         int operand2 = Register[0][src2];
         if(operand2 != 0){
            int res = operand1 / operand2;
            Register[0][dest] = res;
            fprintf(fd_trace, "X%d <- X%d / X%d = %d / %d = %d\n", dest, src1, src2, operand1, operand2, res);
         } else {
            printf("Division by zero - halting.\n");
            end_of_simulation[0] = 1;
         }
         break;
      }

      case DIV_CONST:{ // opcode 12
         int operand1 = Register[0][src1];
         int operand2 = src2;
         if(operand2 != 0){
            int res = operand1 / operand2;
            Register[0][dest] = res;
            fprintf(fd_trace, "X%d <- X%d / %d = %d / %d = %d\n", dest, src1, src2, operand1, operand2, res);
         } else {
            printf("Division by zero - halting.\n");
            end_of_simulation[0] = 1;
         }
         break;
      }


      // memory read (operand1 unused/0; register or address is in operand2)
      case LOAD:{ // opcode 5
         int addr = Register[0][src2];
         Register[0][dest] = read_word(current_mem_slot, addr);
         fprintf(fd_trace, "X%d <- M[%d] = %d\n", dest, addr, Register[0][dest]);
         break;
      }

      case VLOAD:{ // opcode 37
         int addr = Register[0][src2];
         for(int i = 0; i < 8; i++, addr += 4){
            VectorRegister[0][dest][i] = read_word(current_mem_slot, addr);
            fprintf(fd_trace, "V%d%d <- M[%d] = %d\n", dest, i, addr, VectorRegister[0][dest][i]);
         }
         break;
      }

      case LOAD_CONST:{ // opcode 13
         int addr = src2;
         Register[0][dest] = read_word(current_mem_slot, addr);
         fprintf(fd_trace, "X%d <- M[%d] = %d\n", dest, addr, Register[0][dest]);
         break;
      }

      case VLOAD_CONST:{ // opcode 44
         int addr = src2;
         for(int i = 0; i < 8; i++, addr += 4){
            VectorRegister[0][dest][i] = read_word(current_mem_slot, addr);
            fprintf(fd_trace, "V%d%d <- M[%d] = %d\n", dest, i, addr, VectorRegister[0][dest][i]);
         }
         break;
      }


      // memory write (operand1 unused/0; value register is in operand2)
      case STORE:{ // opcode 6
         int addr = Register[0][dest];
         int val = Register[0][src2];
         write_word(current_mem_slot, addr, val);
         fprintf(fd_trace, "M[%d] <- X%d = %d\n", addr, src2, val);
         break;
      }

      case VSTORE:{ // opcode 38
         int addr = Register[0][dest];
         for(int i = 0; i < 8; i++, addr += 4){
            int val = VectorRegister[0][src2][i];
            write_word(current_mem_slot, addr, val);
            fprintf(fd_trace, "M[%d] <- V%d%d = %d\n", addr, src2, i, val);
         }
         break;
      }

      case STORE_CONST:{ // opcode 14
         int addr = dest;
         int val = Register[0][src2];
         write_word(current_mem_slot, addr, val);
         fprintf(fd_trace, "M[%d] <- X%d = %d\n", addr, src2, val);
         break;
      }

      case VSTORE_CONST:{ // opcode 46
         int addr = dest;
         for(int i = 0; i < 8; i++, addr += 4){
            int val = VectorRegister[0][src2][i];
            write_word(current_mem_slot, addr, val);
            fprintf(fd_trace, "M[%d] <- V%d%d = %d\n", addr, src2, i, val);
         }
         break;
      }


      // data movement
      case MOV: // opcode 7
         Register[0][dest] = Register[0][src2];
         fprintf(fd_trace, "X%d <- X%d = %d\n", dest, src2, Register[0][dest]);
         break;

      case MOV_CONST: // opcode 15
         Register[0][dest] = src2;
         fprintf(fd_trace, "X%d <- %d\n", dest, Register[0][dest]);
         break;


      // print
      case PRINT:{ // opcode 8
         int val = Register[0][src2];
         if(fd_execution){
            fprintf(fd_execution, "Process id: %d  x%d : %d\n", os_current_pid(), src2, val);
            fflush(fd_execution);
         }
         fprintf(fd_trace, "Print x%d = %d (logged for P%d)\n", src2, val, 0);
         break;
      }

      case VPRINT:{ // opcode 32
         if(fd_execution){
            for(int i = 0; i < 8; i++){
               fprintf(fd_execution, "Process id: %d  v%d[%d] : %d\n", os_current_pid(), src2, i, VectorRegister[0][src2][i]);
            }
            fflush(fd_execution);
         }
         fprintf(fd_trace, "Print v%d = [%d %d %d %d %d %d %d %d] (logged for P%d)\n", src2,
            VectorRegister[0][src2][0], VectorRegister[0][src2][1],
            VectorRegister[0][src2][2], VectorRegister[0][src2][3],
            VectorRegister[0][src2][4], VectorRegister[0][src2][5],
            VectorRegister[0][src2][6], VectorRegister[0][src2][7], 0);
         break;
      }


      // branch instructions
      case BEQ: case BNE: case BCS: case BCC: case BMI: case BPL:
      case BVS: case BVC: case BHI: case BLS: case BGE: case BLT:
      case BGT: case BLE: case BAL:{
         if(branch_taken(opcode)){
            /* src2 is the branch relative offset byte (0-255, unsigned) as fetched from memory (0-255,
            unsigned, since memory[] is unsigned char). Reinterpret as signed here, the one place this
            byte means "signed offset" rather than a register/address/constant. */
            signed char signed_offset = (signed char)src2;
            PC[0] = instruction_PC + signed_offset;
            fprintf(fd_trace, "Branch taken: PC <- %d\n", PC[0]);
         } else {
            fprintf(fd_trace, "Branch not taken\n");
         }
         break;
      }

      default:
         printf("Invalid opcode: %d\n", opcode);
         end_of_simulation[0] = 1;
         break;
   }
   fprintf(fd_trace, "\n");
}

void save_context(Context *ctx) {
    memcpy(ctx->reg,  Register[0], sizeof(Register[0]));
    memcpy(ctx->vreg, VectorRegister[0], sizeof(VectorRegister[0]));
    ctx->PC  = PC[0];
    ctx->N = N; ctx->Z = Z; ctx->C = C; ctx->V = V;
    ctx->end_of_simulation = end_of_simulation[0];
}

void load_context(Context *ctx) {
    memcpy(Register[0], ctx->reg, sizeof(Register[0]));
    memcpy(VectorRegister[0], ctx->vreg, sizeof(VectorRegister[0]));
    PC[0] = ctx->PC;
    N = ctx->N; Z = ctx->Z; C = ctx->C; V = ctx->V;
    end_of_simulation[0] = ctx->end_of_simulation;
}

/* Gives processor-id one scheduling quantum… */
void process_instructions(int instruction_count){
   for(int i = 0; i < instruction_count && !end_of_simulation[0]; i++){
      fetch();
      decode();
      execute();
   }
   usleep(10);
}
