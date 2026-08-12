#ifndef PROCESSOR_H
#define PROCESSOR_H

// defining opcodes
#define HALT 0
#define ADD 1
#define SUB 2
#define MUL 3
#define DIV 4
#define READ 5
#define WRITE 6
#define MOV 7

void reset();
void fetch();
void decode();
void execute();

extern int end_of_simulation;

#endif