#include "processor.h"
#include "memory.h"
#include "stdio.h"
#include "unistd.h"
#include "devices.h"

/* Per-processor state. 
   opcode/dest/src1/src2/instruction_PC and the N Z C V flags stay as plain scalars 
   since only one processor executes fetch/decode/execute at a time. 
   The scheduler always finishes a processor's timeslice before switching to the next one. */
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

/* Resets processor proc_id: zeroes its registers, resets its PC,
   clears its simulation flag, and ensures the shared logs are open. */
void reset(int proc_id){
   for(int i = 0; i < 256; i++){
      Register[proc_id][i] = 0;
   }
   for(int i = 0; i < 32; i++){
      for(int j = 0; j < 8; j++){
         VectorRegister[proc_id][i][j] = 0;
      }
   }
   PC[proc_id] = 0;
   end_of_simulation[proc_id] = 0;

   if(!fd_execution){
      fd_execution = fopen("execution.log", "a");
   }
   if(!fd_trace){
      fd_trace = fopen("trace.log", "a");
   }
}

/* Fetches the next instruction for processor id */
void fetch(int proc_id){
   instruction_PC = PC[proc_id];

   int addr0 = getPhysicalAddress(proc_id, 1, PC[proc_id]);
   int addr1 = getPhysicalAddress(proc_id, 1, PC[proc_id] + 1);
   int addr2 = getPhysicalAddress(proc_id, 1, PC[proc_id] + 2);
   int addr3 = getPhysicalAddress(proc_id, 1, PC[proc_id] + 3);
   
   opcode = memory[addr0];
   dest = memory[addr1];
   src1 = memory[addr2];
   src2 = memory[addr3];

   PC[proc_id] += 4;

   fprintf(fd_trace, "[P%d] Fetching instruction bytes %d-%d:\n", proc_id, instruction_PC, instruction_PC + 3);
   fprintf(fd_trace, "Opcode: %d | ", opcode);
   fprintf(fd_trace, "Destination: %d | ", dest);
   fprintf(fd_trace, "Operand 1: %d | ", src1);
   fprintf(fd_trace, "Operand 2: %d\n", src2);
}

