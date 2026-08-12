#include "processor.h"
#include "memory.h"
#include "stdio.h"

int Register[256]; // register for storing values
int PC; // program counter to store address of next instruction
int instruction_PC; // stores current instruction PC (for branch instructions)
int opcode, dest, src1, src2; // instruction details
int N, Z, C, V; // arithmetic flags
int end_of_simulation = 0; // flag for controlling simulation

/* Resets the processor state by initializing all registers to zero,
   resetting the PC and reactivating the simulation. */
void reset(){
   for(int i = 0; i < 256; i++){
      Register[i] = 0;
   }
   PC = 0;
   N = 0, Z = 0, C = 0, V = 0;
   end_of_simulation = 0;
}

/* Fetches the next instruction from Instruction memory using the
   Program Counter (PC), extracts its opcode and operands, and
   advances the PC to the next instruction. */
void fetch(){
   instruction_PC = PC; // store current address to calculate target address for branch instructions

   opcode = Instruction[PC];
   dest = Instruction[PC+1];
   src1 = Instruction[PC+2];
   src2 = Instruction[PC+3];

   PC += 4;
   
   printf("Fetching instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);
   printf("Opcode: %d | ", opcode);
   printf("Destination: %d | ", dest);
   printf("Operand 1: %d | ", src1);
   printf("Operand 2: %d\n", src2);
}

/* Decodes fetched instruction */
void decode(){
   printf("Decoding instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);
}

/* Updates Z, N, C, V flags after addition operation */
void update_add_flags(int operand1, int operand2, int result){
   // Z - result is zero
   Z = (result == 0);

   // N - result is negative (MSB of result is 1)
   N = ((unsigned int)result >> 31) & 1;

   // C - unsigned result is less than either unsigned input
   unsigned int unsigned_operand1 = (unsigned int)operand1;
   unsigned int unsigned_operand2 = (unsigned int)operand2;
   unsigned int unsigned_result = (unsigned int)result;
   C = (unsigned_result < unsigned_operand1) || (unsigned_result < unsigned_operand2);

   // V - operands have same sign, result has different sign
   V = ((operand1 >= 0 && operand2 >= 0 && result < 0) || (operand1 < 0 && operand2 < 0 && result >= 0));
}

/* Updates Z, N, C, V flags after subtraction operation */
void update_sub_flags(int operand1, int operand2, int result){
    // Z - result is zero
    Z = (result == 0);

    // N - result is negative (MSB of result is 1)
    N = ((unsigned int)result >> 31) & 1;

    // C - operand 1 is greater than operand 2
    C = ((unsigned int)operand1 > (unsigned int)operand2);

    // V - operands have different signs, and result's sign matches second operand's original sign
    V = ((operand1 >= 0 && operand2 < 0 && result < 0) || (operand1 < 0 && operand2 >= 0 && result >= 0));
}

int branch_taken(int opcode){
   switch(opcode){
      case BEQ: // branch if equal
         return Z == 1;

      case BNE: // branch if not equal
         return Z == 0;

      case BCS: // branch if carry set
         return C == 1;

      case BCC: // branch if carry clear
         return C == 0;

      case BMI: // branch if negative
         return N == 1;

      case BPL: // branch if zero or positive
         return N == 0;

      case BVS: // branch if overflow set
         return V == 1;

      case BVC: // branch if overflow clear
         return V == 0;

      case BHI: // branch if carry set and not zero
         return C == 1 && Z == 0;

      case BLS: // branch if carry clear or zero
         return C == 0 || Z == 1;

      case BGE: // branch if greater than or equal
         return N == V;

      case BLT: // branch if less than
         return N != V;

      case BGT: // branch if greater than
         return Z == 0 && N == V;

      case BLE: // branch if less than or equal
         return Z == 1 || N != V;

      case BAL: // branch always
         return 1;

      default: // invalid branch opcode
         return 0;
   }
}

/* Executes the decoded instruction based on its opcode by performing
   arithmetic, memory access, data movement, or program termination. */
void execute(){
   printf("Executing instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);
   switch(opcode){
      // complete or stop execution
      case HALT: // opcode 0
         end_of_simulation = 1;
         printf("Program executed successfully.");
         break;
      
      // addition
      case ADD:{ // opcode 1
         int operand1 = Register[src1];
         int operand2 = Register[src2];
         int res = operand1 + operand2;
         Register[dest] = res;
         update_add_flags(operand1, operand2, res);
         printf("Register[%d] <- Register[%d] + Register[%d] = %d + %d = %d\n", dest, src1, src2, operand1, operand2, res);
         printf("Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case ADD_CONST:{ // opcode 9
         int operand1 = Register[src1];
         int operand2 = src2;
         int res = operand1 + operand2;
         Register[dest] = res;
         update_add_flags(operand1, operand2, res);
         printf("Register[%d] <- Register[%d] + %d = %d + %d = %d\n", dest, src1, src2, operand1, operand2, res);
         printf("Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }
      
      // subtraction
      case SUB:{ // opcode 2
         int operand1 = Register[src1];
         int operand2 = Register[src2];
         int res = operand1 - operand2;
         Register[dest] = res;
         update_sub_flags(operand1, operand2, res);
         printf("Register[%d] <- Register[%d] - Register[%d] = %d - %d = %d\n", dest, src1, src2, operand1, operand2, res);
         printf("Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }

      case SUB_CONST:{ // opcode 10
         int operand1 = Register[src1];
         int operand2 = src2;
         int res = operand1 - operand2;
         Register[dest] = res;
         update_sub_flags(operand1, operand2, res);
         printf("Register[%d] <- Register[%d] - %d = %d - %d = %d\n", dest, src1, src2, operand1, operand2, res);
         printf("Flags: N=%d Z=%d C=%d V=%d\n", N, Z, C, V);
         break;
      }
      
      // multiplication
      case MUL:{ // opcode 3
         int operand1 = Register[src1];
         int operand2 = Register[src2];
         int res = operand1 * operand2;
         Register[dest] = res;
         printf("Register[%d] <- Register[%d] * Register[%d] = %d * %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }

      case MUL_CONST:{ // opcode 11
         int operand1 = Register[src1];
         int operand2 = src2;
         int res = operand1 * operand2;
         Register[dest] = res;
         printf("Register[%d] <- Register[%d] * %d = %d * %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }
      
      // division
      case DIV:{ // opcode 4
         int operand1 = Register[src1];
         int operand2 = Register[src2];
         if(operand2 != 0){
               int res = operand1 / operand2;
               Register[dest] = res;
               printf("Register[%d] <- Register[%d] / Register[%d] = %d / %d = %d\n", dest, src1, src2, operand1, operand2, res);
         }
         else{
               printf("Division by zero is not possible.\n");
               end_of_simulation = 1;
         }
         break;
      }

      case DIV_CONST:{ // opcode 12
         int operand1 = Register[src1];
         int operand2 = src2;
         int res = operand1 / operand2;
         Register[dest] = res;
         printf("Register[%d] <- Register[%d] / %d = %d / %d = %d\n", dest, src1, src2, operand1, operand2, res);
         break;
      }
      
      // memory read
      case LOAD:{ // opcode 5 
         int addr = Register[src1];
         Register[dest] = read_word(addr);
         printf("Register[%d] <- Data[%d] = %d\n", dest, addr, Register[dest]);
         break;
      }

      case LOAD_CONST:{ // opcode 13
         int addr = src1;
         Register[dest] = read_word(addr);
         printf("Register[%d] <- Data[%d] = %d\n", dest, addr, Register[dest]);
         break;
      }

      // memory write
      case STORE:{ // opcode 6
         int addr = Register[dest];
         int val = Register[src1];
         write_word(addr, val);
         printf("Data[%d] <- Register[%d] = %d\n", addr, src1, val);
         break;
      }

      case STORE_CONST:{ // opcode 14
         int addr = dest;
         int val = Register[src1];
         write_word(addr, val);
         printf("Memory[%d] <- Register[%d] = %d\n", addr, src1, val);
         break;
      }
      
      // data movement
      case MOV: // opcode 7
         Register[dest] = Register[src1];
         printf("Register[%d] <- Register[%d] = %d\n", dest, src1, Register[dest]);
         break;

      case MOV_CONST: // opcode 15
         Register[dest] = src1;
         printf("Register[%d] <- %d\n", dest, Register[dest]);
         break;
      
      // branch instructions
      case BEQ: // opcode 16 
      case BNE: // opcode 17 
      case BCS: // opcode 18
      case BCC: // opcode 19
      case BMI: // opcode 20
      case BPL: // opcode 21
      case BVS: // opcode 22
      case BVC: // opcode 23
      case BHI: // opcode 24
      case BLS: // opcode 25
      case BGE: // opcode 26
      case BLT: // opcode 27
      case BGT: // opcode 28
      case BLE: // opcode 29
      case BAL: // opcode 30
      {  
         if(branch_taken(opcode)){
            PC = instruction_PC + src2; // target address = current address + offset
            printf("Branch taken: PC <- %d\n", PC);
         }
         else{
            printf("Branch not taken\n");
         }
         break;
      }

      // invalid opcode
      default:
         printf("Invalid opcode: %d\n", opcode);
         end_of_simulation = 1;
         break; 
   }
   printf("\n");
}