/* Decodes fetched instruction */
void decode(int proc_id){
   fprintf(fd_trace, "[P%d] Decoding instruction bytes %d-%d:\n", proc_id, instruction_PC, instruction_PC + 3);
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
void execute(int proc_id){
   fprintf(fd_trace, "[P%d] Executing instruction bytes %d-%d:\n", proc_id, instruction_PC, instruction_PC + 3);
   switch(opcode){
      case HALT: // opcode 0
         end_of_simulation[proc_id] = 1;
         fprintf(fd_trace, "Program executed successfully.");
         break;


      // addition
      case ADD:{ // opcode 1
         int operand1 = Register[proc_id][src1];
         int operand2 = Register[proc_id][src2];
         int res = operand1 + operand2;
         Register[proc_id][dest] = res;
         update_add_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d + X%d = %d + %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VADD:{ // opcode 33
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = VectorRegister[proc_id][src2][i];
            int res = operand1 + operand2;
            VectorRegister[proc_id][dest][i] = res;
            update_add_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d + V%d%d = %d + %d = %d\n", dest, i, src1, i, src2, i, operand1, operand2, res);
         }
         break;
      }

      case ADD_CONST:{ // opcode 9
         int operand1 = Register[proc_id][src1];
         int operand2 = src2;
         int res = operand1 + operand2;
         Register[proc_id][dest] = res;
         update_add_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d + %d = %d + %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VADD_CONST:{ // opcode 41
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = src2;
            int res = operand1 + operand2;
            VectorRegister[proc_id][dest][i] = res;
            update_add_flags(operand1, operand2, res); // FIX (was update_sub_flags)
            fprintf(fd_trace, "V%d%d <- V%d%d + %d = %d + %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }

      case VADD_REG:{ // opcode 47
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = Register[proc_id][src2];
            int res = operand1 + operand2;
            VectorRegister[proc_id][dest][i] = res;
            update_add_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d + X%d = %d + %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }


      // subtraction
      case SUB:{ // opcode 2
         int operand1 = Register[proc_id][src1];
         int operand2 = Register[proc_id][src2];
         int res = operand1 - operand2;
         Register[proc_id][dest] = res;
         update_sub_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d - X%d = %d - %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VSUB:{ // opcode 34
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = VectorRegister[proc_id][src2][i];
            int res = operand1 - operand2;
            VectorRegister[proc_id][dest][i] = res;
            update_sub_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d - V%d%d = %d - %d = %d\n", dest, i, src1, i, src2, i, operand1, operand2, res);
         }
         break;
      }

      case SUB_CONST:{ // opcode 10
         int operand1 = Register[proc_id][src1];
         int operand2 = src2;
         int res = operand1 - operand2;
         Register[proc_id][dest] = res;
         update_sub_flags(operand1, operand2, res);
         fprintf(fd_trace, "X%d <- X%d - %d = %d - %d = %d\n", dest, src1, src2, operand1, operand2, res);
         fprintf(fd_trace, "Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case VSUB_CONST:{ // opcode 42
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = src2;
            int res = operand1 - operand2;
            VectorRegister[proc_id][dest][i] = res;
            update_sub_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d - %d = %d - %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }

      case VSUB_REG:{ // opcode 48
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = Register[proc_id][src2];
            int res = operand1 - operand2;
            VectorRegister[proc_id][dest][i] = res;
            update_sub_flags(operand1, operand2, res);
            fprintf(fd_trace, "V%d%d <- V%d%d - X%d = %d - %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }


      // multiplication
      case MUL:{ // opcode 3
         int operand1 = Register[proc_id][src1];
         int operand2 = Register[proc_id][src2];
         int res = operand1 * operand2;
         Register[proc_id][dest] = res;
         fprintf(fd_trace, "X%d <- X%d * X%d = %d * %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }

      case VMUL:{ // opcode 35
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = VectorRegister[proc_id][src2][i];
            int res = operand1 * operand2;
            VectorRegister[proc_id][dest][i] = res;
            fprintf(fd_trace, "V%d%d <- V%d%d * V%d%d = %d * %d = %d\n", dest, i, src1, i, src2, i, operand1, operand2, res);
         }
         break;
      }

      case MUL_CONST:{ // opcode 11
         int operand1 = Register[proc_id][src1];
         int operand2 = src2;
         int res = operand1 * operand2;
         Register[proc_id][dest] = res;
         fprintf(fd_trace, "X%d <- X%d * %d = %d * %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }

      case VMUL_CONST:{ // opcode 43
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = src2;
            int res = operand1 * operand2;
            VectorRegister[proc_id][dest][i] = res;
            // FIX: removed erroneous update_sub_flags() call - MUL never sets flags
            fprintf(fd_trace, "V%d%d <- V%d%d * %d = %d * %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }

      case VMUL_REG:{ // opcode 49
         for(int i = 0; i < 8; i++){
            int operand1 = VectorRegister[proc_id][src1][i];
            int operand2 = Register[proc_id][src2];
            int res = operand1 * operand2;
            VectorRegister[proc_id][dest][i] = res;
            fprintf(fd_trace, "V%d%d <- V%d%d * X%d = %d * %d = %d\n", dest, i, src1, i, src2, operand1, operand2, res);
         }
         break;
      }


      // division
      case DIV:{ // opcode 4
         int operand1 = Register[proc_id][src1];
         int operand2 = Register[proc_id][src2];
         if(operand2 != 0){
            int res = operand1 / operand2;
            Register[proc_id][dest] = res;
            fprintf(fd_trace, "X%d <- X%d / X%d = %d / %d = %d\n", dest, src1, src2, operand1, operand2, res);
         } else {
            printf("[P%d] Division by zero - halting.\n", proc_id);
            end_of_simulation[proc_id] = 1;
         }
         break;
      }

      case DIV_CONST:{ // opcode 12
         int operand1 = Register[proc_id][src1];
         int operand2 = src2;
         if(operand2 != 0){
            int res = operand1 / operand2;
            Register[proc_id][dest] = res;
            fprintf(fd_trace, "X%d <- X%d / %d = %d / %d = %d\n", dest, src1, src2, operand1, operand2, res);
         } else {
            printf("[P%d] Division by zero - halting.\n", proc_id);
            end_of_simulation[proc_id] = 1;
         }
         break;
      }


      // memory read (operand1 unused/0; register or address is in operand2)
      case LOAD:{ // opcode 5
         int addr = Register[proc_id][src2];
         Register[proc_id][dest] = read_word(proc_id, addr);
         fprintf(fd_trace, "X%d <- M[%d] = %d\n", dest, addr, Register[proc_id][dest]);
         break;
      }

      case VLOAD:{ // opcode 37
         int addr = Register[proc_id][src2];
         for(int i = 0; i < 8; i++, addr += 4){
            VectorRegister[proc_id][dest][i] = read_word(proc_id, addr);
            fprintf(fd_trace, "V%d%d <- M[%d] = %d\n", dest, i, addr, VectorRegister[proc_id][dest][i]);
         }
         break;
      }

      case LOAD_CONST:{ // opcode 13
         int addr = src2;
         Register[proc_id][dest] = read_word(proc_id, addr);
         fprintf(fd_trace, "X%d <- M[%d] = %d\n", dest, addr, Register[proc_id][dest]);
         break;
      }

      case VLOAD_CONST:{ // opcode 44
         int addr = src2;
         for(int i = 0; i < 8; i++, addr += 4){
            VectorRegister[proc_id][dest][i] = read_word(proc_id, addr);
            fprintf(fd_trace, "V%d%d <- M[%d] = %d\n", dest, i, addr, VectorRegister[proc_id][dest][i]);
         }
         break;
      }


      // memory write (operand1 unused/0; value register is in operand2)
      case STORE:{ // opcode 6
         int addr = Register[proc_id][dest];
         int val = Register[proc_id][src2];
         write_word(proc_id, addr, val);
         fprintf(fd_trace, "M[%d] <- X%d = %d\n", addr, src2, val);
         break;
      }

      case VSTORE:{ // opcode 38
         int addr = Register[proc_id][dest];
         for(int i = 0; i < 8; i++, addr += 4){
            int val = VectorRegister[proc_id][src2][i];
            write_word(proc_id, addr, val);
            fprintf(fd_trace, "M[%d] <- V%d%d = %d\n", addr, src2, i, val);
         }
         break;
      }

      case STORE_CONST:{ // opcode 14
         int addr = dest;
         int val = Register[proc_id][src2];
         write_word(proc_id, addr, val);
         fprintf(fd_trace, "M[%d] <- X%d = %d\n", addr, src2, val);
         break;
      }

      case VSTORE_CONST:{ // opcode 46
         int addr = dest;
         for(int i = 0; i < 8; i++, addr += 4){
            int val = VectorRegister[proc_id][src2][i];
            write_word(proc_id, addr, val);
            fprintf(fd_trace, "M[%d] <- V%d%d = %d\n", addr, src2, i, val);
         }
         break;
      }


      // data movement
      case MOV: // opcode 7
         Register[proc_id][dest] = Register[proc_id][src2];
         fprintf(fd_trace, "X%d <- X%d = %d\n", dest, src2, Register[proc_id][dest]);
         break;

      case MOV_CONST: // opcode 15
         Register[proc_id][dest] = src2;
         fprintf(fd_trace, "X%d <- %d\n", dest, Register[proc_id][dest]);
         break;


      // print
      case PRINT:{ // opcode 8
         int val = Register[proc_id][src2];
         if(fd_execution){
            fprintf(fd_execution, "Process id: %d  x%d : %d\n", proc_id, src2, val);
            fflush(fd_execution);
         }
         fprintf(fd_trace, "Print x%d = %d (logged for P%d)\n", src2, val, proc_id);
         break;
      }

      case VPRINT:{ // opcode 32
         if(fd_execution){
            for(int i = 0; i < 8; i++){
               fprintf(fd_execution, "Process id: %d  v%d[%d] : %d\n", proc_id, src2, i, VectorRegister[proc_id][src2][i]);
            }
            fflush(fd_execution);
         }
         fprintf(fd_trace, "Print v%d = [%d %d %d %d %d %d %d %d] (logged for P%d)\n", src2,
            VectorRegister[proc_id][src2][0], VectorRegister[proc_id][src2][1],
            VectorRegister[proc_id][src2][2], VectorRegister[proc_id][src2][3],
            VectorRegister[proc_id][src2][4], VectorRegister[proc_id][src2][5],
            VectorRegister[proc_id][src2][6], VectorRegister[proc_id][src2][7], proc_id);
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
            PC[proc_id] = instruction_PC + signed_offset;
            fprintf(fd_trace, "Branch taken: PC <- %d\n", PC[proc_id]);
         } else {
            fprintf(fd_trace, "Branch not taken\n");
         }
         break;
      }

      default:
         printf("[P%d] Invalid opcode: %d\n", proc_id, opcode);
         end_of_simulation[proc_id] = 1;
         break;
   }
   fprintf(fd_trace, "\n");
}

/* Gives processor-id one scheduling quantum up to 'instruction_count'
   fetch/decode/execute cycles, stopping early if the program halts
   in between, and then sleeps briefly so multi-process runs are
   observable in real time. */
void process_instructions(int proc_id, int instruction_count){
   for(int i = 0; i < instruction_count && !end_of_simulation[proc_id]; i++){
      fetch(proc_id);
      decode(proc_id);
      execute(proc_id);
      dev_tick(); /* one simulator tick per retired instruction */
   }
   usleep(10);
}
